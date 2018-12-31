#include <ACROBOTIC_SSD1306.h>
#include <ESP8266WiFi.h> 

unsigned int ch = 1; 

struct beacon_buf {

  // Radio Information
  signed rssi : 8;      // RSSI
  uint8_t rxctl[11]; // Unimportant radio info

  // 112 Bytes of Frame
  
  // - Header
  uint8_t wifiheader[16];
  uint8_t bssid[6];
  uint8_t fragment[2];
  
  // - Fixed 802.11 Parameters
  uint8_t fixed[12];

  // - 100 bytes of tagged 802.11 Parameters
  // - The SSID, which we are interested in, should be here
  uint8_t tagged[76];

  // Other WiFi Information
  uint16_t cnt;      
  uint16_t len;
  
};

struct ap_info {
  uint8_t bssid[6];
  unsigned char essid[33];
  uint8_t essid_len;
  uint8_t chan;
  signed rssi;
  unsigned long last_seen;
};

// Room for 150 APs
struct ap_info aps[150];
int known_aps = 0;

int rssi_cmp(const void *cmp1, const void *cmp2) {
  struct ap_info ap1 = *((struct ap_info *)cmp1);
  struct ap_info ap2 = *((struct ap_info *)cmp2);

  if(ap2.rssi > ap1.rssi) return 1;
  if(ap1.rssi > ap2.rssi) return -1;
  return 0;
  
}

void add_ap(struct ap_info ap) {

    unsigned long cur_time = millis();
    
    // Check if it is a known ap
    for(int i=0;i<known_aps;i++) {
       if(!memcmp(aps[i].bssid, ap.bssid, 6)) {

          // If it's been less than 100ms, then only update the rssi upward     
          // Otherwise, update it if it has changed.  
          if( ((cur_time - aps[i].last_seen) <= 100 && aps[i].rssi <  ap.rssi) ||
              ((cur_time - aps[i].last_seen) >  100 && aps[i].rssi != ap.rssi) ) {
              
              aps[i].rssi = ap.rssi;
              qsort(aps, known_aps, sizeof(struct ap_info), rssi_cmp);
          } 
          aps[i].last_seen = cur_time;
          return;
        }
    }

    memcpy(&aps[known_aps], &ap, sizeof(ap_info));
    aps[known_aps].last_seen = millis();
    known_aps++;

    // Keep it sorted
    qsort(aps, known_aps, sizeof(struct ap_info), rssi_cmp);
}

struct ap_info parse_tagged(uint8_t *tagged) {
    
    struct ap_info b;
    b.essid_len = 0;
    
    int pos = 0;
    uint8_t len = 0;
    uint8_t tag = 0;
    
    while (pos < 71) {
      tag = tagged[pos];
      len = tagged[pos+1];
      if(len <= 0) return b;
      
      switch(tag) {
        case 0x00:
        
          if(len<32) {
            b.essid_len = len;
            memset(b.essid, 0, 33);
            memcpy(b.essid, &tagged[pos+2], b.essid_len); 
          }
             
          break;
          
        case 0x03:
          b.chan = tagged[pos+2];
          break;
          
        default: break;
      }

      pos += (len + 2);
    }

    return b;
    
}

void promisc_cb(uint8_t *buf, uint16_t len)
{
  if (len == 128) {
    struct beacon_buf *bbuf = (struct beacon_buf*) buf;
    struct ap_info ap = parse_tagged(bbuf->tagged);
    
    if(ap.essid_len>0) {
      ap.rssi = bbuf->rssi;
      memcpy(&ap.bssid, bbuf->bssid, 6);
      add_ap(ap);
    }


  }
}

void setup() { 
 Serial.begin(57600); 
 wifi_set_opmode(STATION_MODE);
 wifi_set_channel(ch); 
 wifi_promiscuous_enable(0); 
 wifi_set_promiscuous_rx_cb(promisc_cb);
 wifi_promiscuous_enable(1); 

 Wire.begin();
 oled.init();
 oled.clearDisplay();
 oled.setFont(font8x8);

} 

void print_ap(struct ap_info ap) {
  //Serial.printf(" [Ch %02d] [%03d] [%s]\n", ap.chan, ap.rssi, ap.essid );
  char line[17];
  snprintf(line, 17, "%16s", ap.essid );
  oled.putString(line);
}

void update_display() {
  char line[17];
    
  snprintf(line, 17, "Ch. %02d Seen %d", ch, known_aps);
  oled.setTextXY(0,0);
  oled.putString("WiFi Scanner");
  oled.setTextXY(1,0);
  oled.putString(line);

  int num_shown = 0;
  unsigned long cur_time = millis();
  for(int i=0;i<known_aps;i++) {
    if(cur_time - aps[i].last_seen < 10000) {
        num_shown++;
        oled.setTextXY(1+num_shown,0);
        print_ap(aps[i]);
    }
    if(num_shown==6) return;
  }
  
}

void loop() { 
  ch = ((ch == 14) ? 1 : ch+1);
  wifi_set_channel(ch);
  update_display();
  delay(300);
} 

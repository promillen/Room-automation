/*
 * ESP8266 Home control webserver
 * Christoffer Kolbe Boye-Hansen
 * * * Version 1: First working version
 * * * Version 2: Removed WakeOnLan
 * * * Version 3: Changed default relay position to off during setup, and fixed on/off confusion
 * * * Version 4: Added power saving mode 
 * * * Version 5: Make debug print statement optional.
 * * * Version 6: Added deep sleep and power saving functionality https://www.esp8266.com/viewtopic.php?p=59566
 * * * Version 7: Removed deep sleep functionality because of bug. Pins go high after deep sleep because of ESP functionality.
 * * * Version 8 (future version): Add daylight saving time functionality
 * * * Version 9:(future version): Add sunrise and -set https://arduinodiy.wordpress.com/2017/03/07/calculating-sunrise-and-sunset-on-arduino-or-other-microcontroller/ or change time according to month
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "Credentials.h"

//#define DEBUG //Comment out for no debugging (serial disabled)
#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.print (x)
 #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
#endif

/*Put your SSID & Password*/
const char* ssid = ssid_from_header_file;           // Enter SSID here
const char* password = password_from_header_file;   // Enter Password here

boolean wifiConnected = false;

ESP8266WebServer server(80);

uint8_t Lys = D5;
uint8_t Computer = D6;
uint8_t Lyd = D7;

//LOW = relæ (slukket)
bool ComputerStatus = LOW;
bool LysStatus = LOW;
bool LydStatus = LOW;
bool MasterStatus = LOW;

//Variabler til hjælp med strømbesparelse
boolean isPowerSavingOn = true;
boolean hasBeenInDeepSleep = false;

const long utcOffsetInSeconds = 3600;
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
const long interval = 36000; /*3600000*/
int dayAtLastReading;
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60     /* Time ESP32 will go to sleep (in seconds) (30 minutes) (1800) */

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", utcOffsetInSeconds);

void setup() {
  #ifdef DEBUG
    Serial.begin(115200);
  #endif

  delay(100);
  pinMode(Computer, OUTPUT);
  pinMode(Lys, OUTPUT);
  pinMode(Lyd, OUTPUT);

  DEBUG_PRINTLN("Connecting to ");
  DEBUG_PRINTLN(ssid);

  if(hasBeenInDeepSleep) {WiFi.forceSleepWake(); hasBeenInDeepSleep = false;}

  //connect to your local wi-fi network
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }
  
  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected..!");
  DEBUG_PRINT("Got IP: ");  DEBUG_PRINTLN(WiFi.localIP());

  server.on("/", handle_OnConnect);
  
  server.on("/computerOff", handle_computerOff);
  server.on("/computerOn", handle_computerOn);

  server.on("/lysOff", handle_lysOff);
  server.on("/lysOn", handle_lysOn);
  
  server.on("/lydOff", handle_lydOff);
  server.on("/lydOn", handle_lydOn);
  
  server.on("/masterOff", handle_masterOff);
  server.on("/masterOn", handle_masterOn);

  server.on("/pwrSaveOff", handle_pwrSaveOff);
  server.on("/pwrSaveOn", handle_pwrSaveOn);
  
  server.onNotFound(handle_NotFound);
  
  server.begin();
  DEBUG_PRINTLN("HTTP server started");

  timeClient.begin();

  dayAtLastReading = timeClient.getDay();
}

void loop() {
  server.handleClient();
  currentMillis = millis();
  
  if(ComputerStatus) 
    {digitalWrite(Computer, LOW);}
    else 
    {digitalWrite(Computer, HIGH);}

  if(LysStatus) 
    {digitalWrite(Lys, LOW);}
    else
    {digitalWrite(Lys, HIGH);}

  if(LydStatus) 
    {digitalWrite(Lyd, LOW);}
    else
    {digitalWrite(Lyd, HIGH);}

  if(currentMillis - previousMillis >= interval) {
    Serial.println("Checking power save");
    timeClient.forceUpdate();
    DEBUG_PRINT("Time is: "); Serial.println(timeClient.getHours());
    DEBUG_PRINT("Day is: "); Serial.println(timeClient.getDay());
    DEBUG_PRINT("Day at last reading is: "); Serial.println(dayAtLastReading);
    
    if(dayAtLastReading != timeClient.getDay()) 
      {handle_pwrSaveOn();}
    if(isPowerSavingOn) 
      {powerSave(timeClient.getHours());}
      
    previousMillis = currentMillis;
    dayAtLastReading = timeClient.getDay();
  }
}

void powerSave(int hour) {
  DEBUG_PRINTLN("Power save method called");
  if(hour > 2 && hour < 7) {
    handle_masterOff();
    DEBUG_PRINTLN("Going to sleep for half an hour");
    //hasBeenInDeepSleep = true;
    //ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    } else {
    handle_lysOn();
  }
}

//Kaldes når enhed går ind på hjemmesiden
void handle_OnConnect() {  
  DEBUG_PRINTLN("Device connected");
  server.send(200, "text/html", SendHTML(ComputerStatus,LysStatus,LydStatus,MasterStatus,isPowerSavingOn));
}

//Relæ til computer
void handle_computerOff() {
  ComputerStatus = LOW;
  
  DEBUG_PRINTLN("Computer Status: OFF");
  server.send(200, "text/html", SendHTML(false,LysStatus,LydStatus,MasterStatus,isPowerSavingOn));
}

void handle_computerOn() {
  ComputerStatus = HIGH;
  MasterStatus = HIGH;
  
  DEBUG_PRINTLN("Computer Status: ON");
  server.send(200, "text/html", SendHTML(true,LysStatus,LydStatus,true,isPowerSavingOn));
}

//Relæ til lys
void handle_lysOff() {
  LysStatus = LOW;
  
  DEBUG_PRINTLN("Light Status: OFF");
  server.send(200, "text/html", SendHTML(ComputerStatus,false,LydStatus,MasterStatus,isPowerSavingOn));
}

void handle_lysOn() {
  LysStatus = HIGH;
  MasterStatus = HIGH;
  
  DEBUG_PRINTLN("Light Status: ON");
  server.send(200, "text/html", SendHTML(ComputerStatus,true,LydStatus,true,isPowerSavingOn));
}

//Relæ til lyd, chromecast og standerlampe
void handle_lydOff() {
  LydStatus = LOW;
  
  DEBUG_PRINTLN("Sound Status: OFF");
  server.send(200, "text/html", SendHTML(ComputerStatus,LysStatus,false,MasterStatus,isPowerSavingOn));
}

void handle_lydOn() {
  LydStatus = HIGH;
  MasterStatus = HIGH;
  
  DEBUG_PRINTLN("Sound Status: ON");
  server.send(200, "text/html", SendHTML(ComputerStatus,LysStatus,true,true,isPowerSavingOn));
}

//Samlet tænd og sluk knap
void handle_masterOff() {
  MasterStatus = LOW;
  ComputerStatus = LOW;
  LysStatus = LOW;
  LydStatus = LOW;
  
  DEBUG_PRINTLN("Master Status: OFF");
  server.send(200, "text/html", SendHTML(false,false,false,false,isPowerSavingOn));
}

void handle_masterOn() {
  MasterStatus = HIGH;
  ComputerStatus = HIGH;
  LysStatus = HIGH;
  LydStatus = HIGH;
  
  DEBUG_PRINTLN("Master Status: ON");
  server.send(200, "text/html", SendHTML(true,true,true,true,isPowerSavingOn));
}

//Power save funktion, til sluk om natten
void handle_pwrSaveOff() {
  isPowerSavingOn = false;
  server.send(200, "text/html", SendHTML(ComputerStatus,LysStatus,LydStatus,MasterStatus,isPowerSavingOn));
}

void handle_pwrSaveOn() {
  isPowerSavingOn = true;
  server.send(200, "text/html", SendHTML(ComputerStatus,LysStatus,LydStatus,MasterStatus,isPowerSavingOn));
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

//HTML for hjemmeside
String SendHTML(uint8_t computerstat,uint8_t lysstat,uint8_t lydstat,uint8_t masterstat,uint8_t pwrsavestat){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<link rel=\"icon\" type=\"image/ico\"href=\"https://besticon-demo.herokuapp.com/icon?url=https://www.proaudio.lu/home/1/en/&size=80..120..200\">\n";
  ptr +="<title>Room Control</title>\n";
  ptr +="<style>html { font-family: Helvetica; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 0;}\n";
  ptr +="p{font-size: 14px;color: #888;margin-bottom: 10px;margin-top:20px;}\n";
  ptr +=".Master p{font-size: 20px;}\n";
  ptr +=".button {background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;display:inline-block;font-size: 25px;cursor: pointer;border-radius: 4px;width: 80px;}\n";
  ptr +=".button-on {background-color: #1abc9c;}\n";
  ptr +=".button-on:active {background-color: #16a085;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +=".button-disabled {background-color: #808080;border: none;color: white;padding: 17px 27px;text-decoration: none;display:inline-block;font-size: 18px;border-radius: 4px;width: 86px;color: #D3D3D3}\n";
  ptr +=".button-special {background-color: #63bbf2;padding:13px 25px; width:90px;}\n";
  ptr +=".buttonSp {background-color: #e3ba17;border: none;color: white;padding: 13px 30px;text-decoration: none;display:inline-block;font-size: 25px;cursor: pointer;border-radius: 4px;width: 80px;}\n";
  ptr +=".buttonSp-on {background-color: #e3ba17;}\n";
  ptr +=".buttonSp-on:active {background-color: #bf9d13;}\n";
  ptr +=".buttonSp-off {background-color: #eb4034;}\n";
  ptr +=".buttonSp-off:active {background-color: #d13d32;}\n";
  ptr +="span {margin: 0;}\n";
  ptr +=".grid-container {display: grid;grid-template-columns: 1fr 150px 10px 150px 1fr;grid-template-rows: 1fr 1fr 1fr 1fr;grid-template-areas: \". Master Master Master .\" \". Computer . ComPWR .\" \". Lys . Lyd .\" \". PWRSaving . . .\";}\n";
  ptr +=".Master { grid-area: Master; }\n";
  ptr +=".PWRSaving { grid-area: PWRSaving; }\n";
  ptr +=".Computer { grid-area: Computer; }\n";
  ptr +=".Lys { grid-area: Lys; }\n";
  ptr +=".Lyd { grid-area: Lyd; }\n";
  ptr +=".ComPWR { grid-area: ComPWR; }\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>Room Control</h1>\n";
  ptr +="<div class=\"grid-container\">\n";

  ptr +="<div class=\"PWRSaving\">\n";
  if(pwrsavestat)
  {ptr +="<p>PWR saving enabled</p><a class=\"buttonSp buttonSp-on\" href=\"/pwrSaveOff\">YES</a>\n";}
  else
  {ptr +="<p>PWR saving enabled</p><a class=\"buttonSp buttonSp-off\" href=\"/pwrSaveOn\">NO</a>\n";}
  ptr +="</div>\n";

  ptr +="<div class=\"Computer\">\n";
  if(computerstat)
  {ptr +="<p>Computer: ON</p><a class=\"button button-on\" href=\"/computerOff\">ON</a>\n";}
  else
  {ptr +="<p>Computer: OFF</p><a class=\"button button-off\" href=\"/computerOn\">OFF</a>\n";}
  ptr +="</div>\n";

  ptr +="<div class=\"Lys\">\n";
  if(lysstat)
  {ptr +="<p>Lys: ON</p><a class=\"button button-on\" href=\"/lysOff\">ON</a>\n";}
  else
  {ptr +="<p>Lys: OFF</p><a class=\"button button-off\" href=\"/lysOn\">OFF</a>\n";}
  ptr +="</div>\n";

  ptr +="<div class=\"Lyd\">\n";
  if(lydstat)
  {ptr +="<p>Lyd: ON</p><a class=\"button button-on\" href=\"/lydOff\">ON</a>\n";}
  else
  {ptr +="<p>Lyd: OFF</p><a class=\"button button-off\" href=\"/lydOn\">OFF</a>\n";}
  ptr +="</div>\n";

  ptr +="<div class=\"ComPWR\">\n";
  {ptr +="<p>Computer PWR</p><a class=\"button button-special\">POWER</a>\n";}
  ptr +="</div>\n";

  ptr +="<div class=\"Master\">\n";
  if(masterstat)
  {ptr +="<p>Master Status: ON</p><a class=\"button button-on\" href=\"/masterOff\">ON</a>\n";}
  else
  {ptr +="<p>Master Status: OFF</p><a class=\"button button-off\" href=\"/masterOn\">OFF</a>\n";}
  ptr +="</div>\n";

  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

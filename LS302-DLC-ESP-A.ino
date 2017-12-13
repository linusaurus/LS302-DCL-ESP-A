/*
MQTT client LIFT_SLIDE Motor Controller
r.young 
11.27.2017,12-12-2017**
LS302-DLC-ESP-A
*/
#include <ArduinoJson.h>
#include <Encoder.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>

#define DEBUG 0
#define ENCODER_OUTPUT 0

// Use this ON 157 7199 Only- Door 1 LS301-DCL-01 (5,4)
// Use this ON 157 1175 Onlt- Do0r 2  LS301-DCL-02 (4,5)
Encoder encoder(4,5);// Need to be changed 5,4 to this for the 01 door??
// WiFi Connections-----------------------------
const char* ssid = "MonkeyRanch";
const char* password = "12345678";
const char* mqtt_server = "192.168.10.92";
//----------------------------------------------
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

int PWMpin = 15;
int DIRpin = 12;
int Switch_LO = 13;

byte MotorState= 0;  // 0=STOP; 1=RAISE ; 2=LOWER

long HI_LIMIT = 68000;
long LO_LIMIT = 0;
unsigned long interval = 20000;
unsigned long currentMillis;
unsigned long previousMillis = 0; 

bool CalibrateState= false;
bool LastCalibrateState = false;;
// Encoder Values ---------------------
long newPosition;
long oldPosition;
//-------------------------------------
byte newDoorPosition;
byte oldDoorPosition;
// EEPROM Values ----------------------
long LastEncoderValue;
byte LastDoorPosition;
byte LastMotorState; // Last know saved to EEPROM
byte LastCommandActive; // last command
//-------------------------------------

byte ButtonState_HI = HIGH;

const char* MQTT_CLIENT_ID = "LS301DLC-01";
const char* outTopic = "STATUS";
const char* inTopic = "DLIN";


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  #ifdef DEBUG
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {   
    for(int i = 0; i<500; i++){     
      delay(1);
    }
    #ifdef DEBUG
    Serial.print(".");
    #endif
  }
  #ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
 
  #endif
}

void callback(char* topic, byte* payload, unsigned int length) {
  #ifdef DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] "); 
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  #endif
  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '0') {
    MotorState = 0;  // Stop Motor
    #ifdef DEBUG
    Serial.println("Motor State -> 0");
    #endif
    
  } else if ((char)payload[0] == '1') {    
    MotorState = 1;  // Raise Doors
    LastCommandActive = 1;
    EEPROM.write(2,LastCommandActive);
    EEPROM.commit();
    #ifdef DEBUG
    Serial.println("Motor State -> 1");
    #endif
   
  } else if ((char)payload[0] == '2') {
    MotorState = 2;  //Lower Doors
    LastCommandActive = 2;
    EEPROM.write(2,LastCommandActive);
    EEPROM.commit();
    #ifdef DEBUG
    Serial.print("Motor State -> 2 ");
    #endif
       
  }
}

void reconnect() {
  //Tag the MCU with the ESO8266 chip id
  // Loop until we're reconnected
  char CHIP_ID[8];
  String(ESP.getChipId()).toCharArray(CHIP_ID,8);
  
  while (!client.connected()) {
    #ifdef DEBUG
    Serial.print("Attempting MQTT connection...");
    #endif
    // Attempt to connect
    if (client.connect(CHIP_ID)) {
      #ifdef DEBUG
      Serial.println("connected");
      #endif
      // Once connected, publish an announcement...
      client.publish(outTopic, CHIP_ID );
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      #ifdef DEBUG
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      #endif
      // Wait 5 seconds before retrying
      for(int i = 0; i<5000; i++){
        
        delay(1);
      }
    }
  }
}


void OnPositionChanged(){
  // check for value change
   if(newDoorPosition != oldDoorPosition){
      if(newDoorPosition==1){
      client.publish(outTopic, "1");
          EEPROMWritelong(4,newPosition);
          EEPROM.write(0,newDoorPosition);
          EEPROM.write(1,LastMotorState);
          EEPROM.commit();
      #ifdef DEBUG
      Serial.println("-> Send 1");
      #endif
        }
      if(newDoorPosition==2){
        client.publish(outTopic, "2");
          EEPROMWritelong(4,newPosition);
          EEPROM.write(0,newDoorPosition);
          EEPROM.write(1,LastMotorState);
          EEPROM.commit();
        #ifdef DEBUG
        Serial.println("-> Send 2");
        #endif
        }
              
      oldDoorPosition = newDoorPosition; 
   }
}

void setup() {
  
  EEPROM.begin(512);              // Begin eeprom to store on/off state
  pinMode(13,OUTPUT);
  pinMode(12,OUTPUT);
  pinMode(PWMpin,OUTPUT);
  pinMode(DIRpin,OUTPUT);
  pinMode(Switch_LO,INPUT_PULLUP);
  digitalWrite(PWMpin,LOW);

  delay(500);
  #ifdef DEBUG
  Serial.begin(115200);
  #endif
  setup_wifi();                   // Connect to wifi   
  //-----------------------------------
  encoder.write(EEPROMReadlong(4));
  LastDoorPosition = EEPROM.read(0);
  LastMotorState  = EEPROM.read(1);
  Serial.println("");
  Serial.print("Last Encoder Position : ");
  Serial.println(EEPROMReadlong(4));
  Serial.print("Last Door Position : ");
  Serial.println(LastDoorPosition);
  Serial.print("Last Command : ");
  Serial.println(LastCommandActive);
  Serial.print("Last Motor State : ");
  Serial.println(LastMotorState);
 //------------------------------------
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
   
  newPosition = encoder.read();
  if (newPosition != oldPosition) {
    oldPosition = newPosition;
    #ifdef ENCODER_OUTPUT
    Serial.println(newPosition);

    #endif
  }

  currentMillis = millis();
  
/// ---------------------Switches----------------------------------------------------------
  
   if(digitalRead(Switch_LO)==LOW)
   {CalibrateState=true; encoder.write(0);EEPROMWritelong(4,0);}
   else if(digitalRead(Switch_LO)==HIGH)
   {CalibrateState=false; }
 
/// ---------------------------------------------------------------------------------------
 
/// ---------------------State Table-----------------------------------------
  if(newPosition < HI_LIMIT && MotorState==1){Raise();}
  if(newPosition >= HI_LIMIT && MotorState==1){
    newDoorPosition=1;//Door is Raised
    MotorState=0;

    }
  
  if(newPosition > LO_LIMIT && MotorState==2){Lower();}
  if(newPosition <= LO_LIMIT && MotorState==2){
    newDoorPosition=2;//Door us Lowered
    MotorState=0;
    
    }
    
  if(MotorState==0){Stop();} 

  
///----------------------END-LOOP------------------------

   OnPositionChanged();
   OnSwitchChanged();
   OnPollingCycle();
   
 
}

//------------------------------------------------
////////////////// Motor Functions /////////////////////
//------------------------------------------------

void Stop(){
  digitalWrite(DIRpin,LOW);
  digitalWrite(PWMpin, LOW);
}
void Raise(){
  digitalWrite(DIRpin,HIGH);
  digitalWrite(PWMpin, HIGH);
}
void Lower(){
  digitalWrite(DIRpin,LOW);
  digitalWrite(PWMpin, HIGH);
}

void OnPollingCycle(){

  if(currentMillis - previousMillis > interval) {
   
    previousMillis = currentMillis;    
    Serial.println("Status...");
    Serial.println("-------------------------");
    Serial.print("Last Encoder Position : ");
    Serial.println(EEPROMReadlong(4));
    Serial.print("Last Door Position : ");
    Serial.println(LastDoorPosition);
    Serial.print("Last Command : ");
    Serial.println(EEPROM.read(2));
    Serial.print("Last Motor State : ");
    Serial.println(LastMotorState);
  }
  
}

void OnSwitchChanged(){
  if(CalibrateState != LastCalibrateState)
     
    if(CalibrateState == true){
      Serial.println("Calibration Active!");
      
      }
    else if (CalibrateState ==false){
      Serial.println("Calibrate Stopped");
      encoder.write(0);
      }
    LastCalibrateState = CalibrateState;
}

//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to address + 3.
void EEPROMWritelong(int address, long value)
      {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);
//Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
      }
//This function will return a 4 byte (32bit) long from the eeprom
//at the specified address to address + 3.
long EEPROMReadlong(long address)
      {
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);
//Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + 
      ((two << 16) & 0xFFFFFF) + 
      ((one << 24) & 0xFFFFFFFF);
      }


  




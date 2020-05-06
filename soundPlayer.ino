
#include "SeeedGroveMP3.h"
#include "KT403A_Player.h"
#include "WT2003S_Player.h"
#include "wiring_private.h"
#include "EventManager.h"
#include "Ultrasonic.h"


//---------------------------------
//      Defintions & Variables     
//---------------------------------
#define COMSerial mySerial // the serial port used for UART communication with the mp3 player
#define POT_PIN A0
#define SENSOR_PIN 4 
#define BUTTON_PIN 2

MP3Player<KT403A<Uart>> Mp3Player; // mp3 player object, v2.0
Ultrasonic sensor(SENSOR_PIN);

int adcIn = 0;   // variable to store the value coming from the sensor
static int potThresh = 5;

// thresholds for each level of swiping 
static int bottomThresh = 10; 
static int middleThresh = 18;
static int topThresh = 40;

// These lines configure the Arduino to use pins D6 and D7 as a serial port
Uart mySerial (&sercom0, 6, 7, SERCOM_RX_PAD_0, UART_TX_PAD_2);
void SERCOM0_Handler() 
{
    mySerial.IrqHandler();
}


//---------------------------------
//     Event & State Variables     
//---------------------------------
EventManager eventManager;
#define EVENT_BUTTONDOWN EventManager::kEventUser0
#define EVENT_POTCHANGE EventManager::kEventUser1
#define EVENT_PAUSEULTRA EventManager::kEventUser2
#define EVENT_PREVULTRA EventManager::kEventUser3
#define EVENT_NEXTULTRA EventManager::kEventUser4

// States for the state machine
enum SystemState_t {STATE_CHANGE_SONG, STATE_PAUSE, STATE_INIT, STATE_PLAY};
SystemState_t currentState = STATE_INIT;


//---------------------------------
//              Setup  
//---------------------------------
void setup() {
    
    // Configure pins 6/7 as serial
    pinPeripheral(6, PIO_SERCOM_ALT); 
    pinPeripheral(7, PIO_SERCOM_ALT);

    Serial.begin(9600);
    COMSerial.begin(9600); // initialize serial port
    while (!Serial);
    while (!COMSerial); // wait for serial to connect
    
    // Initialize state machine as a listener for events
    eventManager.addListener(EVENT_BUTTONDOWN, MUSIC_SM);
    eventManager.addListener(EVENT_POTCHANGE, MUSIC_SM);
    eventManager.addListener(EVENT_NEXTULTRA, MUSIC_SM);
    eventManager.addListener(EVENT_PREVULTRA, MUSIC_SM);
    eventManager.addListener(EVENT_PAUSEULTRA, MUSIC_SM);


    // Initialize LED state machine
    MUSIC_SM(STATE_INIT,0);
}

//---------------------------------
//           Main Loop  
//---------------------------------
void loop() {

    // Handle any events that are in the queue
    eventManager.processEvent();
    buttonPress();
    potChange();
    ultraChange();
}


//---------------------------------
//        Event Checkers  
//---------------------------------
/* Checks if an event has happened, and posts them 
 *  (and any corresponding parameters) to the event 
 * queue, to be handled by the state machine. */

void buttonPress() {
    bool eventHappened = false; // Initialize eventHappened flag to false
    int eventParameter = 0;

    // Create variable to hold last button pin reading
    // note: static variables are only initialized once and keep their value between function calls.
    static int lastButtonPinReading = LOW;

    // Read value of button pin
    int thisButtonPinReading = digitalRead(BUTTON_PIN);

    // Check if button was pressed
    if (thisButtonPinReading != lastButtonPinReading) { 
      if (thisButtonPinReading == 1) {
        // Set flag so event will be posted
        eventHappened = true;
      }
    }  

    // Post the event if it occured
    if (eventHappened == true) {
      eventManager.queueEvent(EVENT_BUTTONDOWN, eventParameter);
    }
  
   // Update last button reading to current reading
   lastButtonPinReading = thisButtonPinReading;
   
}


void potChange() {
    bool eventHappened = false; // Initialize eventHappened flag to false
    int eventParameter = 0;

    // Create variable to hold last pot pin reading
    static int lastPotPinReading = adcIn;

    // Read value of pot pin
    int thisPotPinReading = analogRead(POT_PIN);

    // Check pot reading changed
    if (abs(thisPotPinReading - lastPotPinReading) > potThresh) { 
        eventParameter = map(thisPotPinReading, 0, 1024, 0, 30);
        
        // Set flag so event will be posted
        eventHappened = true;

        // Update last pot reading to current reading 
        lastPotPinReading = thisPotPinReading;
    }

    // Post the event if it occured
    if (eventHappened == true) {
      eventManager.queueEvent(EVENT_POTCHANGE, eventParameter);
    }   
}

void ultraChange() {
    bool nextEvent = false; // Initialize our eventHappened flag to false
    bool prevEvent = false;
    bool pauseEvent = false;
    int eventParameter = 0;

    // Read value of ultrasonic sensor
    int thisUltrasonicReading = sensor.MeasureInCentimeters();

    // Check ultrasonic reading
    if (thisUltrasonicReading < bottomThresh) {
      prevEvent = true;
    } else if (thisUltrasonicReading < middleThresh) {
      nextEvent = true;
    } else if (thisUltrasonicReading < topThresh) {
      pauseEvent = true;
    }

    // Post the event if it occured
    if (pauseEvent == true) {
      eventManager.queueEvent(EVENT_PAUSEULTRA, eventParameter);
    } else if (prevEvent == true) {
      eventManager.queueEvent(EVENT_PREVULTRA, eventParameter);
    } else if (nextEvent == true) {
      eventManager.queueEvent(EVENT_NEXTULTRA, eventParameter);
    } 

}

//---------------------------------
//           State Machine  
//---------------------------------
/* Responds to events based on the current state. */

void MUSIC_SM( int event, int param) {
    //Initialize next state
    SystemState_t nextState = currentState;

    switch (currentState) { 
        case STATE_INIT:

            // Initialize things 
            pinMode(BUTTON_PIN, INPUT);
            pinMode(POT_PIN, INPUT);
            Mp3Player.controller->init(COMSerial); // initialize the MP3 player

            // Start with song on!
            Mp3Player.controller->playSongMP3(1); 
            Mp3Player.controller->pause_or_play();
         
            nextState = STATE_PLAY;
            break;
            
        case STATE_PLAY:
        
           // Pause by pressing button or swiping top of device
            if (event == EVENT_BUTTONDOWN) {
              Serial.println("pause song");
              Mp3Player.controller->pause_or_play();
              nextState = STATE_PAUSE;
            }
           if (event == EVENT_PAUSEULTRA) {
              Serial.println("pause song");
              Mp3Player.controller->pause_or_play();
              nextState = STATE_PAUSE;
            }  
            
           // Next song by swiping middle of device
            if (event == EVENT_NEXTULTRA) {
              Serial.println("next song");
              Mp3Player.controller->next();
              nextState = STATE_PLAY;             
            } 

           // Previous song by swiping bottom of device
            if (event == EVENT_PREVULTRA) {
              Serial.println("previous song");
              Mp3Player.controller->previous();
              nextState = STATE_PLAY;             
            } 

           // Volume level by changing potentiometer
            if (event == EVENT_POTCHANGE) {
              //change volume
              Mp3Player.controller->volume(param);
              nextState = STATE_PLAY;             
            }     
            break;
            
        case STATE_PAUSE:
        
           // Play by pressing button or swiping top of device
           if (event == EVENT_BUTTONDOWN) {
              Mp3Player.controller->pause_or_play();
              nextState = STATE_PLAY;
           }
           if (event == EVENT_PAUSEULTRA) {
              Mp3Player.controller->pause_or_play();
              nextState = STATE_PLAY;
           }
            break;
            
        default:
            Serial.println("STATE: Unknown State");
            break;
      }
       currentState = nextState;
}


/*
>> Pulse Sensor Amped <<
 This code is for Pulse Sensor Amped by Joel Murphy and Yury Gitman
 www.pulsesensor.com 
 >>> Pulse Sensor purple wire goes to Analog Pin 0 <<<
 Pulse Sensor sample aquisition and processing happens in the background via Timer 1 interrupt. 1mS sample rate.
 PWM on pins 9 and 10 will not work when using this code!
 The following variables are automatically updated:
 Pulse :     boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.
 Signal :    int that holds the analog signal data straight from the sensor. updated every 1mS.
 HRV  :      int that holds the time between the last two beats. 1mS resolution.
 BPM  :      int that holds the heart rate value. derived every pulse from averaging previous 10 HRV values.
 QS  :       boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
 
 This code is designed with output serial data to Processing sketch "PulseSensorAmped_Processing-xx"
 The Processing sketch is a simple data visualizer. 
 All the work to find the heartbeat and determine the heartrate happens in the code below.
 Pin 13 LED will blink with heartbeat.
 It will also fade an LED on pin 11 with every beat. Put an LED and series resistor from pin 11 to GND
 
 See the README for more information and known issues.
 Code Version 0.1 by Joel Murphy & Yury Gitman  Summer 2012     
 */

//  VARIABLES
int pulsePin = 0;          // pulse sensor purple wire connected to analog pin 0
int fadeRate = 0;          // used to fade LED on PWM pin 11
#define LEDR_PIN 3
#define LEDG_PIN 5
#define LEDB_PIN 6
#define NUM_RGB_COLORS 511 // the total number of possible RGB combos fading from r -> g -> b with constant power
#define RED 0
#define GREEN 1
#define BLUE 2
unsigned char rgbColorMap[NUM_RGB_COLORS][3];
#define HRV_HISTORY_LENGTH 15  // how many pulses to keep track of
int hrvHistory[HRV_HISTORY_LENGTH];  // maintains a history the bpm as calculated based on the hrv for each beat
// these are volatile because they are used during the interrupt!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int HRV;                   // holds the time between beats
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when pulse rate is determined. every 20 pulses
volatile unsigned int sampleCounter = 0;  // counts time since last pulse in ms


void setup(){
  // pinMode(13,OUTPUT);    // pin 13 will blink to your heartbeat!
  // pinMode(11,OUTPUT);    // pin 11 will fade to your heartbeat!
  pinMode(LEDR_PIN, OUTPUT);
  pinMode(LEDG_PIN, OUTPUT);
  pinMode(LEDB_PIN, OUTPUT);
  Serial.begin(115200);  // we agree to talk fast!
  interruptSetup();      // sets up to read Pulse Sensor signal every 1mS 
  // UN-COMMENT THE NEXT LINE IF YOU ARE POWERING THE PulseSensor AT LOW VOLTAGE, 
  // AND APPLY THAT VOLTAGE TO THE A-REF PIN
  //analogReference(EXTERNAL);   

  // seed hrv history with a reasonable bpm so we can start smoothly
  for (int i = 0; i < HRV_HISTORY_LENGTH; i++) {
    hrvHistory[i] = 60;
  }
  // populate the color map such that we fade evenly from R -> G -> B
  // at any index r + g + b = 255
  // max brightness (based on circuit) is 0 (not 255) for any given LED
  for (int i = 0; i < 256; i++) {
    rgbColorMap[i][RED]       = i;
    rgbColorMap[i][GREEN]     = 255-i;
    rgbColorMap[i][BLUE]      = 255;
    rgbColorMap[i+255][RED]   = 255;
    rgbColorMap[i+255][GREEN] = i;
    rgbColorMap[i+255][BLUE]  = 255-i;
  }

}

void loop(){
  // sendDataToProcessing('S', Signal);   // send Processing the raw Pulse Sensor data

  if (QS == true){                     // Quantified Self flag is true when arduino finds a heartbeat
    fadeRate = 255;                    // Set 'fadeRate' Variable to 255 to fade LED with pulse
    sendDataToProcessing('B',BPM);     // send the time between beats with a 'B' prefix
    sendDataToProcessing('Q',HRV);     // send heart rate with a 'Q' prefix
    fadeHrvLed();                      // ensure that fadeHrvLed() gets called before QS is reset (it can get set (interrupt) while function is runnin

    // and then reset without the function having a chance to see it as true.
    QS = false;                        // reset the Quantified Self flag for next time   
  }
  fadeHrvLed(); // requires that QS flag not be reset yet.
  delay(10);                          //  take a break
}

int hrvHistoryMin = 50; 
int hrvHistoryMax = 90;
void fadeHrvLed() {
  /* if we have a new HRV value */
  if(QS == true) {
    // seed hrvHistory vars with unrealistically high and low numbers
    hrvHistoryMin = 200; 
    hrvHistoryMax = 0;
    // shift history buffer over, making room for new value
    for (int i = 0; i < HRV_HISTORY_LENGTH-1; i++) {
      hrvHistory[i] = hrvHistory[i+1];
    }

    // calculate the new hrv value and append it to the history buffer
    int bpmFromHrv = 60000/HRV;
    bpmFromHrv = constrain(bpmFromHrv, 30, 100); // constrain bpm to reasonable values
    hrvHistory[HRV_HISTORY_LENGTH-1] = bpmFromHrv; // add the new HRV value as the BPM for the last two beats 

    // calculate the new min and max values now that the new hrv value is appended
    for (int i = 0; i < HRV_HISTORY_LENGTH; i++) {
      hrvHistoryMin = min(hrvHistoryMin, hrvHistory[i]);
      hrvHistoryMax = max(hrvHistoryMax, hrvHistory[i]);
    }
  }

  int lastRgbIndex = int(map(hrvHistory[HRV_HISTORY_LENGTH-2], hrvHistoryMin, hrvHistoryMax, 0, NUM_RGB_COLORS-1)); // This is the color we are fading from
  lastRgbIndex = constrain(lastRgbIndex, 0, NUM_RGB_COLORS-1);
  int nextRgbIndex = int(map(hrvHistory[HRV_HISTORY_LENGTH-1], hrvHistoryMin, hrvHistoryMax, 0, NUM_RGB_COLORS-1)); // This is the color we are fading towards
  nextRgbIndex = constrain(nextRgbIndex, 0, NUM_RGB_COLORS-1);  
  int fadeTime = 60000/hrvHistoryMax;  // how long we should spend fading to the new color (in ms).  to be safe its the max bpm (min time between beats)
  int rgbIndex;
  // if the current bpm value is greater than the last bpm value we need to fade one direction
  if(hrvHistory[HRV_HISTORY_LENGTH-1] > hrvHistory[HRV_HISTORY_LENGTH-2]) {
    rgbIndex = int(map(sampleCounter, 0, fadeTime, lastRgbIndex, nextRgbIndex)); // this is the next color in the fade process
    rgbIndex = constrain(rgbIndex, lastRgbIndex, nextRgbIndex); // we should never fade past our starting point or destination
  } 
  else { // if the current bpm value is less, we need to fade the other direction
    rgbIndex = int(map(sampleCounter, 0, fadeTime, lastRgbIndex, nextRgbIndex)); // this is the next color in the fade process
    rgbIndex = constrain(rgbIndex, nextRgbIndex, lastRgbIndex); // we should never fade past our starting point or destination
  }


  String toPrint = "";
  toPrint = "min: " + String(hrvHistoryMin);
  Serial.print(toPrint);
  toPrint = ", max: " + String(hrvHistoryMax);
  Serial.println(toPrint);  
  Serial.print("lastRgbIndex: ");
  Serial.println(lastRgbIndex);
  Serial.print("nextRgbIndex: ");
  Serial.println(nextRgbIndex);
  Serial.print("rgbIndex: ");
  Serial.println(rgbIndex);
  Serial.print("hrvHistory[HRV_HISTORY_LENGTH-1]: ");
  Serial.println(hrvHistory[HRV_HISTORY_LENGTH-1]);
  Serial.print("hrvHistory[HRV_HISTORY_LENGTH-2]: ");
  Serial.println(hrvHistory[HRV_HISTORY_LENGTH-2]);
  Serial.print("fadeTime: ");
  Serial.println(fadeTime);
  Serial.print("sampleCounter: ");
  Serial.println(sampleCounter);
  Serial.println("-=-=-=-=-=-=-=-=-=");

  // Set the R,G and B LEDs based on the three mapped values
  analogWrite(LEDR_PIN, rgbColorMap[rgbIndex][RED]);
  analogWrite(LEDG_PIN, rgbColorMap[rgbIndex][GREEN]);
  analogWrite(LEDB_PIN, rgbColorMap[rgbIndex][BLUE]);

}

void setHrvLed() {
  // seed hrvHistory vars with unrealistically high and low numbers
  hrvHistoryMin = 200; 
  hrvHistoryMax = 0;
  // shift history buffer over, making room for new value
  for (int i = 0; i < HRV_HISTORY_LENGTH-1; i ++) {
    // if we have a new HRV value, shift over the history buffer
    hrvHistory[i] = hrvHistory[i+1];
    hrvHistoryMin = min(hrvHistoryMin, hrvHistory[i]);
    hrvHistoryMax = max(hrvHistoryMax, hrvHistory[i]);
  }
  // if we have a new HRV value, stick it in this history buffer

  int bpmFromHrv = 60000/HRV;
  bpmFromHrv = constrain(bpmFromHrv, 30, 100); // constrain bpm to reasonable values
  hrvHistory[HRV_HISTORY_LENGTH-1] = bpmFromHrv; // add the new HRV value as the BPM for the last two beats

  int rgbIndex = int(map(bpmFromHrv, hrvHistoryMin, hrvHistoryMax, 0, NUM_RGB_COLORS-1));
  // map() doesn't care if value goes out of bounds, which it can do at certain edge cases, such as the first value
  rgbIndex = constrain(rgbIndex, 0, NUM_RGB_COLORS-1); 

  // Set the R,G and B LEDs based on the three mapped values
  analogWrite(LEDR_PIN, rgbColorMap[rgbIndex][RED]);
  analogWrite(LEDG_PIN, rgbColorMap[rgbIndex][GREEN]);
  analogWrite(LEDB_PIN, rgbColorMap[rgbIndex][BLUE]);
}

void sendDataToProcessing(char symbol, int data ){
  Serial.print(symbol);      // symbol prefix tells Processing what type of data is coming
  Serial.println(data);      // the data to send
}









































/*
	Project Name:
	Clashdoino

	Author:
	Letsdoino!

	Link to the full guide:
	https://letsdoino.com/2019/02/10/clashdoino/
*/

/* include libraries */
#include "Arduino.h"
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include "EasyVR.h"

/* game settings */
#define DOUBLE_ELIXIR_GAME_TIME 120 // double elixir game mode start [seconds]
#define DOUBLE_ELIXIR_STEP_TIME 1.4 // time to gain 1 elixir in double elixir game mode [seconds]
#define ELIXIR_STEP_TIME 2.8 // time to gain 1 elixir in standard game mode [seconds]
#define ELIXIR_STEPS 10 // total amount of elixirs
#define ELIXIR_STEPS_START 5 // elixir amount at game start
#define COLLECTOR_PRODUCT_TIME 8.5 // elixir collector 1 elixir production time [seconds]
#define COLLECTOR_DEPLOY_TIME 1 // elixir collector deploy time [seconds]
#define COLLECTOR_TIMER 70 // elixir collector life time [seconds]
#define END_GAME_TIME 360 // end game time [seconds]: 3 minutes  + 3 minutes of overtime

/* game related variables or definitions */
byte collector_stored_elixir=0;
byte battle_start=0; // 0:battle not started 1:battle in progress 2: battle just started (game initialization needed)
bool bar_full=false;
bool collector_active=false;
bool collector_reached_full_bar=false;
bool collector_product_running=false;
bool collector_deploy_running=false;
bool double_elixir_game_started=false;
bool single_consumption_bar_full=false;
unsigned int step_time=time_ms(ELIXIR_STEP_TIME);
unsigned int steps_limit[ELIXIR_STEPS];
unsigned long bar_cursor; // the cursor of the elixir bar
unsigned long bar_offset=time_ms(ELIXIR_STEP_TIME)*ELIXIR_STEPS_START; // starting amount of the elixir bar
unsigned long bar_timer_start; // start time of elixir progress bar
unsigned long collector_product_cursor=0; // the cursor of the elixir collector
unsigned long collector_product_start; // start time of the elixir production by the collector
unsigned long collector_timer_start; // start time of the elixir collector
unsigned long game_timer_start; // start time of the battle

/* lcd module related variables or definitions */
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
byte BAR_STEP_0[8] = {B00000,B00000,B00000,B00000,B00000,B00000,B00000,B00000}; // bar step progress drawing
byte BAR_STEP_1[8] = {B10000,B10000,B10000,B10000,B10000,B10000,B10000,B10000}; // bar step progress drawing
byte BAR_STEP_2[8] = {B11000,B11000,B11000,B11000,B11000,B11000,B11000,B11000}; // bar step progress drawing
byte BAR_STEP_3[8] = {B11100,B11100,B11100,B11100,B11100,B11100,B11100,B11100}; // bar step progress drawing
byte BAR_STEP_4[8] = {B11110,B11110,B11110,B11110,B11110,B11110,B11110,B11110}; // bar step progress drawing
byte BAR_STEP_5[8] = {B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111}; // bar step progress drawing
byte DROP[8]       = {B00100,B01110,B01010,B10001,B10001,B10001,B10001,B01110}; // elixir drop drawing

/*          voice recognition module related variables or definitions                 */
/* NOTE:this code comes from the code generator tool of the EasyVR Commander software */
#if !defined(SERIAL_PORT_MONITOR)
  #error "Arduino version not supported. Please update your IDE to the latest version."
#endif
#if defined(__SAMD21G18A__)
  #define port SERIAL_PORT_HARDWARE // Shield Jumper on HW (for Zero, use Programming Port)
  #define pcSerial SERIAL_PORT_MONITOR
#elif defined(SERIAL_PORT_USBVIRTUAL)
  #define port SERIAL_PORT_HARDWARE // Shield Jumper on HW (for Leonardo and Due, use Native Port)
  #define pcSerial SERIAL_PORT_USBVIRTUAL
#else
  #include "SoftwareSerial.h" // Shield Jumper on SW (using pins 12/13 or 8/9 as RX/TX)
  SoftwareSerial port(12, 13);
  #define pcSerial SERIAL_PORT_MONITOR
#endif

EasyVR easyvr(port);
int8_t group, idx;

enum Groups // voice recognition groups and commands
{
  GROUP_1  = 1, // only one group related to the Clash Royale commands
};

/* clash royale vocal commands */
enum Group1
{
  G1_GO = 0,		// starts the battle
  G1_ONE = 1,		// consumes 1 elixirs
  G1_TWO = 2,		// consumes 2 elixirs
  G1_THREE = 3,		// consumes 3 elixirs
  G1_FOUR = 4,		// consumes 4 elixirs
  G1_FIVE = 5,		// consumes 5 elixirs
  G1_SIX = 6,		// consumes 6 elixirs
  G1_SEVEN = 7,		// consumes 7 elixirs
  G1_EIGHT = 8,		// consumes 8 elixirs
  G1_NINE = 9,		// consumes 9 elixirs
  G1_TEN = 10,		// consumes 10 elixirs
  G1_START = 11,	// generates an elixir collector
  G1_STOP = 12,		// destroys the elixir collector card
  G1_END = 13		// ends the battle
};

/* arduino setup function */
void setup()
{
  /* VR module setup */
  Serial.begin(9600);
  setup_easyvr();
  delay(3000);
  
  /* lcd screen initialization and initial screen */
  lcd.begin(16, 2);
  setup_progressbar();
	lcd.setCursor(3, 0);
	lcd.print("CLASHDOINO");
	lcd.setCursor(1, 1);
	lcd.print("elixir counter");
	delay(3000);
  lcd.clear();

  /* calculate the elixir steps time thresholds */
  get_steps_limit(time_ms(ELIXIR_STEP_TIME));

} // arduino setup end

/* arduino loop function */
void loop() {

  easyvr.recognizeCommand(group); // VR module command recognition

  /* elixir bar calculation routine*/
  do 
  {
    
    /* if battle is in progress and game time has not expired calculate elixir bar*/
    if (battle_start>0 && !check_end_game_time()) {

    	/* if battle has just started through the GO vocal command, initialize match parameters */
	    if (battle_start==2) {
	    	initialize_game();
	    	battle_start=1;
	    }

	    /* update elixir bar cursor */
	    (collector_active) ? collector_check() : do_nothing();
	    update_cursor(bar_offset);

	    /* check if game time shall be changed */
	    check_double_elixir_start_time();

	    /* show the informations on the LCD */
	    draw_elixir(bar_cursor);
	    draw_elixirbar(bar_cursor);
	    draw_collector(collector_product_cursor);
	    draw_collectorbar(collector_product_cursor);
	    
	  /* if battle is not in progress keep track of timing and print default message*/
    } else {
    	draw_game_start_template();
    	bar_timer_start = millis();
  		game_timer_start = millis();
    }
  }
  while (!easyvr.hasFinished()); // compute the progress bar while the VR module is busy

  /* action performed if the vocal command has been recogninized */
  idx = easyvr.getCommand();
  action(); 

} // arduino loop end

/* draws on the LCD the elixir amount*/
void draw_elixir(unsigned long timer)
{

  lcd.setCursor(1, 0);
  
  if (timer<=steps_limit[0]){
    lcd.print(0);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>steps_limit[0] && timer<=steps_limit[1]){
    lcd.print(1);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[1] && timer<steps_limit[2]){
    lcd.print(2);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[2] && timer<steps_limit[3]){
    lcd.print(3);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[3] && timer<steps_limit[4]){
    lcd.print(4);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[4] && timer<steps_limit[5]){
    lcd.print(5);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[5] && timer<steps_limit[6]){
    lcd.print(6);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[6] && timer<steps_limit[7]){
    lcd.print(7);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[7] && timer<steps_limit[8]){
    lcd.print(8);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[8] && timer<steps_limit[9]){
    lcd.print(9);
    lcd.setCursor(2,0);
    lcd.write((byte)0);}
  else if (timer>=steps_limit[9]){
    lcd.print(10);}
}

/* adds one elixir */
void add_elixir(unsigned long cursor)
{
  int current_step=cursor/step_time;
  current_step=(current_step>=10)?9:current_step;
  bar_offset= steps_limit[current_step];
  bar_timer_start = millis();
}

/* consumes a certain amount of elixirs */
void consume_elixirs(unsigned int amount)
{
    amount=amount-collector_stored_elixir;
    collector_stored_elixir=0;
    bar_cursor = bar_cursor < amount*step_time ? amount*step_time : bar_cursor;
    bar_offset = bar_cursor - amount*step_time;
    bar_timer_start = millis();
    update_cursor(bar_offset);
}

/* activates the elixir collector */
void collector_enable()
{
	collector_timer_start=millis();
  collector_active=1;
}

/* computes the elixir collector production */
void collector_check()
{
  collector_deploy_running = (read_timer(collector_timer_start) <= time_ms(COLLECTOR_DEPLOY_TIME))?true:false;

  if (!collector_deploy_running){

  		bar_full = (bar_cursor>=step_time*ELIXIR_STEPS) ? 1 : 0;
	  	collector_product_start=(collector_product_running)?collector_product_start:millis();

	  	// production running 
      if (collector_product_running && (read_timer(collector_product_start) < time_ms(COLLECTOR_PRODUCT_TIME))){
        collector_product_cursor=read_timer(collector_product_start);}
      else if (collector_product_running) {
        collector_product_cursor=read_timer(collector_product_start);
        bar_offset = bar_cursor + step_time;
        collector_reached_full_bar=(bar_cursor<step_time*ELIXIR_STEPS && bar_offset>=step_time*ELIXIR_STEPS)?true:false;
        bar_timer_start = millis();
        update_cursor(bar_offset);
        collector_product_running=false;}
      else if ((!collector_product_running && bar_full) && !collector_reached_full_bar && !single_consumption_bar_full) {
        collector_stored_elixir=1;
        collector_reached_full_bar=false;
      	single_consumption_bar_full=false;}
      else {
        collector_product_running=true;}
    }

	if (read_timer(collector_timer_start) > time_ms(COLLECTOR_TIMER)){
		collector_disable();}
}

/* disables the elixir collector */
void collector_disable()
{
  collector_active=0;
  collector_stored_elixir=0;
  collector_product_running=false;
  collector_product_cursor=0;
}

/* saves the time thresholds for each elixir bar */
void get_steps_limit(unsigned int time)
{
  step_time = time;
  steps_limit[0]=time;
  for(int i=1;i<ELIXIR_STEPS;i++){
    steps_limit[i]=steps_limit[i-1]+time;}
}

/* reads the time that is passed */
unsigned long read_timer(unsigned long timer_start)
{
  return (millis()-timer_start);
  ;
}

/* creates the characters needed to the LCD to build the elixir bar */
void setup_progressbar()
{
  lcd.createChar(0, BAR_STEP_0);
  lcd.createChar(1, BAR_STEP_1);
  lcd.createChar(2, BAR_STEP_2);
  lcd.createChar(3, BAR_STEP_3);
  lcd.createChar(4, BAR_STEP_4);
  lcd.createChar(5, BAR_STEP_5);
  lcd.createChar(6, DROP);
}

/* no instructions */
void do_nothing()
{
  // really nothing!
  ;
}

/* draws on the LCD the elixir progress bar */
void draw_elixirbar(unsigned long cursor)
{
  byte percent;
  
  /* from bar cursor to percentage */
  percent = (cursor*100)/(step_time*ELIXIR_STEPS);
  percent = percent > 100 ? 100 : percent;
  percent = percent < 0 ? 0 : percent;

  /* from percent to bar pixels */
  byte pixels = map(percent, 0, 100, 0, 10 * 5);

  /* lcd cursor placing */
  lcd.setCursor(0, 1);
  lcd.write(6);
  lcd.setCursor(1, 1);

  /* bar drawing loop */
  for (byte i = 0; i < 10; i++) {

    if (pixels == 0) {
      lcd.write((byte) 0);

    } else if (pixels >= 5) {
      lcd.write(5);
      pixels = pixels - 5;

    } else {
      lcd.write(pixels);
      pixels = 0;
    }
  }
}

/* draws on the LCD the elixir production time */
void draw_collector(unsigned long timer)
{
	byte production_seconds=timer/1000;
  lcd.setCursor(12, 1);
	if (collector_active && !collector_deploy_running) {
		lcd.print(production_seconds);}
	else {
		lcd.write((byte)0);
	}
}

/* updates the elixir bar cursor */
void update_cursor(unsigned long offset)
{
  bar_cursor = offset + read_timer(bar_timer_start);
  bar_cursor = bar_cursor > step_time*ELIXIR_STEPS ? step_time*ELIXIR_STEPS : bar_cursor;
  bar_cursor = bar_cursor < 0 ? 0 : bar_cursor;
}

/* draws on the LCD the elixir colletor fuel level */
void draw_collectorbar(unsigned long timer)
{
  
  /* from bar cursor to percentage */
  byte percent;
  percent = (timer*100)/(time_ms(COLLECTOR_PRODUCT_TIME));
  percent = percent > 100 ? 100 : percent;
  percent = percent < 0 ? 0 : percent;

  /* from percent to bar pixels */
  byte pixels = map(percent, 0, 100, 0, 1 * 5);  
  
  /* lcd cursor placing */
  lcd.setCursor(13, 1);

  /* bar drawing loop */
  for (byte i = 0; i < 1; i++) {

    if (pixels == 0) {
      lcd.write((byte) 0);

    } else if (pixels >= 5) {
      lcd.write(5);
      pixels = pixels - 5;

    } else {
      lcd.write(pixels);
      pixels = 0;
    }
  }
}

/* VR module setup function */
void setup_easyvr()
{
  bridge:
  // bridge mode?
  int mode = easyvr.bridgeRequested(pcSerial);
  switch (mode)
  {
  case EasyVR::BRIDGE_NONE:
    // setup EasyVR serial port
    port.begin(9600);
    // run normally
    pcSerial.println(F("Bridge not requested, run normally"));
    pcSerial.println(F("---"));
    break;
    
  case EasyVR::BRIDGE_NORMAL:
    // setup EasyVR serial port (low speed)
    port.begin(9600);
    // soft-connect the two serial ports (PC and EasyVR)
    easyvr.bridgeLoop(pcSerial);
    // resume normally if aborted
    pcSerial.println(F("Bridge connection aborted"));
    pcSerial.println(F("---"));
    break;
    
  case EasyVR::BRIDGE_BOOT:
    // setup EasyVR serial port (high speed)
    port.begin(115200);
    pcSerial.end();
    pcSerial.begin(115200);
    // soft-connect the two serial ports (PC and EasyVR)
    easyvr.bridgeLoop(pcSerial);
    // resume normally if aborted
    pcSerial.println(F("Bridge connection aborted"));
    pcSerial.println(F("---"));
    break;
  }

  // initialize EasyVR  
  while (!easyvr.detect())
  {
    pcSerial.println(F("EasyVR not detected!"));
    for (int i = 0; i < 10; ++i)
    {
      if (pcSerial.read() == '?')
        goto bridge;
      delay(100);
    }
  }

  pcSerial.print(F("EasyVR detected, version "));
  pcSerial.print(easyvr.getID());

  if (easyvr.getID() < EasyVR::EASYVR3)
    easyvr.setPinOutput(EasyVR::IO1, LOW); // Shield 2.0 LED off

  if (easyvr.getID() < EasyVR::EASYVR)
    pcSerial.print(F(" = VRbot module"));
  else if (easyvr.getID() < EasyVR::EASYVR2)
    pcSerial.print(F(" = EasyVR module"));
  else if (easyvr.getID() < EasyVR::EASYVR3)
    pcSerial.print(F(" = EasyVR 2 module"));
  else
    pcSerial.print(F(" = EasyVR 3 module"));
    pcSerial.print(F(", FW Rev."));
    pcSerial.println(easyvr.getID() & 7);

  easyvr.setDelay(0); // speed-up replies

  easyvr.setTimeout(0);
  easyvr.setLanguage(0); //<-- same language set on EasyVR Commander when code was generated

  group = 1; //<-- start group (customize)
}

/* VR module action function */
void action()
{
  switch (group)
  {
  case GROUP_1:
    switch (idx)
    {
    case G1_GO:
      battle_start = 2;
      lcd.clear();
      break;
    case G1_ONE:
    	/* if one elixir is consumed while the elixir collector is full, then discharge the collector */
    	if (collector_stored_elixir){
    		collector_product_running=false;
    		collector_stored_elixir=0;
    		single_consumption_bar_full=true;
    	/* otherwise consume one elixir */
    	} else { 
    		consume_elixirs(1);}
      break;
    case G1_TWO:
      consume_elixirs(2);
      break;
    case G1_THREE:
      consume_elixirs(3);
      break;
    case G1_FOUR:
      consume_elixirs(4);
      break;
    case G1_FIVE:
      consume_elixirs(5);
      break;
    case G1_SIX:
      consume_elixirs(6);
      break;
    case G1_SEVEN:
      consume_elixirs(7);
      break;
    case G1_EIGHT:
      consume_elixirs(8);
      break;
    case G1_NINE:
      consume_elixirs(9);
      break;
    case G1_TEN:
      consume_elixirs(10);
      break;
    case G1_START:
      consume_elixirs(6);
      collector_enable();
      break;
    case G1_STOP:
      collector_disable();
      break;
    case G1_END:
      draw_game_end_template();
      break;      
    }
  }
}

/* checks if battle must change to double elixir mode */
void check_double_elixir_start_time()
{
  if (read_timer(game_timer_start)>= time_ms(DOUBLE_ELIXIR_GAME_TIME) && !double_elixir_game_started){
    get_steps_limit(time_ms(DOUBLE_ELIXIR_STEP_TIME));
    bar_offset=(bar_cursor*time_ms(DOUBLE_ELIXIR_STEP_TIME))/time_ms(ELIXIR_STEP_TIME);
    add_elixir(bar_offset);
    double_elixir_game_started=true;
  }
}

/* checks if battle time has expired */
bool check_end_game_time()
{
	if (read_timer(game_timer_start) >= time_ms(END_GAME_TIME)) {
		draw_game_end_template();
		return 1;
	}
	return 0;
}

/* draws on the LCD the game start template */
void draw_game_start_template()
{
	lcd.setCursor(4, 0);
	lcd.print("say 'go'");
	lcd.setCursor(4, 1);
	lcd.print("to begin");
}

/* initialize game parameters */
void initialize_game() 
{
	bar_timer_start = millis();
	game_timer_start = millis();
  	bar_offset = time_ms(ELIXIR_STEP_TIME)*ELIXIR_STEPS_START;
  	get_steps_limit(time_ms(ELIXIR_STEP_TIME));
	bar_full = false;
	collector_active = false;
	collector_reached_full_bar = false;
	collector_product_running = false;
	collector_deploy_running = false;
	double_elixir_game_started = false;
}

/* draws on the LCD the game end template */
void draw_game_end_template()
{
	lcd.clear();
	lcd.setCursor(3, 0);
	lcd.print("battle end");
	lcd.setCursor(1, 1);
	lcd.print("good game! :)");
	delay(3000);
	lcd.clear();
	battle_start=0;
}

/* converts time to milliseconds */
unsigned long time_ms(float time)
{
	;
	return time*1000;
}

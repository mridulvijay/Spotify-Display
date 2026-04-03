#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <SpotifyEsp32.h>
#include <SPI.h>

//lcd pins
#define TFT_CS 1
#define TFT_RST 2
#define TFT_DC 3
#define TFT_SCLK 4
#define TFT_MOSI 5

// switches
#define BTN_PREV 34
#define BTN_PLAY_PAUSE 35
#define BTN_NEXT 36
#define BTN_SHUFFLE 37

// wifi and spotify
const char* SSID = "wifi-1366";
const char* PASSWORD = "YOUR WIFI PASSWORD";
const char* CLIENT_ID = "YOUR CLIENT ID";
const char* CLIENT_SECRET = "YOUR CLIENT SECRET";
const char* REFRESH_TOKEN = "YOUR REFRESH TOKEN";

// colours
#define COLOR_BG ST77XX_BLACK
#define COLOR_ACCENT ST77XX_GREEN
#define COLOR_TEXT ST77XX_WHITE
#define COLOR_MUTED 0x4208

// states
enum DisplayState {
  STATE_LOADING,
  STATE_PLAYING,
  STATE_ERROR
};


Spotify sp(CLIENT_ID, CLIENT_SECRET, REFRESH_TOKEN);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// states
DisplayState currentState = STATE_LOADING;
unsigned long lastUpdateTime = 0;
unsigned long buttonCheckTime = 0;
const unsigned long UPDATE_INTERVAL = 2000;
const unsigned long BUTTON_DEBOUNCE = 250;

// data
struct TrackData {
  String trackName = "";
  String artistName = "";
  String deviceId = "";
  int progressMs = 0;
  int durationMs = 1;
  bool isPlaying = false;
  bool shuffle = false;
  String repeatMode = "off";
};

TrackData currentTrack;
TrackData lastTrack;

// button state
struct ButtonState {
  bool lastPrev = HIGH;
  bool lastPlayPause = HIGH;
  bool lastNext = HIGH;
  bool lastShuffle = HIGH;
};

ButtonState buttonState;


void initWiFi();
void initSpotify();
void drawLoadingScreen();
void drawPlayingScreen();
void updateTrackData();
void drawProgressBar(int x, int y, int width, int height);
void drawControls();
void handleButtons();
void onPrevPressed();
void onPlayPausePressed();
void onNextPressed();
void onShufflePressed();


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== SPOTIFY DISPLAY ===\n");

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  Serial.println("TFT init");

  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_PLAY_PAUSE, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_SHUFFLE, INPUT_PULLUP);

  currentState = STATE_LOADING;
  drawLoadingScreen();

  initWiFi();
  initSpotify();

  currentState = STATE_PLAYING;
  Serial.println("Setup done\n");
}

// setup
void loop() {
  handleButtons();

  if (millis() - lastUpdateTime > UPDATE_INTERVAL) {
    updateTrackData();
    lastUpdateTime = millis();
  }

  if (currentState == STATE_PLAYING) {
    if (currentTrack.trackName != lastTrack.trackName || 
        currentTrack.progressMs != lastTrack.progressMs) {
      drawPlayingScreen();
      lastTrack = currentTrack;
    }
  }

  delay(50);
}

// wifi
void initWiFi() {
  Serial.print("WiFi: ");
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    tft.fillScreen(COLOR_BG);
    tft.setCursor(10, 10);
    tft.setTextColor(COLOR_ACCENT);
    tft.setTextSize(1);
    tft.print("WiFi: ");
    tft.println(WiFi.localIP());
    delay(2000);
  } else {
    Serial.println("\nWiFi failed");
    currentState = STATE_ERROR;
  }
}

// spotify
void initSpotify() {
  Serial.println("Spotify init");
  sp.set_log_level(SPOTIFY_LOG_INFO);
  sp.begin();

  if (sp.is_auth()) {
    Serial.println("Authenticated");
    if (sp.get_access_token()) {
      Serial.println("Token ok");
    }
  } else {
    Serial.println("Auth failed");
    currentState = STATE_ERROR;
  }
}

// display ui
void drawLoadingScreen() {
  tft.fillScreen(COLOR_BG);

  tft.fillRect(0, 0, 160, 8, COLOR_ACCENT);

  tft.setCursor(30, 50);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.print("MRIDUL");

  tft.setCursor(20, 80);
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(1);
  tft.print("Connecting...");

  for (int i = 0; i < 3; i++) {
    tft.setCursor(100 + (i * 8), 80);
    tft.print(".");
    delay(300);
  }
}

void drawPlayingScreen() {
  tft.fillScreen(COLOR_BG);

  // NOW PLAYING
  tft.setCursor(8, 8);
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(1);
  tft.print("NOW PLAYING");

  // Track name
  tft.setCursor(8, 24);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  
  String displayTrackName = currentTrack.trackName;
  if (displayTrackName.length() > 25) {
    displayTrackName = displayTrackName.substring(0, 22) + "...";
  }
  tft.println(displayTrackName);

  // Artist name
  tft.setCursor(8, 34);
  tft.setTextColor(COLOR_MUTED);
  tft.setTextSize(1);
  
  String displayArtistName = currentTrack.artistName;
  if (displayArtistName.length() > 25) {
    displayArtistName = displayArtistName.substring(0, 22) + "...";
  }
  tft.println(displayArtistName);

  // progress bar
  drawProgressBar(8, 44, 144, 3);

  // time
  tft.setCursor(8, 54);
  tft.setTextColor(COLOR_MUTED);
  tft.setTextSize(1);
  int progressSec = currentTrack.progressMs / 1000;
  int durationSec = currentTrack.durationMs / 1000;
  char timeStr[20];
  snprintf(timeStr, sizeof(timeStr), "%d:%02d / %d:%02d", 
    progressSec / 60, progressSec % 60,
    durationSec / 60, durationSec % 60);
  tft.print(timeStr);

 
  drawControls();
}

void drawProgressBar(int x, int y, int width, int height) {
  tft.fillRect(x, y, width, height, COLOR_MUTED);

  if (currentTrack.durationMs > 0) {
    int fillWidth = (currentTrack.progressMs * width) / currentTrack.durationMs;
    tft.fillRect(x, y, fillWidth, height, COLOR_ACCENT);
  }
}

void drawControls() {
  int ctrlY = 75;
  int btnSize = 16;

  // prev 
  tft.drawRect(18, ctrlY, btnSize, btnSize, COLOR_ACCENT);
  tft.setCursor(20, ctrlY + 3);
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(1);
  tft.print("<<");

  // play/button 
  int playBtnX = 80;
  int playBtnY = ctrlY - 2;
  int playBtnSize = 20;
  tft.fillRect(playBtnX - playBtnSize/2, playBtnY, playBtnSize, playBtnSize, COLOR_ACCENT);
  tft.setCursor(playBtnX - 3, playBtnY + 4);
  tft.setTextColor(COLOR_BG);
  tft.setTextSize(1);
  if (currentTrack.isPlaying) {
    tft.print("||");
  } else {
    tft.print(">");
  }

  // next 
  tft.drawRect(122, ctrlY, btnSize, btnSize, COLOR_ACCENT);
  tft.setCursor(124, ctrlY + 3);
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(1);
  tft.print(">>");

  // shuffle
  int shuffleBtnX = 80;
  int shuffleBtnY = 105;
  int shuffleBtnW = 40;
  int shuffleBtnH = 14;
  
  if (currentTrack.shuffle) {
    tft.fillRect(shuffleBtnX - shuffleBtnW/2, shuffleBtnY, shuffleBtnW, shuffleBtnH, COLOR_ACCENT);
    tft.setCursor(shuffleBtnX - 14, shuffleBtnY + 2);
    tft.setTextColor(COLOR_BG);
  } else {
    tft.drawRect(shuffleBtnX - shuffleBtnW/2, shuffleBtnY, shuffleBtnW, shuffleBtnH, COLOR_ACCENT);
    tft.setCursor(shuffleBtnX - 14, shuffleBtnY + 2);
    tft.setTextColor(COLOR_ACCENT);
  }
  tft.setTextSize(1);
  tft.print("SHUFFLE");
}

// data
void updateTrackData() {
  if (!sp.is_auth()) {
    Serial.println("Not auth");
    return;
  }

  JsonDocument filter;
  filter["item"]["name"] = true;
  filter["item"]["artists"] = true;
  filter["item"]["duration_ms"] = true;
  filter["progress_ms"] = true;
  filter["is_playing"] = true;
  filter["device"]["id"] = true;
  filter["shuffle_state"] = true;
  filter["repeat_state"] = true;

  response resp = sp.get_currently_playing_track(filter);

  if (resp.status_code == 200) {
    if (!resp.reply.isNull() && !resp.reply["item"].isNull()) {
      currentTrack.trackName = resp.reply["item"]["name"].as<String>();
      
      JsonArray artists = resp.reply["item"]["artists"].as<JsonArray>();
      currentTrack.artistName = "";
      for (int i = 0; i < artists.size(); i++) {
        if (i > 0) currentTrack.artistName += ", ";
        currentTrack.artistName += artists[i]["name"].as<String>();
      }

      currentTrack.progressMs = resp.reply["progress_ms"].as<int>();
      currentTrack.durationMs = resp.reply["item"]["duration_ms"].as<int>();
      currentTrack.isPlaying = resp.reply["is_playing"].as<bool>();
      currentTrack.deviceId = resp.reply["device"]["id"].as<String>();
      currentTrack.shuffle = resp.reply["shuffle_state"].as<bool>();
      currentTrack.repeatMode = resp.reply["repeat_state"].as<String>();

      Serial.print("Track: ");
      Serial.println(currentTrack.trackName);
    }
  } else {
    Serial.print("API error: ");
    Serial.println(resp.status_code);
  }
}

// ===== BUTTONS =====
void handleButtons() {
  if (millis() - buttonCheckTime < BUTTON_DEBOUNCE) {
    return;
  }

  bool prevPressed = digitalRead(BTN_PREV) == LOW;
  bool playPausePressed = digitalRead(BTN_PLAY_PAUSE) == LOW;
  bool nextPressed = digitalRead(BTN_NEXT) == LOW;
  bool shufflePressed = digitalRead(BTN_SHUFFLE) == LOW;

  if (prevPressed && buttonState.lastPrev == HIGH) {
    onPrevPressed();
    buttonCheckTime = millis();
  }
  if (playPausePressed && buttonState.lastPlayPause == HIGH) {
    onPlayPausePressed();
    buttonCheckTime = millis();
  }
  if (nextPressed && buttonState.lastNext == HIGH) {
    onNextPressed();
    buttonCheckTime = millis();
  }
  if (shufflePressed && buttonState.lastShuffle == HIGH) {
    onShufflePressed();
    buttonCheckTime = millis();
  }

  buttonState.lastPrev = prevPressed;
  buttonState.lastPlayPause = playPausePressed;
  buttonState.lastNext = nextPressed;
  buttonState.lastShuffle = shufflePressed;
}

void onPrevPressed() {
  Serial.println("Prev");
  response resp = sp.skip_to_previous();
  if (resp.status_code == 204) {
    delay(300);
    updateTrackData();
  }
}

void onPlayPausePressed() {
  Serial.println("Play/Pause");
  response resp;
  
  if (currentTrack.isPlaying) {
    resp = sp.pause_playback();
  } else {
    resp = sp.start_a_users_playback(currentTrack.deviceId.c_str());
  }
  
  if (resp.status_code == 204 || resp.status_code == 200) {
    delay(300);
    updateTrackData();
  }
}

void onNextPressed() {
  Serial.println("Next");
  response resp = sp.skip_to_next();
  if (resp.status_code == 204) {
    delay(300);
    updateTrackData();
  }
}

void onShufflePressed() {
  Serial.println("Shuffle");
  response resp = sp.toggle_shuffle(!currentTrack.shuffle);
  if (resp.status_code == 204 || resp.status_code == 200) {
    delay(300);
    updateTrackData();
  }
}

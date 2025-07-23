#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define SS_PIN 10
#define RST_PIN 9
#define RED_PIN 6
#define YELLOW_PIN 5
#define GREEN_PIN 7
#define BUZZER_PIN 4
#define BUTT 3
#define MAX_CARDS 5

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

// UID kartu valid: A2 1E F6 21
bool inRegisterMode = false;
const byte knownSize = 4;
byte knownUIDs[MAX_CARDS][4] = {
  {0xA2, 0x1E, 0xF6, 0x21},  // Kartu 1
};
int registeredCards = 1; // Jumlah kartu yang sudah didaftarkan

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(RED_PIN, OUTPUT);
  pinMode(YELLOW_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); // nonaktifkan buzzer awal
  pinMode(BUTT, INPUT_PULLUP);

  Serial.println("Letakkan kartu RFID...");
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Scan Kartu...");
}

// Fungsi suara buzzer
void beepShort(int times = 2) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
  }
}

void beepLong() {
  digitalWrite(BUZZER_PIN, LOW);
  delay(1000);
  digitalWrite(BUZZER_PIN, HIGH);
}

void saveUIDToEEPROM(byte *uid, byte size) {
  EEPROM.write(0, size); // Simpan ukuran UID
  for (byte i = 0; i < size; i++) {
    EEPROM.write(i + 1, uid[i]); // Simpan UID mulai dari alamat 1
  }
}

bool isSameUID(byte a[], byte b[], byte size) {
  for (byte i = 0; i < size; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

bool isRegistered(byte uid[], byte size) {
  for (int i = 0; i < registeredCards; i++) {
    if (isSameUID(uid, knownUIDs[i], size)) {
      return true;
    }
  }
  return false;
}

void registerCard(byte uid[], byte size) {
  if (registeredCards < MAX_CARDS && !isRegistered(uid, size)) {
    // Simpan ke RAM
    for (byte i = 0; i < size; i++) {
      knownUIDs[registeredCards][i] = uid[i];
    }

    // Simpan ke EEPROM
    int baseAddr = registeredCards * knownSize;
    for (byte i = 0; i < size; i++) {
      EEPROM.update(baseAddr + i, uid[i]);
    }

    // Simpan jumlah kartu
    registeredCards++;
    EEPROM.update(255, registeredCards); // misalnya alamat 255 khusus untuk jumlah kartu

    Serial.println("Kartu berhasil didaftarkan");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Kartu baru");
    lcd.setCursor(0, 1);
    lcd.print("Terdaftar!");
    beepShort(3);
  } else {
    Serial.println("Kartu sudah terdaftar / Penuh");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gagal daftar:");
    lcd.setCursor(0, 1);
    lcd.print("Sudah/Penuh");
    beepLong();
  }
}

void loadRegisteredCards() {
  registeredCards = EEPROM.read(255); // alamat penyimpanan jumlah kartu
  for (int i = 0; i < registeredCards; i++) {
    int baseAddr = i * knownSize;
    for (byte j = 0; j < knownSize; j++) {
      knownUIDs[i][j] = EEPROM.read(baseAddr + j);
    }
  }
}

bool readUIDFromEEPROM(byte *uid, byte &size) {
  size = EEPROM.read(0);
  if (size > 10 || size == 0) return false; // Validasi ukuran

  for (byte i = 0; i < size; i++) {
    uid[i] = EEPROM.read(i + 1);
  }
  return true;
}

void loop() {
  // Mode daftar kartu
  if (digitalRead(BUTT) == LOW) {
    inRegisterMode = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MODE DAFTAR");
    delay(1500);
  }else{
    inRegisterMode = false;
    lcd.setCursor(0, 0);
    lcd.print("Scan kartu...");
  }

  // Jika tidak ada kartu
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    digitalWrite(YELLOW_PIN, HIGH);
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);
    return;
  }

  digitalWrite(YELLOW_PIN, LOW); // kartu terdeteksi

  // Debug: Tampilkan UID
  Serial.print("UID Kartu: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    Serial.print(rfid.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Mode pendaftaran
  if (inRegisterMode) {
    registerCard(rfid.uid.uidByte, rfid.uid.size);
    inRegisterMode = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Kartu Tersimpan");
    delay(1500);
  } 
  // Mode normal
  else {
    if (isRegistered(rfid.uid.uidByte, rfid.uid.size)) {
      Serial.println("Status: Dikenali");
      digitalWrite(GREEN_PIN, HIGH);
      digitalWrite(RED_PIN, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Akses:");
      lcd.setCursor(0, 1);
      lcd.print("Diterima");
      beepShort();
    } else {
      Serial.println("Status: Tidak dikenal");
      digitalWrite(GREEN_PIN, LOW);
      digitalWrite(RED_PIN, HIGH);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Akses:");
      lcd.setCursor(0, 1);
      lcd.print("Ditolak");
      beepLong();
    }
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Kartu...");
  }

  // Reset kondisi
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH); // Matikan buzzer
  rfid.PICC_HaltA(); // Hentikan kartu
}

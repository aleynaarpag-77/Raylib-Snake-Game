#include "raylib.h"    // Görüntü, ses ve klavye için temel kütüphane
#include <stdio.h>     // Metinleri (skor gibi) biçimlendirmek için standart kütüphane

// Web tarayıcılarında çalışması için emscripten ayarı
#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#define SNAKE_LENGTH 256 // Yılan en fazla kaç parça olabilir?
#define SQUARE_SIZE 25   // Oyunun her bir karesinin (ızgara) piksel boyutu

// --- NESNE YAPILARI ---
// Yılan parçası özellikleri (Konum, boyut, hız, renk)
typedef struct Snake { Vector2 position; Vector2 size; Vector2 speed; Color color; } Snake;
// Meyve özellikleri (Konum, boyut, aktif mi, renk)
typedef struct Food { Vector2 position; Vector2 size; bool active; Color color; } Food;

// --- GLOBAL DEĞİŞKENLER --- (Tüm fonksiyonların ortak aklı)
static const int screenWidth = 600;  // Pencere eni
static const int screenHeight = 400; // Pencere boyu
static int framesCounter = 0;        // Oyun hızını kontrol eden zaman sayacı
static bool gameOver = false;        // "Öldük mü?" kontrolü
static bool pause = false;           // "Durdurduk mu?" kontrolü

static Food fruit = { 0 };           // Bir adet meyve kutusu
static Snake snake[SNAKE_LENGTH] = { 0 }; // Yılanın gövdesini tutan liste
static Vector2 snakePosition[SNAKE_LENGTH] = { 0 }; // Hareket arşivi
static bool allowMove = false;       // Bir karede tek yön değişimi kilidi
static Vector2 offset = { 0 };       // Ekran kenar boşluğu
static int counterTail = 0;          // Yılanın güncel uzunluğu

static int score = 0;                // Bu turdaki puanın
static int hiScores[5] = { 0 };      // En yüksek 5 skoru tutan hafıza
static Sound patSesi;                // Ölme sesi
static Sound hamSesi;                // Yemek sesi

// --- FONKSİYONLARIN ÖZETLERİ ---
static void InitGame(void);          // Oyunu sıfırla
static void UpdateGame(void);        // Mantığı hesapla (Tuşlar, çarpma vb.)
static void DrawGame(void);          // Ekrana çiz
static void UpdateDrawFrame(void);   // Hem hesapla hem çiz (Döngü)
static void UpdateHighScores(int currentScore); // Listeyi güncelle

// --- ANA GİRİŞ KAPISI (MAIN) ---
int main(void) {
    InitWindow(screenWidth, screenHeight, "Ham Ham! - Top 5"); // Pencereyi aç
    InitAudioDevice(); // Ses kartını hazırla
    
    patSesi = LoadSound("scream.mp3"); // Sesi dosyadan belleğe al
    hamSesi = LoadSound("yemek.mp3");
    
    InitGame(); // İlk başlangıç ayarlarını yap

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60); // Oyunu saniyede 60 kareye sabitle
    while (!WindowShouldClose()) { UpdateDrawFrame(); } // Çarpıya basana kadar dön
#endif

    // Belleği temizle (Efendi gibi çıkış yap)
    UnloadSound(hamSesi); UnloadSound(patSesi);
    CloseAudioDevice(); CloseWindow();
    return 0;
}

// --- OYUNU İLK HALİNE GETİR ---
void InitGame(void) {
    framesCounter = 0; gameOver = false; pause = false;
    counterTail = 1; allowMove = false; score = 0;
    
    offset.x = screenWidth % SQUARE_SIZE;
    offset.y = screenHeight % SQUARE_SIZE;

    for (int i = 0; i < SNAKE_LENGTH; i++) {
        snake[i].position = (Vector2){ offset.x/2, offset.y/2 };
        snake[i].size = (Vector2){ SQUARE_SIZE, SQUARE_SIZE };
        snake[i].speed = (Vector2){ SQUARE_SIZE, 0 }; // Sağa gitmeye başla
        snake[i].color = (i == 0) ? PURPLE : VIOLET; // Kafa mor, gövde lila
    }
    fruit.size = (Vector2){ SQUARE_SIZE, SQUARE_SIZE };
    fruit.color = RED; fruit.active = false;
}

// --- SKOR LİSTESİNİ SIRALA VE GÜNCELLE ---
void UpdateHighScores(int currentScore) {
    for (int i = 0; i < 5; i++) {
        if (currentScore > hiScores[i]) { // Eğer yeni puan listedekinden büyükse
            for (int j = 4; j > i; j--) hiScores[j] = hiScores[j - 1]; // Alttakileri kaydır
            hiScores[i] = currentScore; // Yeni skoru araya sok
            break; 
        }
    }
}

// --- OYUNUN BEYNİ (HESAPLAMALAR) ---
void UpdateGame(void) {
    if (!gameOver) {
        if (IsKeyPressed('P')) pause = !pause; // P ile durdur
        if (!pause) {
            // YÖN TUŞLARI: Ters yöne dönmeyi engeller
            if (IsKeyPressed(KEY_RIGHT) && (snake[0].speed.x == 0) && allowMove) { snake[0].speed = (Vector2){ SQUARE_SIZE, 0 }; allowMove = false; }
            if (IsKeyPressed(KEY_LEFT) && (snake[0].speed.x == 0) && allowMove) { snake[0].speed = (Vector2){ -SQUARE_SIZE, 0 }; allowMove = false; }
            if (IsKeyPressed(KEY_UP) && (snake[0].speed.y == 0) && allowMove) { snake[0].speed = (Vector2){ 0, -SQUARE_SIZE }; allowMove = false; }
            if (IsKeyPressed(KEY_DOWN) && (snake[0].speed.y == 0) && allowMove) { snake[0].speed = (Vector2){ 0, SQUARE_SIZE }; allowMove = false; }
            
            // Konumları arşive al (Kuyruk takibi için)
            for (int i = 0; i < counterTail; i++) snakePosition[i] = snake[i].position;
            
            // HAREKET ZAMANI: Her 10 karede bir (Hız kontrolü)
            if ((framesCounter % 10) == 0) {
                for (int i = 0; i < counterTail; i++) {
                    if (i == 0) { // Sadece kafa yöne gider
                        snake[0].position.x += snake[0].speed.x;
                        snake[0].position.y += snake[0].speed.y;
                        allowMove = true;
                    }
                    else snake[i].position = snakePosition[i-1]; // Kuyruk bir öncekini izler
                }
            }

            // ÇARPIŞMA: Duvara vurdu mu?
            if (((snake[0].position.x) > (screenWidth - offset.x)) || ((snake[0].position.y) > (screenHeight - offset.y)) || (snake[0].position.x < 0) || (snake[0].position.y < 0)) {
                gameOver = true; UpdateHighScores(score); PlaySound(patSesi);
            }
            
            // ÇARPIŞMA: Kendi kuyruğunu ısırdı mı?
            for (int i = 1; i < counterTail; i++) {
                if ((snake[0].position.x == snake[i].position.x) && (snake[0].position.y == snake[i].position.y)) {
                    gameOver = true; UpdateHighScores(score); PlaySound(patSesi);
                }
            }

            // MEYVE: Yoksa rastgele bir yere koy
            if (!fruit.active) {
                fruit.active = true;
                fruit.position = (Vector2){ GetRandomValue(0, (screenWidth/SQUARE_SIZE) - 1)*SQUARE_SIZE + offset.x/2, GetRandomValue(0, (screenHeight/SQUARE_SIZE) - 1)*SQUARE_SIZE + offset.y/2 };
            }

            // YEMEK YEME: Kafayla meyve üst üste mi?
            if (CheckCollisionRecs((Rectangle){snake[0].position.x, snake[0].position.y, snake[0].size.x, snake[0].size.y}, (Rectangle){fruit.position.x, fruit.position.y, fruit.size.x, fruit.size.y})) {
                snake[counterTail].position = snakePosition[counterTail - 1]; // Yeni kuyruk ekle
                counterTail++; score += 10; fruit.active = false; PlaySound(hamSesi);
            }
            framesCounter++;
        }
    } else if (IsKeyPressed(KEY_ENTER)) InitGame(); // Yeniden başla
}

// --- RESSAM (EKRANA ÇİZİM) ---
void DrawGame(void) {
    BeginDrawing();
    ClearBackground(RAYWHITE); // Tuvali temizle
    
    if (!gameOver) {
        for (int i = 0; i < counterTail; i++) DrawRectangleV(snake[i].position, snake[i].size, snake[i].color);
        DrawRectangleV(fruit.position, fruit.size, fruit.color);
        DrawText(TextFormat("SKOR: %04i", score), 10, 10, 20, PINK);
        if (pause) DrawText("OYUN DURAKLATILDI", screenWidth/2 - 100, screenHeight/2 - 10, 20, GRAY);
    } else {
        // --- SKOR TABLOSU EKRANI ---
        DrawText("YANDIN! [ENTER] BAS", screenWidth/2 - 110, 40, 20, RED);
        DrawText(TextFormat("BU TUR PUANIN: %i", score), screenWidth/2 - 70, 75, 20, PURPLE);
        
        DrawRectangle(screenWidth/2 - 100, 120, 200, 210, LIGHTGRAY); // Gri kutu
        DrawText("EN YÜKSEK 5 SKOR", screenWidth/2 - 85, 130, 18, DARKGRAY);
        
        for (int i = 0; i < 5; i++) {
            Color sColor = (score == hiScores[i] && score != 0) ? GOLD : BLACK; // Yeni rekoru altın yap
            DrawText(TextFormat("%i. %04i", i + 1, hiScores[i]), screenWidth/2 - 40, 170 + (i * 30), 20, sColor);
        }
    }
    EndDrawing();
}

void UpdateDrawFrame(void) { UpdateGame(); DrawGame(); }
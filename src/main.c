#include "raylib.h"
#include <math.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

#define AUDIO_FILE  "resources/resources/song.mp3"
#define ART_FILE    "resources/resources/cover.png"

#define SCREEN_W     1200
#define SCREEN_H     700
#define SAMPLE_COUNT 512

typedef enum { MODE_RADIAL, MODE_WAVE } VisMode;

typedef struct {
    Music     music;
    bool      playing;
    Texture2D albumArt;
    VisMode   mode;
    Color     accent;
    float     pcm[SAMPLE_COUNT];
    float     smooth[SAMPLE_COUNT];
    float     volume;
    bool      isDraggingVolume;
    float     rotation;
} AppState;

static AppState app;

void OnAudioData(void *buffer, unsigned int frames)
{
    float *samples = (float *)buffer;
    unsigned int count = frames < SAMPLE_COUNT ? frames : SAMPLE_COUNT;
    for (unsigned int i = 0; i < count; i++) {
        app.pcm[i] = (samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
    }
}

void ProcessAudio(void)
{
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        float raw  = fabsf(app.pcm[i]);
        float rate = raw > app.smooth[i] ? 0.8f : 0.05f;
        app.smooth[i] += (raw - app.smooth[i]) * rate;
    }
}

Color GetDominantColor(Texture2D tex)
{
    Image  small = LoadImageFromTexture(tex);
    ImageResize(&small, 16, 16);
    Color *px = LoadImageColors(small);
    long   r = 0, g = 0, b = 0;
    for (int i = 0; i < 256; i++) { r += px[i].r; g += px[i].g; b += px[i].b; }
    UnloadImageColors(px);
    UnloadImage(small);
    return (Color){ r/256, g/256, b/256, 255 };
}

void DrawRadial(void)
{
    int   cx    = SCREEN_W / 2;
    int   cy    = SCREEN_H / 2 + 20;
    float baseR = 130.0f;
    float d     = (baseR - 6) * 2.0f;

    if (app.playing) {
        app.rotation += 0.5f * GetFrameTime();
        if (app.rotation >= 360.0f) app.rotation -= 360.0f;
    }

    DrawTexturePro(
        app.albumArt,
        (Rectangle){0, 0, app.albumArt.width, app.albumArt.height},
        (Rectangle){cx, cy, d, d},
        (Vector2){d / 2, d /2},
        app.rotation,
        WHITE
    );

    for (int ring = 1; ring <= 4; ring++) {
        float r = baseR * 0.3f + ring * 14.0f;
        DrawCircleLines(cx, cy, r, (Color){0, 0, 0, 60});
    }

    DrawCircle(cx, cy, 10, (Color){20 ,20 ,20 ,255});
    DrawCircleLines(cx, cy, 10, app.accent);

    DrawCircleLines(cx, cy, baseR - 4, app.accent);

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        float angle = ((float)i / SAMPLE_COUNT) * 2.0f * PI - PI / 2.0f;
        float amp   = app.smooth[i] * 400.0f;

        Vector2 p1 = { cx + cosf(angle) * baseR,         cy + sinf(angle) * baseR };
        Vector2 p2 = { cx + cosf(angle) * (baseR + amp), cy + sinf(angle) * (baseR + amp) };

        Color c = ColorFromHSV((float)i / SAMPLE_COUNT * 360.0f, 0.8f, 0.95f);
        DrawLineEx(p1, p2, 2.0f, c);
    }
}

void DrawWave(void)
{
    int w  = SCREEN_W - 100;
    int x0 = 50;
    int cy = SCREEN_H / 2 + 20;

    DrawLineEx((Vector2){x0, cy}, (Vector2){x0 + w, cy}, 1.0f, (Color){50,50,50,255});

    for (int layer = 2; layer >= 0; layer--) {
        float yOff = (float)(layer - 1) * 20.0f;
        Color lc   = ColorAlpha(app.accent, 1.0f - layer * 0.28f);

        Vector2 prev = {x0, cy + yOff};
        for (int i = 0; i < SAMPLE_COUNT; i++) {
            float x = x0 + (float)i / SAMPLE_COUNT * w;
            float y = cy + yOff + app.pcm[i] * 300.0f;
            Vector2 cur = {x, y};
            if (i > 0) DrawLineEx(prev, cur, 2.0f, lc);
            prev = cur;
        }
    }
}

void GameLoop(void)
{
    UpdateMusicStream(app.music);
    ProcessAudio();

    if (IsKeyPressed(KEY_SPACE)) {
        if (app.playing) { PauseMusicStream(app.music);  app.playing = false; }
        else             { ResumeMusicStream(app.music); app.playing = true;  }
    }

    Rectangle btnRadial   = {20,  20, 110, 32};
    Rectangle btnWave     = {140, 20, 110, 32};
    Rectangle volTrack    = {SCREEN_W - 30, 80, 10, 200};
    Rectangle progressBar = {20, SCREEN_H - 20, SCREEN_W - 40, 16};

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mp = GetMousePosition();
        Rectangle volZone = {SCREEN_W - 44, volTrack.y, 38, volTrack.height};
        if (app.isDraggingVolume || CheckCollisionPointRec(mp, volZone)) {
            app.isDraggingVolume = true;
            float pct = 1.0f - (mp.y - volTrack.y) / volTrack.height;
            app.volume = CLAMP(pct, 0.0f, 1.0f);
            SetMusicVolume(app.music, app.volume);
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) app.isDraggingVolume = false;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mp = GetMousePosition();
        if (CheckCollisionPointRec(mp, btnRadial)) app.mode = MODE_RADIAL;
        if (CheckCollisionPointRec(mp, btnWave))   app.mode = MODE_WAVE;
        if (CheckCollisionPointRec(mp, progressBar)) {
            float pct   = (mp.x - 20.0f) / (SCREEN_W - 40.0f);
            float total = GetMusicTimeLength(app.music);
            SeekMusicStream(app.music, total * pct);
        }
    }

    BeginDrawing();
    ClearBackground((Color){10, 10, 12, 255});

    if (app.mode == MODE_RADIAL) DrawRadial();
    else                         DrawWave();

    Color radCol = (app.mode == MODE_RADIAL) ? app.accent : DARKGRAY;
    Color wavCol = (app.mode == MODE_WAVE)   ? app.accent : DARKGRAY;

    DrawRectangleRec(btnRadial, (Color){20,20,20,255});
    DrawRectangleLinesEx(btnRadial, 1, radCol);
    DrawText("RADIAL",   btnRadial.x + 22, btnRadial.y + 9, 14, radCol);

    DrawRectangleRec(btnWave, (Color){20,20,20,255});
    DrawRectangleLinesEx(btnWave, 1, wavCol);
    DrawText("WAVEFORM", btnWave.x + 8,   btnWave.y + 9,   14, wavCol);

    const char *st = app.playing ? "PLAYING - SPACE to pause" : "PAUSED - SPACE to play";
    DrawText(st, 20, SCREEN_H - 30, 12, DARKGRAY);

    float total   = GetMusicTimeLength(app.music);
    float elapsed = GetMusicTimePlayed(app.music);
    int   pw      = SCREEN_W - 40;
    DrawRectangle(20, SCREEN_H - 12, pw, 4, (Color){30,30,30,255});
    if (total > 0)
        DrawRectangle(20, SCREEN_H - 12, (int)(elapsed / total * pw), 4, app.accent);

    DrawRectangleRec(volTrack, (Color){30,30,30,255});
    float fillH = app.volume * volTrack.height;
    DrawRectangle(volTrack.x, (int)(volTrack.y + volTrack.height - fillH),
                  volTrack.width, (int)fillH, app.accent);

    Rectangle volHandle = {
        SCREEN_W - 34,
        volTrack.y + CLAMP((1.0f - app.volume) * volTrack.height - 8, 0, volTrack.height - 16),
        18, 16
    };
    DrawRectangleRec(volHandle, LIGHTGRAY);
    DrawText("VOL", SCREEN_W - 36, 290, 10, DARKGRAY);

    DrawText("SPECTRUM", SCREEN_W/2 - 44, SCREEN_H - 30, 14, (Color){45,45,45,255});

    EndDrawing();
}

int main(void)
{
    InitWindow(SCREEN_W, SCREEN_H, "SPECTRUM");
    InitAudioDevice();
    SetTargetFPS(60);

    app.albumArt = LoadTexture(ART_FILE);
    app.accent   = GetDominantColor(app.albumArt);
    app.music    = LoadMusicStream(AUDIO_FILE);
    app.mode     = MODE_RADIAL;
    app.playing  = true;
    app.volume  = 1.0f;
    SetMusicVolume(app.music, app.volume);

    AttachAudioStreamProcessor(app.music.stream, OnAudioData);
    PlayMusicStream(app.music);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(GameLoop, 0, 1);
#else
    while (!WindowShouldClose()) GameLoop();
    StopMusicStream(app.music);
    UnloadMusicStream(app.music);
    UnloadTexture(app.albumArt);
    CloseAudioDevice();
    CloseWindow();
#endif

    return 0;
}
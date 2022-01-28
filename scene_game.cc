#include "main.hh"

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_TAU
#define M_TAU 6.28318530717958647692
#endif

struct vec2 {
  float x, y;
  vec2(float x = 0, float y = 0) : x(x), y(y) { }
  inline vec2 operator + (const vec2 &b) const { return vec2(x + b.x, y + b.y); }
  inline vec2 operator - (const vec2 &b) const { return vec2(x - b.x, y - b.y); }
  inline vec2 operator - () const { return vec2(-x, -y); }
  inline vec2 operator * (const float k) const { return vec2(x * k, y * k); }
  inline vec2 operator / (const float k) const { return vec2(x / k, y / k); }
  inline float dot(const vec2 &b) const { return x * b.x + y * b.y; }
  inline float det(const vec2 &b) const { return x * b.y - y * b.x; }
  inline vec2 rot(const float a) const {
    float sin_a = sinf(a), cos_a = cosf(a);
    return vec2(x * cos_a - y * sin_a, x * sin_a + y * cos_a);
  }
  inline float norm() const { return sqrtf(x * x + y * y); }
};

static inline bool seg_intxn(
  const vec2 a, const vec2 b,
  const vec2 c, const vec2 d
) {
  return (c - a).det(b - a) * (d - a).det(b - a) <= 0 &&
         (a - c).det(d - c) * (b - c).det(d - c) <= 0;
}

class scene_game : public scene {
public:
  // ==== Display-related constants ====
  static constexpr float BOARD_W = 20;
  static constexpr float BOARD_H = 12;
  static constexpr float SCALE = 35;
  static rl::Vector2 scr(vec2 p) {
    vec2 q = p * SCALE + vec2(W, H) / 2;
    return (rl::Vector2){q.x, q.y};
  }
  static vec2 board(float x, float y) {
    return (vec2(x, y) - vec2(W, H) / 2) / SCALE;
  }

  static const int STEPS = 480;

  // ==== Tracks ====
  struct track {
    vec2 o;
    float len;  // Total length in units
    enum {
      ATTRACT = (1 << 0),
      RETURN = (1 << 1),
      COLLI = ATTRACT | RETURN,
      FIXED = (1 << 4),
    };
    unsigned flags;

    bool sel;   // Is selected

    track() { }
    track(vec2 o, float len, unsigned flags)
      : o(o), len(len),
        flags(flags),
        sel(false)
      { }
    // Local position at given phase
    virtual vec2 local_at(float t) const = 0;
    vec2 at(float t) const { return local_at(t) + o; }
    // Phase of nearest point on path, given a local point
    // Returns <phase, distance>
    virtual std::pair<float, float> local_nearest(vec2 p) const = 0;
    std::pair<float, float> nearest(vec2 p) const { return local_nearest(p - o); }

    virtual void draw(int T) const = 0;

    inline rl::Color tint() const {
      rl::Color t = (rl::Color){128, 128, 128, 255};
      if (flags & ATTRACT) t = (rl::Color){136, 136, 64, 255};
      if (flags & RETURN) t = (rl::Color){160, 96, 216, 255};
      if (sel) {
        t.r = 255 - (255 - t.r) / 2;
        t.g = 255 - (255 - t.g) / 2;
        t.b = 255 - (255 - t.b) / 2;
      }
      /*if (flags & FIXED) {
        t.r = t.r * 2/3;
        t.g = t.g * 2/3;
        t.b = t.b * 2/3;
      }*/
      return t;
    }
    inline void ripples(int T, float &dist, float &alpha) const {
      if (flags & RETURN) {
        float phase = (float)((T + 450) % 900) / 600;
        if (phase < 1) {
          dist = (1 - powf(1 - phase, 4)) * 0.26;
          alpha = (13 * expf(-4 * phase) * sin(phase) * (1 - phase)) * 0.6;
        }
      }
      if (flags & ATTRACT) {
        float phase = (float)(T % 900) / 600;
        if (phase < 1) {
          dist = powf(1 - phase, 4) * 0.26;
          alpha = (19 * expf(-5.9 * phase) * sin(phase) * (1 - phase)) * 0.6;
        }
      }
    }
  };

  static inline rl::Color premul_alpha(rl::Color tint, float alpha) {
    return (rl::Color){
      (unsigned char)(tint.r * alpha),
      (unsigned char)(tint.g * alpha),
      (unsigned char)(tint.b * alpha),
      (unsigned char)(tint.a * alpha),
    };
  }

  struct track_cir : public track {
    float r;  // Radius
    float fix_angle;  // Angle at which the fix mark is displayed
    int fix_count;    // Number of fix marks
    track_cir(vec2 o, float r, unsigned flags = 0,
      float fix_angle = 0, int fix_count = 2)
      : r(r), fix_angle(fix_angle), fix_count(fix_count),
        track(o, 2 * M_PI * r, flags)
      { }
    vec2 local_at(float t) const { return vec2(r, 0).rot(t / r); }
    std::pair<float, float> local_nearest(vec2 p) const {
      float a = atan2f(p.y, p.x);
      if (a < 0) a += 2 * M_PI;
      return {a * r, (p - vec2(r, 0).rot(a)).norm()};
    }
    void draw(int T) const {
      using namespace rl;
      float w = (flags & FIXED) ? 2 : 2;
      DrawRing(scr(o),
        r * SCALE - w / 2, r * SCALE + w / 2,
        0, 360, 24 * (r < 1 ? 1 : r), tint());

      if (flags & FIXED) {
        float angle = fix_angle;
        vec2 p = vec2(r, 0).rot(angle);
        vec2 move = vec2(0.13, 0).rot(angle - 1.0);
        DrawLineEx(scr(o + p - move), scr(o + p + move), 2, tint());
        if (fix_count != 1)
          DrawLineEx(scr(o - p - move), scr(o - p + move), 2, tint());
      }

      float dist = 0, alpha = 0;
      ripples(T, dist, alpha);
      if (alpha > 0) {
        DrawRing(scr(o),
          (r + dist) * SCALE - w / 2,
          (r + dist) * SCALE + w / 2,
          0, 360, 48, premul_alpha(tint(), alpha));
        if (r > dist) DrawRing(scr(o),
          (r - dist) * SCALE - w / 2,
          (r - dist) * SCALE + w / 2,
          0, 360, 48, premul_alpha(tint(), alpha));
      }
    }
  };

  struct track_seg : public track {
    vec2 ext; // Extension on both sides
    track_seg(vec2 o, vec2 ext, unsigned flags = 0)
      : ext(ext / ext.norm()),
        track(o, ext.norm() * 2, flags)
      { }
    vec2 local_at(float t) const { return ext * (t - len / 2); }
    std::pair<float, float> local_nearest(vec2 p) const {
      float t = p.dot(ext);
      t = (t < -len / 2 ? -len / 2 : (t > len / 2 ? len / 2 : t));
      return {t + len / 2, (p - (ext * t)).norm()};
    }
    void draw(int T) const {
      using namespace rl;
      DrawLineEx(
        scr(o - ext * len / 2), scr(o + ext * len / 2),
        2, tint());

      vec2 n = (ext / ext.norm()).rot(M_PI / 2);

      if (flags & FIXED) {
        for (vec2 endpt : {(o - ext * len / 2), (o + ext * len / 2)}) {
          DrawLineEx(
            scr(endpt - n * 0.1), scr(endpt + n * 0.1),
            2, tint());
        }
      }

      float dist = 0, alpha = 0;
      ripples(T, dist, alpha);
      if (alpha > 0) {
        vec2 move = n * dist;
        DrawLineEx(
          scr(o + move - ext * len / 2), scr(o + move + ext * len / 2),
          2, premul_alpha(tint(), alpha));
        DrawLineEx(
          scr(o - move - ext * len / 2), scr(o - move + ext * len / 2),
          2, premul_alpha(tint(), alpha));
      }
    }
  };

  // ==== Fireflies ===
  struct firefly {
    // Position (track + phase)
    const track *tr;
    float t;
    inline vec2 pos() const { return tr->at(t); }
    float v;    // Velocity

    bool sel;   // Selected?
    static const int TRAIL_N = 20;
    static const int TRAIL_I = 8;
    vec2 trail[TRAIL_N];

    firefly(const track *tr, float t, float v)
      : tr(tr), t(t), v(v),
        sel(false)
      { }

    inline void update(const std::vector<track *> &tracks) {
      float t_prev = t;
      vec2 p1 = pos();
      t += v / STEPS;
      if (t >= tr->len) t -= tr->len;
      if (t < 0) t += tr->len;
      vec2 p2 = pos();

      // Attracting tracks
      for (const auto tr : tracks) if (tr != this->tr && (tr->flags & track::COLLI)) {
        auto near = tr->nearest(p1);
        if (near.second >= 0.01) continue;
        float t1 = near.first;
        float t2 = tr->nearest(p2).first;
        if (fabs(t1 - t2) < 1e-6) {
          float d = (t1 < 1 ? 1e-6 : (t1 * 1e-6));
          t1 -= d;
          t2 += d;
        }
        // Lemma: (p1, p2) crosses the curve C iff
        // (p1, p2) crosses (C(t1), C(t2))
        if (seg_intxn(p1, p2, tr->at(t1), tr->at(t2))) {
          // Point of intersection
          if (tr->flags & track::ATTRACT) {
            // Move to the new track
            this->tr = tr;
            // Take the later parameter to avoid recursion
            this->t = t2;
            // Reverse if making acute turns
            if (this->v * (t2 - t1) < 0) this->v = -this->v;
          }
          if (tr->flags & track::RETURN) {
          /*
            printf("p1 = (%f, %f)\n", p1.x, p1.y);
            printf("p2 = (%f, %f)\n", p2.x, p2.y);
            printf("tr->at(t1) = (%f, %f)\n", tr->at(t1).x, tr->at(t1).y);
            printf("tr->at(t2) = (%f, %f)\n", tr->at(t2).x, tr->at(t2).y);
          */
            this->t = t_prev;
            this->v = -this->v;
          }
          break;
        }
      }
    }
    inline void draw(int offs) const {
      using namespace rl;
      Color tint = (sel ?
        (Color){255, 64, 64, 255} :
        (Color){255, 255, 16, 255});
      float alpha = 1.0/8 * (v < 1 ? 1 : v);
      Color fade = (Color){
        (unsigned char)(tint.r * alpha),
        (unsigned char)(tint.g * alpha),
        (unsigned char)(tint.b * alpha),
        (unsigned char)(tint.a * alpha)
      };

      DrawCircleV(scr(pos()), 4, tint);
      for (int i = 0; i < TRAIL_N; i++) {
        vec2 p = trail[(i + offs) % TRAIL_N];
        DrawCircleV(scr(p), 4 - (float)i / TRAIL_N * 2, fade);
      }
    }

    struct trail_manager {
      std::vector<firefly> &fireflies;
      int counter = 0;
      int pointer = 0;
      trail_manager(std::vector<firefly> &fireflies)
        : fireflies(fireflies),
          counter(0), pointer(0)
        { }

      void reset() {
        counter = pointer = 0;
      }

      void recalc_init() {
        for (auto &f : fireflies) {
          for (int i = 0; i < TRAIL_N; i++)
            f.trail[i] = f.tr->at(f.t - f.v * (float(TRAIL_I) / STEPS) * i);
        }
      }

      void step() {
        if (++counter == TRAIL_I) {
          counter = 0;
          pointer = (pointer + (TRAIL_N - 1)) % TRAIL_N;
          for (firefly &f : fireflies)
            f.trail[pointer] = f.pos();
        }
      }
    };
  };

  // ==== Bellflowers ====
  struct bellflower {
    vec2 o;
    float r;
    int c0, c;  // Initial count and current count

    bellflower(vec2 o, float r, int c0)
      : o(o), r(r), c0(c0)
    {
      reset();
    }

    bool last_on;
    virtual void reset() {
      last_on = false;
      c = c0;
    }
    void update(bool on) {
      if (!last_on && on) c--;
      last_on = on;
    }
    virtual void update(const std::vector<firefly> &fireflies) = 0;
    virtual void draw() const = 0;

    inline bool fireflies_within(const std::vector<firefly> &fireflies) {
      for (const auto f : fireflies)
        if ((f.pos() - o).norm() <= r) return true;
      return false;
    }
  };

  struct bellflower_ord : public bellflower {
    bellflower_ord(vec2 o, float r, int c0)
      : bellflower(o, r, c0)
      { }
    void update(const std::vector<firefly> &fireflies) {
      bool on = fireflies_within(fireflies);
      bellflower::update(on);
    }
    void draw() const {
      using namespace rl;
      DrawRing(scr(o), r * SCALE - 1, r * SCALE + 1, 0, 360, 48, (Color){64, 64, 64, 128});
      DrawCircleV(scr(o), 0.5 * SCALE, last_on ? GREEN : GRAY);
      char s[8];
      snprintf(s, sizeof s, "%d", c);
      DrawText(s, scr(o).x - 4, scr(o).y - 8, 16, BLACK);
    }
  };

  struct bellflower_delay : public bellflower {
    int d, d0;
    bellflower_delay(vec2 o, float r, int c0, float d0)
      : bellflower(o, r, c0), d0(d0 * STEPS)
      { reset(); }
    void reset() {
      bellflower::reset();
      d = d0;
    }
    void update(const std::vector<firefly> &fireflies) {
      bool on = fireflies_within(fireflies);
      if (on) {
        if (d > 0) d--;
      } else {
        d = d0;
      }
      bellflower::update(d == 0);
    }
    void draw() const {
      using namespace rl;
      DrawRing(scr(o), r * SCALE - 1, r * SCALE + 1, 0, 360, 48, (Color){64, 64, 64, 128});
      DrawCircleV(scr(o), 0.5 * SCALE, GRAY);
      DrawCircleV(scr(o), 0.5 * SCALE * (d0 - d) / d0, GREEN);
      char s[8];
      snprintf(s, sizeof s, "%d", c);
      DrawText(s, scr(o).x - 4, scr(o).y - 8, 16, BLACK);
    }
  };

  // ==== Scene ====
  int T;  // Update counter. Overflows after 51 days but whatever

  const char *level_title;
  std::vector<track *> tracks;
  std::vector<firefly> fireflies, fireflies_init;
  std::vector<bellflower *> bellflowers;
  std::vector<std::vector<std::pair<firefly *, float>>> ff_links;

  firefly::trail_manager trail_m;

  firefly *sel_ff;
  track *sel_track;
  vec2 sel_offs;
  int run_state = 0;

  const float RT_SCALE = 2; // Scaling factor for render targets
  rl::RenderTexture2D texBloomBase, texBloomStage1, texBloomStage2;
  rl::Shader shaderBloom;
  int shaderBloomPassLoc;

  rl::Texture2D texBackground;
  static const int BG_TREES_N = 25;
  struct {
    vec2 pos;
    float rot_cen, rot_amp, rot_period;
    unsigned char tint;
  } trees[BG_TREES_N];

  scene_game(int puzzle_id)
    : T(0),
      sel_ff(nullptr), sel_track(nullptr),
      trail_m(fireflies)
  {
    std::vector<std::vector<int>> links;
    switch (puzzle_id) {
      #define T_cir   new track_cir
      #define T_seg   new track_seg
      #define B_ord   new bellflower_ord
      #define B_delay new bellflower_delay
      #define F(_i, _t, ...) \
        firefly(tracks[_i], tracks[_i]->len * (_t), __VA_ARGS__)
      #include "puzzles.hh"
    }
    build_links(links);

    trail_m.recalc_init();

    texBloomBase = rl::LoadRenderTexture(W * RT_SCALE, H * RT_SCALE);
    rl::SetTextureFilter(texBloomBase.texture, rl::TEXTURE_FILTER_BILINEAR);
    texBloomStage1 = rl::LoadRenderTexture(W * RT_SCALE, H * RT_SCALE);
    rl::SetTextureFilter(texBloomStage1.texture, rl::TEXTURE_FILTER_BILINEAR);
    texBloomStage2 = rl::LoadRenderTexture(W, H);
    rl::SetTextureFilter(texBloomStage2.texture, rl::TEXTURE_FILTER_BILINEAR);
  #ifdef PLATFORM_WEB
    shaderBloom = rl::LoadShader("res/bloom_web.vert", "res/bloom_web.frag");
  #else
    shaderBloom = rl::LoadShader("res/bloom.vert", "res/bloom.frag");
  #endif
    shaderBloomPassLoc = rl::GetShaderLocation(shaderBloom, "pass");

    texBackground = rl::LoadTexture("res/board_bg.png");

    unsigned seed = 20220128;
    for (const char *s = level_title; *s != '\0'; s++)
      seed = (seed * 997 + *s);
    for (int i = 0; i < BG_TREES_N; i++) {
      unsigned rands[5];
      for (int j = 0; j < 5; j++) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        rands[j] = seed;
      }
      trees[i] = {
        .pos = vec2(rands[0] % W, rands[1] % H),
        .rot_cen = (float)rands[2] / 0x7fffffff * (float)M_PI * 2,
        .rot_amp = 0.05f + (float)rands[3] / 0x7fffffff * 0.05f,
        .rot_period = 1200 + 1200 * (float)((rands[4] >> 8) % 256) / 256,
        .tint = (unsigned char)(180 + ((rands[4] >> 16) % 32)),
      };
    }
    for (int it = 0; it < 1000; it++) {
      for (int i = 0; i < BG_TREES_N; i++) {
        vec2 move = vec2(0, 0);
        for (int j = 0; j < BG_TREES_N; j++) if (j != i) {
          vec2 d = (trees[i].pos - trees[j].pos);
          if (d.norm() < 240)
            move = move + d / d.norm() * (240 - d.norm());
        }
        trees[i].pos = trees[i].pos + move * 0.01;
        if (trees[i].pos.x < 0) trees[i].pos.x /= 2;
        if (trees[i].pos.x > W) trees[i].pos.x -= (trees[i].pos.x - W) / 2;
        if (trees[i].pos.y < 0) trees[i].pos.y /= 2;
        if (trees[i].pos.y > H) trees[i].pos.y -= (trees[i].pos.y - H) / 2;
      }
    }
  }

  inline void build_links(std::vector<std::vector<int>> links) {
    ff_links.clear();
    ff_links.resize(fireflies.size());
    for (const auto group : links) {
      for (const auto indep : group) {
        auto &list = ff_links[indep];
        float t = fireflies[indep].t;
        list.reserve(group.size() - 1);
        for (const auto dep : group) if (dep != indep)
          list.push_back({&fireflies[dep], fireflies[dep].t - t});
      }
    }
  }

  inline std::pair<firefly *, track *> find(const vec2 p) {
    // Find the nearest firefly
    firefly *best_ff = nullptr;
    float best_dist = 0.75;
    for (auto &f : fireflies) {
      float dist = (p - f.pos()).norm();
      if (dist < best_dist) {
        best_dist = dist;
        best_ff = &f;
      }
    }
    if (best_ff != nullptr) return {best_ff, nullptr};

    // Find the nearest firefly track
    track *best_track = nullptr;
    std::pair<float, float> best_result = {0, 0.5};
    for (const auto t : tracks) if (!(t->flags & track::FIXED)) {
      auto result = t->nearest(p);
      if (result.second < best_result.second) {
        best_result = result;
        best_track = t;
      }
    }
    return {nullptr, best_track};
  }

  void pton(float x, float y) {
    if (run_state & 1) return;
    vec2 p = board(x, y);

    auto near = find(p);
    if (near.first != nullptr) {
      sel_ff = near.first;
      sel_ff->sel = true;
      sel_offs = sel_ff->pos() - p;
    }
    if (near.second != nullptr) {
      sel_track = near.second;
      sel_track->sel = true;
      sel_offs = sel_track->o - p;
    }
  }

  void ptmove(float x, float y) {
    vec2 p = board(x, y);
    if (sel_ff != nullptr) {
      sel_ff->t = sel_ff->tr->nearest(p + sel_offs).first;
      // Move linked fireflies
      int index = sel_ff - &fireflies[0];
      for (const auto link : ff_links[index]) {
        link.first->t = sel_ff->t + link.second;
      }
      trail_m.recalc_init();
    }
    if (sel_track != nullptr) {
      sel_track->o = p + sel_offs;
      trail_m.recalc_init();
    }
  }

  void ptoff(float x, float y) {
    if (sel_ff != nullptr) {
      sel_ff->sel = false;
      sel_ff = nullptr;
    }
    if (sel_track != nullptr) {
      sel_track->sel = false;
      sel_track = nullptr;
    }
  }

  inline void start_run() {
    // Save
    fireflies_init = fireflies;
  }
  inline void stop_run() {
    fireflies = fireflies_init;
    for (auto b : bellflowers) b->reset();
    trail_m.reset();
  }

  bool last_space_down = false;
  void update() {
    T++;
    bool space_down = rl::IsKeyDown(rl::KEY_SPACE);
    if (sel_track == nullptr && !last_space_down && space_down) {
      run_state ^= 1;
      if (run_state & 1) start_run(); else stop_run();
    }
    last_space_down = space_down;
    run_state = (run_state & 1) |
      ((rl::IsKeyDown(rl::KEY_GRAVE) ? 1 :
        rl::IsKeyDown(rl::KEY_ONE) ? 16 : 4) << 1);
    if (run_state & 1) for (int i = 0; i < (run_state >> 1); i++) {
      for (auto &f : fireflies) f.update(tracks);
      for (auto b : bellflowers) b->update(fireflies);
      trail_m.step();
    }
  }

  void draw() {
    using namespace rl;

    ClearBackground((Color){5, 8, 1, 255});

    // Background
    for (int i = 0; i < BG_TREES_N; i++) {
      int id = i % 4;
      float rot = trees[i].rot_cen + trees[i].rot_amp *
        sinf((float)T / trees[i].rot_period * M_PI * 2);
      unsigned char tint = trees[i].tint;
      DrawTexturePro(texBackground,
        (Rectangle){i * 240.f, 0, 240, 240},
        (Rectangle){trees[i].pos.x, trees[i].pos.y, 240, 240},
        (Vector2){120, 120},
        rot * 180 / M_PI,
        (Color){tint, tint, tint, 255});
    }

    // Rule grid
    int x_range = (W / 2 / SCALE) + 1;
    for (int i = -x_range; i <= x_range; i++) {
      float x = scr(vec2(i, 0)).x;
      DrawLineV((Vector2){x, 0}, (Vector2){x, H}, (Color){30, 30, 30, 255});
    }
    int y_range = (H / 2 / SCALE) + 1;
    for (int i = -y_range; i <= y_range; i++) {
      float y = scr(vec2(0, i)).y;
      DrawLineV((Vector2){0, y}, (Vector2){W, y}, (Color){30, 30, 30, 255});
    }

    // Render scaled to texture
    BeginBlendMode(BLEND_ADD_COLORS);

    Color bg = (Color){0, 0, 0, 0};
    BeginTextureMode(texBloomBase);
    BeginMode2D((Camera2D){(Vector2){0, 0}, (Vector2){0, 0}, 0, RT_SCALE});
      ClearBackground(bg);
      for (const auto t : tracks) t->draw(T);
      for (const auto &f : fireflies) f.draw(trail_m.pointer);
      // DrawRectangle(0, 0, 100, 100, (Color){255, 255, 255, 128});
    EndMode2D();
    EndTextureMode();
    //GenTextureMipmaps(&texBloomBase.texture);

    int pass;
    BeginTextureMode(texBloomStage1);
    BeginMode2D((Camera2D){(Vector2){0, 0}, (Vector2){0, 0}, 0, RT_SCALE});
    pass = 1;
    SetShaderValue(shaderBloom, shaderBloomPassLoc, &pass, SHADER_UNIFORM_INT);
    BeginShaderMode(shaderBloom);
      ClearBackground(bg);
      DrawTexturePro(texBloomBase.texture,
        (Rectangle){0, 0, W * RT_SCALE, -H * RT_SCALE},
        (Rectangle){0, 0, W, H},
        (Vector2){0, 0}, 0, WHITE);
    EndShaderMode();
    EndMode2D();
    EndTextureMode();
    //GenTextureMipmaps(&texBloomStage1.texture);

    BeginTextureMode(texBloomStage2);
    pass = 2;
    SetShaderValue(shaderBloom, shaderBloomPassLoc, &pass, SHADER_UNIFORM_INT);
    BeginShaderMode(shaderBloom);
      ClearBackground(bg);
      DrawTexturePro(texBloomStage1.texture,
        (Rectangle){0, 0, W * RT_SCALE, -H * RT_SCALE},
        (Rectangle){0, 0, W, H},
        (Vector2){0, 0}, 0, WHITE);
    EndShaderMode();
    EndTextureMode();

    EndBlendMode();

    DrawTexturePro(texBloomBase.texture,
      (Rectangle){0, 0, W * RT_SCALE, -H * RT_SCALE},
      (Rectangle){0, 0, W, H},
      (Vector2){0, 0}, 0, (Color){255, 255, 255, 160});
    DrawTexturePro(texBloomStage2.texture,
      (Rectangle){0, 0, W, -H},
      (Rectangle){0, 0, W, H},
      (Vector2){0, 0}, 0, WHITE);

    for (const auto b : bellflowers) b->draw();
    DrawTextEx(
      GetFontDefault(),
      level_title,
      (Vector2){20, H - 40},
      32, 3, WHITE);
  }
};

scene *scene_game(int level_id) {
  return new class scene_game(level_id);
}

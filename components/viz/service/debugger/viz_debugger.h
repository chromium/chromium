// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_H_
#define COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_H_

#include <atomic>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/debug/debugging_buildflags.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

// The visual debugger can be completely disabled/enabled at compile time via
// the |USE_VIZ_DEBUGGER| build flag which corresponds to boolean gn arg
// 'use_viz_debugger'. Consult README.md for more information.

#if BUILDFLAG(USE_VIZ_DEBUGGER)

#define VIZ_DEBUGGER_IS_ON() true

namespace viz {

class VIZ_SERVICE_EXPORT VizDebugger {
 public:
  // These functions are called on a gpu thread that is not the
  // 'VizCompositorThread' and therefore have mulithreaded considerations.
  void FilterDebugStream(base::Value json);
  void StartDebugStream(
      mojo::PendingRemote<mojom::VizDebugOutput> pending_debug_output);
  void StopDebugStream();

  struct VIZ_SERVICE_EXPORT StaticSource {
    StaticSource(const char* anno_name,
                 const char* file_name,
                 int file_line,
                 const char* func_name);
    inline bool IsActive() const { return active; }
    inline bool IsEnabled() const { return enabled; }
    const char* anno = nullptr;
    const char* file = nullptr;
    const char* func = nullptr;
    const int line = 0;

    int reg_index = 0;
    bool active = false;
    bool enabled = false;
  };

  struct DrawOption {
    // TODO(petermcneeley): Consider moving this custom rgba color data over to
    // |SkColor| representation.
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    // Alpha is applied to rect fill only.
    uint8_t color_a;
  };

  static ALWAYS_INLINE bool IsEnabled() {
    return enabled_.load(std::memory_order_acquire);
  }

  static VizDebugger* GetInstance();

  ~VizDebugger();

  void CompleteFrame(const uint64_t counter,
                     const gfx::Size& window_pix,
                     base::TimeTicks time_ticks);
  void DrawText(const gfx::Point& pos,
                const std::string& text,
                const StaticSource* dcs,
                DrawOption option);
  void DrawText(const gfx::Vector2dF& pos,
                const std::string& text,
                const StaticSource* dcs,
                DrawOption option);
  void DrawText(const gfx::PointF& pos,
                const std::string& text,
                const StaticSource* dcs,
                DrawOption option);
  void Draw(const gfx::Size& obj_size,
            const gfx::Vector2dF& pos,
            const StaticSource* dcs,
            DrawOption option);
  void Draw(const gfx::SizeF& obj_size,
            const gfx::Vector2dF& pos,
            const StaticSource* dcs,
            DrawOption option);

  void AddLogMessage(std::string log,
                     const StaticSource* dcs,
                     DrawOption option);

  VizDebugger(const VizDebugger&) = delete;
  VizDebugger& operator=(const VizDebugger&) = delete;

 private:
  friend class VizDebuggerInternal;
  static std::atomic<bool> enabled_;
  VizDebugger();
  base::Value FrameAsJson(const uint64_t counter,
                          const gfx::Size& window_pix,
                          base::TimeTicks time_ticks);

  void AddFrame();
  void UpdateFilters();
  void RegisterSource(StaticSource* source);
  void DrawInternal(const gfx::Size& obj_size,
                    const gfx::Vector2dF& pos,
                    const StaticSource* dcs,
                    DrawOption option);
  void ApplyFilters(VizDebugger::StaticSource* source);
  mojo::Remote<mojom::VizDebugOutput> debug_output_;

  // This |task_runner_| is required to send json through mojo.
  scoped_refptr<base::SequencedTaskRunner> gpu_thread_task_runner_;

  struct CallSubmitCommon {
    CallSubmitCommon(int index, int source, DrawOption draw_option)
        : draw_index(index), source_index(source), option(draw_option) {}
    base::DictionaryValue GetDictionaryValue() const;
    int draw_index;
    int source_index;
    VizDebugger::DrawOption option;
  };

  struct DrawCall : public CallSubmitCommon {
    DrawCall(int index,
             int source,
             DrawOption draw_option,
             gfx::Size size,
             gfx::Vector2dF position)
        : CallSubmitCommon(index, source, draw_option),
          obj_size(size),
          pos(position) {}
    gfx::Size obj_size;
    gfx::Vector2dF pos;
  };

  struct DrawTextCall : public CallSubmitCommon {
    DrawTextCall(int index,
                 int source,
                 DrawOption draw_option,
                 gfx::Vector2dF position,
                 std::string str)
        : CallSubmitCommon(index, source, draw_option),
          pos(position),
          text(str) {}
    gfx::Vector2dF pos;
    std::string text;
  };

  struct LogCall : public CallSubmitCommon {
    LogCall(int index, int source, DrawOption draw_option, std::string str)
        : CallSubmitCommon(index, source, draw_option), value(std::move(str)) {}
    std::string value;
  };

  struct FilterBlock {
    FilterBlock(const std::string file_str,
                const std::string func_str,
                const std::string anno_str,
                bool is_active,
                bool is_enabled);
    ~FilterBlock();
    FilterBlock(const FilterBlock& other);
    std::string file;
    std::string func;
    std::string anno;
    bool active = false;
    bool enabled = false;
  };

  // Synchronize access to the variables in the block below as it is mutated by
  // multiple threads.
  base::Lock common_lock_;
  // New filters to promoted to cached filters on next frame.
  std::vector<FilterBlock> new_filters_;
  bool apply_new_filters_next_frame_ = false;
  // Json is saved out every frame on the call to 'CompleteFrame' but may not be
  // uploaded immediately due to task runner sequencing.
  base::Value json_frame_output_;
  size_t last_sent_source_count_ = 0;

  // Cached filters to apply filtering to new sources not just on filter update.
  std::vector<FilterBlock> cached_filters_;
  // Common counter for all submissions.
  int submission_count_ = 0;
  std::vector<DrawCall> draw_rect_calls_;
  std::vector<DrawTextCall> draw_text_calls_;
  std::vector<LogCall> logs_;
  std::vector<StaticSource*> sources_;

  THREAD_CHECKER(viz_compositor_thread_checker_);
};

}  // namespace viz

#define DBG_OPT_RED viz::VizDebugger::DrawOption({255, 0, 0, 0})
#define DBG_OPT_GREEN viz::VizDebugger::DrawOption({0, 255, 0, 0})
#define DBG_OPT_BLUE viz::VizDebugger::DrawOption({0, 0, 255, 0})
#define DBG_OPT_BLACK viz::VizDebugger::DrawOption({0, 0, 0, 0})

#define DBG_DRAW_RECTANGLE_OPT(anno, option, pos, size)                   \
  do {                                                                    \
    if (viz::VizDebugger::IsEnabled()) {                                  \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__, \
                                                __func__);                \
      if (dcs.IsActive()) {                                               \
        viz::VizDebugger::GetInstance()->Draw(size, pos, &dcs, option);   \
      }                                                                   \
    }                                                                     \
  } while (0)

#define DBG_DRAW_RECTANGLE(anno, pos, size) \
  DBG_DRAW_RECTANGLE_OPT(anno, DBG_OPT_BLACK, pos, size)

#define DBG_DRAW_TEXT_OPT(anno, option, pos, text)                          \
  do {                                                                      \
    if (viz::VizDebugger::IsEnabled()) {                                    \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__,   \
                                                __func__);                  \
      if (dcs.IsActive()) {                                                 \
        viz::VizDebugger::GetInstance()->DrawText(pos, text, &dcs, option); \
      }                                                                     \
    }                                                                       \
  } while (0)

#define DBG_DRAW_TEXT(anno, pos, text) \
  DBG_DRAW_TEXT_OPT(anno, DBG_OPT_BLACK, pos, text)

#define DBG_LOG_OPT(anno, option, format, ...)                            \
  do {                                                                    \
    if (viz::VizDebugger::IsEnabled()) {                                  \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__, \
                                                __func__);                \
      if (dcs.IsActive()) {                                               \
        viz::VizDebugger::GetInstance()->AddLogMessage(                   \
            base::StringPrintf(format, __VA_ARGS__), &dcs, option);       \
      }                                                                   \
    }                                                                     \
  } while (0)

#define DBG_LOG(anno, format, ...) \
  DBG_LOG_OPT(anno, DBG_OPT_BLACK, format, __VA_ARGS__)

#define DBG_DRAW_RECT_OPT(anno, option, rect)                                  \
  DBG_DRAW_RECTANGLE_OPT(anno, option,                                         \
                         gfx::Vector2dF(rect.origin().x(), rect.origin().y()), \
                         rect.size())

#define DBG_DRAW_RECT(anno, rect) DBG_DRAW_RECT_OPT(anno, DBG_OPT_BLACK, rect)

#define DBG_FLAG_FBOOL(anno, fun_name)                                    \
  namespace {                                                             \
  bool fun_name() {                                                       \
    if (viz::VizDebugger::IsEnabled()) {                                  \
      static viz::VizDebugger::StaticSource dcs(anno, __FILE__, __LINE__, \
                                                __func__);                \
      if (dcs.IsEnabled()) {                                              \
        return true;                                                      \
      }                                                                   \
    }                                                                     \
    return false;                                                         \
  }                                                                       \
  }  // namespace

#else  //  !BUILDFLAG(USE_VIZ_DEBUGGER)

#define VIZ_DEBUGGER_IS_ON() false

// Viz Debugger is not enabled. The |VizDebugger| class is minimally defined to
// reduce the need for if/def checks in external code. All debugging macros
// compiled to empty statements but do eat some parameters to prevent used
// variable warnings.

namespace viz {
class VIZ_SERVICE_EXPORT VizDebugger {
 public:
  VizDebugger() = default;
  static inline VizDebugger* GetInstance() {
    static VizDebugger g_debugger;
    return &g_debugger;
  }
  inline void CompleteFrame(uint64_t counter,
                            const gfx::Size& window_pix,
                            base::TimeTicks time_ticks) {}

  static inline bool IsEnabled() { return false; }
  VizDebugger(const VizDebugger&) = delete;
  VizDebugger& operator=(const VizDebugger&) = delete;
};
}  // namespace viz

#define DBG_OPT_RED 0

#define DBG_OPT_GREEN 0

#define DBG_OPT_BLUE 0

#define DBG_OPT_BLACK 0

#define DBG_DRAW_RECTANGLE_OPT(anno, option, pos, size) \
  std::ignore = anno;                                   \
  std::ignore = option;                                 \
  std::ignore = pos;                                    \
  std::ignore = size;

#define DBG_DRAW_RECTANGLE(anno, pos, size) \
  DBG_DRAW_RECTANGLE_OPT(anno, DBG_OPT_BLACK, pos, size)

#define DBG_DRAW_TEXT_OPT(anno, option, pos, text) \
  std::ignore = anno;                              \
  std::ignore = option;                            \
  std::ignore = pos;                               \
  std::ignore = text;

#define DBG_DRAW_TEXT(anno, pos, text) \
  DBG_DRAW_TEXT_OPT(anno, DBG_OPT_BLACK, pos, text)

#define DBG_LOG_OPT(anno, option, format, ...) \
  std::ignore = anno;                          \
  std::ignore = option;                        \
  std::ignore = format;

#define DBG_LOG(anno, format, ...) DBG_LOG_OPT(anno, DBG_OPT_BLACK, format, ...)

#define DBG_DRAW_RECT_OPT(anno, option, rect) \
  std::ignore = anno;                         \
  std::ignore = option;                       \
  std::ignore = rect;

#define DBG_DRAW_RECT(anno, rect) DBG_DRAW_RECT_OPT(anno, DBG_OPT_BLACK, rect)

#define DBG_FLAG_FBOOL(anno, fun_name)        \
  namespace {                                 \
  constexpr bool fun_name() { return false; } \
  }

#endif  // BUILDFLAG(USE_VIZ_DEBUGGER)

#endif  // COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_H_

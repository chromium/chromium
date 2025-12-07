// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_PAINT_STABILITY_MONITOR_H_
#define CHROME_RENDERER_ACTOR_PAINT_STABILITY_MONITOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/journal.h"
#include "third_party/blink/public/web/web_interaction_effects_monitor_observer.h"

namespace blink {
class WebInteractionEffectsMonitor;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {
class PageStabilityMetrics;
class ToolBase;

// Helper class for monitoring paint stability after tool usage using
// interaction-attributed contentful paints. This class is largely an
// alternative to PageStabilityMonitor, except that it only supports a subset of
// interactions (see ToolBase::SupportsPaintStability()), and it does not
// support iframes.
class PaintStabilityMonitor
    : public blink::WebInteractionEffectsMonitorObserver {
 public:
  // Returns a `PaintStabilityMonitor` for the given `frame` if `tool` supports
  // paint stability monitoring and the paint stability monitoring feature is
  // not disabled, otherwise nullptr.
  static std::unique_ptr<PaintStabilityMonitor>
  MaybeCreate(content::RenderFrame& frame, TaskId task_id, Journal& journal);

  ~PaintStabilityMonitor() override;

  // WebInteractionEffectsMonitorObserver
  void OnContentfulPaint(uint64_t new_painted_area) override;

  // Initialize paint stability monitoring. All paints occurring between this
  // and `WaitForStable()` are included in the heuristic, such that stability
  // cannot occur until after `WaitForStable()` is called. Decoupling these
  // allows clients to enforce a minimum stability timeout threshold and tie
  // paint stability to other signals, while allowing this monitor to observe
  // all relevant paints.
  void Start(PageStabilityMetrics* metrics = nullptr);

  // Wait for paint stability and invoke `callback` once reached. `callback`
  // will only be invoked if `mode_` is enabled and not log-only. `callback`
  // will be invoked immediately if stability was detected between calling
  // `Start()` and this.
  void WaitForStable(base::OnceClosure callback);

 private:
  PaintStabilityMonitor(content::RenderFrame& frame,
                        TaskId task_id,
                        Journal& journal);

  void OnPaintStabilityDetected();
  void ScheduleContentfulPaintTimeoutTask(const base::Location& location,
                                          base::TimeDelta delay);

  const features::ActorPaintStabilityMode mode_;

  bool is_started_ = false;

  bool is_wait_for_stable_started_ = false;

  // Whether or not paint stability has been reached. This will be reset if new
  // contentful paints are detected after reaching stability, which can happen
  // between `Start()` and `WaitForStable()`.
  bool is_stability_reached_ = false;

  // The callback to invoke when stability has been reached, if `mode_` is
  // enabled and not log-only.
  base::OnceClosure is_stable_callback_;

  TaskId task_id_;

  // The journal for logging. The journal is owned by the render frame observer
  // which owns PageStabilityMonitor so it never outlives this class.
  raw_ref<Journal> journal_;

  // This is owned by the PageStabilityMonitor and it never outlives this class.
  raw_ptr<PageStabilityMetrics> metrics_;

  std::unique_ptr<blink::WebInteractionEffectsMonitor>
      interaction_effects_monitor_;

  base::OneShotTimer contentful_paint_timer_{/*tick_clock=*/nullptr};

  base::WeakPtrFactory<PaintStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAINT_STABILITY_MONITOR_H_

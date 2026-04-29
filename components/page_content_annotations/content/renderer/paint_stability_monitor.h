// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAINT_STABILITY_MONITOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAINT_STABILITY_MONITOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "third_party/blink/public/web/web_interaction_effects_monitor_observer.h"

namespace blink {
class WebInteractionEffectsMonitor;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace page_content_annotations {

class PageStabilityMonitorDelegate;

// Helper class for monitoring paint stability after tool usage using
// interaction-attributed contentful paints. This class is largely an
// alternative to PageStabilityMonitor, except that it only supports a subset of
// interactions (see ToolBase::SupportsPaintStability()), and it does not
// support iframes.
class PaintStabilityMonitor
    : public blink::WebInteractionEffectsMonitorObserver {
 public:
  // Returns a `PaintStabilityMonitor` for the given `frame`.
  static std::unique_ptr<PaintStabilityMonitor> Create(
      content::RenderFrame& frame,
      PageStabilityMonitorDelegate* delegate = nullptr);

  ~PaintStabilityMonitor() override;

  // WebInteractionEffectsMonitorObserver
  void OnContentfulPaint(uint64_t new_painted_area) override;

  // Initialize paint stability monitoring. All paints occurring between this
  // and `WaitForStable()` are included in the heuristic, such that stability
  // cannot occur until after `WaitForStable()` is called. Decoupling these
  // allows clients to enforce a minimum stability timeout threshold and tie
  // paint stability to other signals, while allowing this monitor to observe
  // all relevant paints.
  void Start();

  // Wait for paint stability and invoke `callback` once reached. `callback`
  // will only be invoked if `mode_` is enabled and not log-only. `callback`
  // will be invoked immediately if stability was detected between calling
  // `Start()` and this.
  void WaitForStable(base::OnceClosure callback);

 private:
  PaintStabilityMonitor(content::RenderFrame& frame,
                        PageStabilityMonitorDelegate* delegate);

  void OnPaintStabilityDetected();
  void ScheduleContentfulPaintTimeoutTask(const base::Location& location,
                                          base::TimeDelta delay);

  base::TimeDelta GetInitialPaintTimeout() const;

  base::TimeDelta GetSubsequentPaintTimeout() const;

  // The delegate is owned by the PageStabilityMonitor that created this
  // sub-monitor. Both the paint and network/main thread monitors share the
  // same delegate as they represent a single, unified monitoring session
  // with consistent configuration and logging needs.
  raw_ptr<PageStabilityMonitorDelegate> delegate_ = nullptr;

  bool is_started_ = false;

  bool is_wait_for_stable_started_ = false;

  // Whether or not paint stability has been reached. This will be reset if new
  // contentful paints are detected after reaching stability, which can happen
  // between `Start()` and `WaitForStable()`.
  bool is_stability_reached_ = false;

  // The callback to invoke when stability has been reached, if `mode_` is
  // enabled and not log-only.
  base::OnceClosure is_stable_callback_;

  std::unique_ptr<blink::WebInteractionEffectsMonitor>
      interaction_effects_monitor_;

  base::OneShotTimer contentful_paint_timer_{/*tick_clock=*/nullptr};

  base::WeakPtrFactory<PaintStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAINT_STABILITY_MONITOR_H_

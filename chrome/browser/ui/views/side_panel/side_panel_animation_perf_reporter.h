// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_PERF_REPORTER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_PERF_REPORTER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"

namespace views {
class Widget;
}

// Handles reporting performance metrics for each side panel animation. This
// needs to be created per animation and the metrics are emitted during
// destruction of the the object.
class SidePanelAnimationPerfReporter : public ui::CompositorObserver {
 public:
  SidePanelAnimationPerfReporter(SidePanelType panel_type,
                                 SidePanelAnimationType animation_type,
                                 base::TimeDelta total_animation_time,
                                 views::Widget* widget);
  ~SidePanelAnimationPerfReporter() override;

  SidePanelAnimationPerfReporter(const SidePanelAnimationPerfReporter&) =
      delete;
  SidePanelAnimationPerfReporter& operator=(
      const SidePanelAnimationPerfReporter&) = delete;

  void OnAnimationProgressed();

 private:
  friend class SidePanelAnimationCoordinator;

  // ui::CompositorObserver:
  void OnDidPresentCompositorFrame(
      ui::Compositor* compositor,
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  const SidePanelType panel_type_;
  const SidePanelAnimationType animation_type_;
  const base::TimeDelta total_animation_time_;

  // Tracks all the successfully presented compositor frame during the
  // course of the animation. This is used to compute the animation FPS.
  std::vector<base::TimeTicks> animation_presented_times_;

  // Tracks the last animation step and largest animation step duration
  // during the progress of the animation.
  base::TimeTicks last_animation_step_timestamp_;
  base::TimeDelta largest_animation_step_time_;

  base::ScopedObservation<ui::Compositor, ui::CompositorObserver>
      compositor_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ANIMATION_PERF_REPORTER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_animation_perf_reporter.h"

#include <math.h>

#include <string_view>

#include "base/functional/bind.h"
#include "chrome/browser/ui/side_panel/side_panel_metrics.h"
#include "ui/views/widget/widget.h"

SidePanelAnimationPerfReporter::SidePanelAnimationPerfReporter(
    SidePanelType panel_type,
    SidePanelAnimationType animation_type,
    base::TimeDelta total_animation_time,
    views::Widget* widget)
    : panel_type_(panel_type),
      animation_type_(animation_type),
      total_animation_time_(total_animation_time),
      last_animation_step_timestamp_(base::TimeTicks::Now()) {
  compositor_observation_.Observe(widget->GetCompositor());
}

SidePanelAnimationPerfReporter::~SidePanelAnimationPerfReporter() {
  if (total_animation_time_.is_positive()) {
    int animation_fps =
        static_cast<int>(std::round(animation_presented_times_.size() /
                                    total_animation_time_.InSecondsF()));
    SidePanelMetrics::RecordSidePanelAnimationMetrics(
        panel_type_, animation_type_, largest_animation_step_time_,
        animation_fps);
  }
}

void SidePanelAnimationPerfReporter::OnAnimationProgressed() {
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta elapsed = now - last_animation_step_timestamp_;
  last_animation_step_timestamp_ = now;

  if (largest_animation_step_time_.is_zero() ||
      elapsed > largest_animation_step_time_) {
    largest_animation_step_time_ = elapsed;
  }
}

void SidePanelAnimationPerfReporter::OnDidPresentCompositorFrame(
    ui::Compositor* compositor,
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  if (!feedback.failed()) {
    animation_presented_times_.push_back(feedback.timestamp);
  }
}

void SidePanelAnimationPerfReporter::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  compositor_observation_.Reset();
}

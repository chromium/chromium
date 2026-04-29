// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/renderer/paint_stability_monitor.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/page_content_annotations/content/renderer/page_stability_monitor_delegate.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_interaction_effects_monitor.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace page_content_annotations {

// static
std::unique_ptr<PaintStabilityMonitor> PaintStabilityMonitor::Create(
    content::RenderFrame& frame,
    PageStabilityMonitorDelegate* delegate) {
  return base::WrapUnique(new PaintStabilityMonitor(frame, delegate));
}

PaintStabilityMonitor::PaintStabilityMonitor(
    content::RenderFrame& frame,
    PageStabilityMonitorDelegate* delegate)
    : delegate_(delegate),
      interaction_effects_monitor_(
          std::make_unique<blink::WebInteractionEffectsMonitor>(
              *frame.GetWebFrame(),
              this)) {}

PaintStabilityMonitor::~PaintStabilityMonitor() = default;

void PaintStabilityMonitor::Start() {
  CHECK(!is_stable_callback_);

  is_started_ = true;

  if (delegate_) {
    delegate_->OnEvent(PaintStabilityMonitorStartedEvent{
        .initial_painted_area =
            interaction_effects_monitor_->TotalPaintedArea()});
  }

  // There won't be any interactions if the underlying frame does not support
  // monitoring, which is the case for iframes. Avoid invoking the
  // `is_stable_callback_` in that case.
  if (interaction_effects_monitor_->InteractionCount() == 0) {
    return;
  }

  // Use the initial contentful paint timeout here, even if there have already
  // been contentful paints, to capture the effects of the last interaction.
  ScheduleContentfulPaintTimeoutTask(FROM_HERE, GetInitialPaintTimeout());
}

void PaintStabilityMonitor::WaitForStable(base::OnceClosure callback) {
  CHECK(!is_stable_callback_);
  is_stable_callback_ = std::move(callback);
  is_wait_for_stable_started_ = true;
  // Paint stability might have been reached between `Start()` and now.
  if (is_stability_reached_) {
    OnPaintStabilityDetected();
  }
}

void PaintStabilityMonitor::OnContentfulPaint(uint64_t new_painted_area) {
  // Don't do anything if `Start()` hasn't been called yet.
  if (!is_started_) {
    return;
  }

  // We keep the paint stability monitor actively monitoring for interaction
  // effects so that the metric can still be recorded if there's new contentful
  // paint after the stability was reached. Just report the event without data.
  if (is_wait_for_stable_started_ && is_stability_reached_) {
    if (delegate_) {
      delegate_->OnEvent(InteractionContentfulPaintEvent{});
    }
    return;
  }

  // Otherwise, report the event with data for logging and metrics.
  if (delegate_) {
    delegate_->OnEvent(InteractionContentfulPaintEvent{
        .data = InteractionContentfulPaintEvent::Data{
            .total_painted_area =
                interaction_effects_monitor_->TotalPaintedArea(),
            .new_painted_area = new_painted_area,
            .was_stability_reached = is_stability_reached_}});
  }

  is_stability_reached_ = false;
  ScheduleContentfulPaintTimeoutTask(FROM_HERE, GetSubsequentPaintTimeout());
}

void PaintStabilityMonitor::OnPaintStabilityDetected() {
  if (delegate_) {
    delegate_->OnEvent(PaintStabilityDetectedEvent{
        .total_painted_area = interaction_effects_monitor_->TotalPaintedArea(),
        .is_waiting_for_stable = !!is_stable_callback_});
  }

  is_stability_reached_ = true;

  // If we're monitoring but not yet waiting for stability, wait for
  // `WaitForStable()` to be called.
  if (!is_stable_callback_) {
    return;
  }

  // TODO(b/507143691): This doesn't enforce a minimum area; should it?
  // Since this doesn't require _any_ area, this essentially amounts to a
  // smaller global timeout for pages and interactions that the
  // `interaction_effects_monitor_` does a poor job of attributing (DOM node
  // removal/hiding, <canvas>, <iframe>, etc.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(is_stable_callback_));
}

void PaintStabilityMonitor::ScheduleContentfulPaintTimeoutTask(
    const base::Location& location,
    base::TimeDelta delay) {
  contentful_paint_timer_.Start(
      location, delay,
      base::BindOnce(&PaintStabilityMonitor::OnPaintStabilityDetected,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Returns how long the monitor should wait for the initial contentful paint
// before declaring paint stability.
//
// TODO(b/507143691): This is not based on data and should be revisited when
// histograms are available, or combined with other heuristics, e.g. pending
// interaction-attributed network requests.
base::TimeDelta PaintStabilityMonitor::GetInitialPaintTimeout() const {
  if (delegate_) {
    return delegate_->GetInitialPaintTimeout();
  }

  return features::kPaintStabilityInitialPaintTimeout.Get();
}

// Returns how long the monitor should wait for subsequent contentful paints
// before declaring paint stability.
//
// TODO(b/507143691): This is not based on data and should be revisited when
// histograms are available, or combined with other heuristics, e.g. pending
// interaction-attributed network requests.
base::TimeDelta PaintStabilityMonitor::GetSubsequentPaintTimeout() const {
  if (delegate_) {
    return delegate_->GetSubsequentPaintTimeout();
  }

  return features::kPaintStabilitySubsequentPaintTimeout.Get();
}

}  // namespace page_content_annotations

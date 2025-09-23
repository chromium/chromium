// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/paint_stability_monitor.h"

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/web/web_interaction_effects_monitor.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace actor {

using ::features::ActorPaintStabilityMode;

namespace {

// Returns how long the monitor should wait for the initial contenful paint
// before declaring paint stability.
//
// TODO(crbug.com/434268231): This is not based on data and should be revisited
// when histograms are available, or combined with other heuristics, e.g.
// pending interaction-attributed network requests.
base::TimeDelta GetInitialPaintTimeout() {
  return features::kActorPaintStabilityIntialPaintTimeout.Get();
}

// Returns how long the monitor should wait for subsequent contenful paints
// before declaring paint stability.
//
// TODO(crbug.com/434268231): This is not based on data and should be revisited
// when histograms are available, or combined with other heuristics, e.g.
// pending interaction-attributed network requests.
base::TimeDelta GetSubsequentPaintTimeout() {
  return features::kActorPaintStabilitySubsequentPaintTimeout.Get();
}

}  // namespace

// static
std::unique_ptr<PaintStabilityMonitor> PaintStabilityMonitor::MaybeCreate(
    content::RenderFrame& frame) {
  if (features::kActorPaintStabilityMode.Get() ==
      ActorPaintStabilityMode::kDisabled) {
    return nullptr;
  }
  return base::WrapUnique(new PaintStabilityMonitor(frame));
}

PaintStabilityMonitor::PaintStabilityMonitor(content::RenderFrame& frame)
    : mode_(features::kActorPaintStabilityMode.Get()),
      interaction_effects_monitor_(
          std::make_unique<blink::WebInteractionEffectsMonitor>(
              *frame.GetWebFrame(),
              this)) {
  CHECK_NE(mode_, ActorPaintStabilityMode::kDisabled);
}

PaintStabilityMonitor::~PaintStabilityMonitor() = default;

void PaintStabilityMonitor::Start(Journal::PendingAsyncEntry* journal_entry) {
  CHECK(!journal_entry_);
  CHECK(!is_stable_callback_);
  journal_entry_ = journal_entry;

  journal_entry_->Log("InteractionContentfulPaint",
                      JournalDetailsBuilder()
                          .Add("initial_painted_area",
                               interaction_effects_monitor_->TotalPaintedArea())
                          .Build());

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
  CHECK(journal_entry_);
  CHECK(!is_stable_callback_);
  is_stable_callback_ = std::move(callback);
  // Paint stability might have been reached between `Start()` and now.
  if (is_stability_reached_) {
    OnPaintStabilityDetected();
  }
}

void PaintStabilityMonitor::OnContentfulPaint(uint64_t new_painted_area) {
  // Don't do anything if `Start()` hasn't been called yet.
  if (!journal_entry_) {
    return;
  }
  CHECK(journal_entry_);
  journal_entry_->Log("InteractionContentfulPaint",
                      JournalDetailsBuilder()
                          .Add("total_painted_area",
                               interaction_effects_monitor_->TotalPaintedArea())
                          .Add("new_painted_area", new_painted_area)
                          .Add("was_stability_reached", is_stability_reached_)
                          .Build());
  is_stability_reached_ = false;
  ScheduleContentfulPaintTimeoutTask(FROM_HERE, GetSubsequentPaintTimeout());
}

void PaintStabilityMonitor::OnPaintStabilityDetected() {
  CHECK(journal_entry_);
  journal_entry_->Log("PaintStabilityDetected",
                      JournalDetailsBuilder()
                          .Add("total_painted_area",
                               interaction_effects_monitor_->TotalPaintedArea())
                          .Add("is_waiting_for_stable", !!is_stable_callback_)
                          .Build());
  is_stability_reached_ = true;

  // If we're only logging, continue monitoring to log post-stable contentful
  // paints.
  if (mode_ == ActorPaintStabilityMode::kLogOnly) {
    return;
  }

  // If we're monitoring but not yet waiting for stability, wait for
  // `WaitForStable()` to be called.
  if (!is_stable_callback_) {
    return;
  }

  // TODO(crbug.com/434268231): This doesn't enforce a minimum area; should it?
  // Since this doesn't require _any_ area, this essentially amounts to a
  // smaller global timeout for pages and interactions that the
  // `interaction_effects_monitor_` does a poor job of attributing (DOM node
  // removal/hiding, <canvas>, <iframe>, etc.
  interaction_effects_monitor_.reset();
  std::move(is_stable_callback_).Run();
}

void PaintStabilityMonitor::ScheduleContentfulPaintTimeoutTask(
    const base::Location& location,
    base::TimeDelta delay) {
  contentful_paint_timer_.Start(
      location, delay,
      base::BindOnce(&PaintStabilityMonitor::OnPaintStabilityDetected,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace actor

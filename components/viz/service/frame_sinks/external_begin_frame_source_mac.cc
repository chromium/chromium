// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mac.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/trace_event/trace_event.h"

namespace viz {

///////////////////////////////////////////////////////////////////////////////
// ExternalBeginFrameSourceMac

ExternalBeginFrameSourceMac::ExternalBeginFrameSourceMac(
    std::unique_ptr<DelayBasedTimeSource> time_source,
    uint32_t restart_id)
    : ExternalBeginFrameSource(this, restart_id),
      time_source_(std::move(time_source)) {
  time_source_->SetClient(this);
}

ExternalBeginFrameSourceMac::~ExternalBeginFrameSourceMac() = default;

void ExternalBeginFrameSourceMac::SetDynamicBeginFrameDeadlineOffsetSource(
    DynamicBeginFrameDeadlineOffsetSource*
        dynamic_begin_frame_deadline_offset_source) {
  begin_frame_args_generator_.set_dynamic_begin_frame_deadline_offset_source(
      dynamic_begin_frame_deadline_offset_source);
}

void ExternalBeginFrameSourceMac::SetVSyncDisplayID(int64_t display_id) {
  if (display_id_ == display_id) {
    return;
  }

  display_id_ = display_id;
  display_link_ = ui::DisplayLinkMac::GetForDisplay(
      base::checked_cast<CGDirectDisplayID>(display_id_));
  time_source_next_update_time_ = base::TimeTicks();
  RequestTimeSourceParamsUpdate();
}

void ExternalBeginFrameSourceMac::OnNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames) {
    return;
  }
  needs_begin_frames_ = needs_begin_frames;

  DCHECK_NE(time_source_->Active(), needs_begin_frames_);
  time_source_->SetActive(needs_begin_frames_);
}

void ExternalBeginFrameSourceMac::OnTimerTick() {
  // The VSync parameters skew over time (astonishingly quickly -- 0.1 msec per
  // second). If too much time has elapsed since the last time the vsync
  // parameters were calculated, re-calculate them.
  if (base::TimeTicks::Now() >= time_source_next_update_time_) {
    RequestTimeSourceParamsUpdate();
  }

  // See comments in DelayBasedBeginFrameSource::OnTimerTick regarding the
  // computation of `frame_time`.
  base::TimeTicks frame_time =
      std::max(time_source_->LastTickTime(),
               time_source_->NextTickTime() - time_source_->Interval());
  OnBeginFrame(begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, time_source_->NextTickTime(),
      time_source_->Interval()));
}

void ExternalBeginFrameSourceMac::RequestTimeSourceParamsUpdate() {
  if (!display_link_ || time_source_updater_) {
    return;
  }
  time_source_updater_ = display_link_->RegisterCallback(base::BindRepeating(
      &ExternalBeginFrameSourceMac::OnTimeSourceParamsUpdate,
      weak_factory_.GetWeakPtr()));
}

void ExternalBeginFrameSourceMac::OnTimeSourceParamsUpdate(
    ui::VSyncParamsMac params) {
  time_source_next_update_time_ = base::TimeTicks::Now() + base::Seconds(10);
  time_source_updater_ = nullptr;

  if (params.display_times_valid) {
    time_source_->SetTimebaseAndInterval(params.display_timebase,
                                         params.display_interval);
    last_timebase_ = params.display_timebase;
  } else {
    time_source_->SetTimebaseAndInterval(last_timebase_,
                                         BeginFrameArgs::DefaultInterval());
  }
}

///////////////////////////////////////////////////////////////////////////////
// DelayBasedBeginFrameSourceMac

DelayBasedBeginFrameSourceMac::DelayBasedBeginFrameSourceMac(
    std::unique_ptr<DelayBasedTimeSource> time_source,
    uint32_t restart_id)
    : DelayBasedBeginFrameSource(std::move(time_source), restart_id) {}

DelayBasedBeginFrameSourceMac::~DelayBasedBeginFrameSourceMac() = default;

void DelayBasedBeginFrameSourceMac::SetVSyncDisplayID(int64_t display_id) {
  if (display_id_ == display_id) {
    return;
  }

  display_id_ = display_id;
  display_link_ = ui::DisplayLinkMac::GetForDisplay(
      base::checked_cast<CGDirectDisplayID>(display_id_));
  time_source_next_update_time_ = base::TimeTicks();
  RequestTimeSourceParamsUpdate();
}

void DelayBasedBeginFrameSourceMac::RequestTimeSourceParamsUpdate() {
  if (!display_link_ || time_source_updater_) {
    return;
  }
  time_source_updater_ = display_link_->RegisterCallback(base::BindRepeating(
      &DelayBasedBeginFrameSourceMac::OnTimeSourceParamsUpdate,
      weak_factory_.GetWeakPtr()));
}

void DelayBasedBeginFrameSourceMac::OnTimeSourceParamsUpdate(
    ui::VSyncParamsMac params) {
  time_source_next_update_time_ = base::TimeTicks::Now() + base::Seconds(10);
  time_source_updater_ = nullptr;
  OnUpdateVSyncParameters(params.display_timebase, params.display_interval);
}

void DelayBasedBeginFrameSourceMac::OnTimerTick() {
  // The VSync parameters skew over time (astonishingly quickly -- 0.1 msec per
  // second). If too much time has elapsed since the last time the vsync
  // parameters were calculated, re-calculate them.
  if (base::TimeTicks::Now() >= time_source_next_update_time_) {
    RequestTimeSourceParamsUpdate();
  }
  DelayBasedBeginFrameSource::OnTimerTick();
}

}  // namespace viz

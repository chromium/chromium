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
namespace {
BASE_FEATURE(kForceMacVSyncTimerForDebugging,
             "ForceMacVSyncTimerForDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalBeginFrameSourceMac

ExternalBeginFrameSourceMac::ExternalBeginFrameSourceMac(uint32_t restart_id,
                                                         int64_t display_id)
    : ExternalBeginFrameSource(this, restart_id) {
  if (display_id == display::kInvalidDisplayId) {
    DLOG(ERROR)
        << "DisplayLinkMac ID is not available. "
           "Switch to DelayBasedTimeSource(Timer) for BeginFrameSource.";
  } else {
    SetVSyncDisplayID(display_id);
  }
}

ExternalBeginFrameSourceMac::~ExternalBeginFrameSourceMac() = default;

void ExternalBeginFrameSourceMac::CreateDelayBasedTimeSourceIfNeeded() {
  if (!time_source_) {
    time_source_ = std::make_unique<DelayBasedTimeSource>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
    time_source_->SetClient(this);
    time_source_->SetTimebaseAndInterval(base::TimeTicks::Now(),
                                         BeginFrameArgs::DefaultInterval());
  }
}

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

  // Remove the current callback from display_link_mac_ or from the timer.
  if (needs_begin_frames_) {
    StopBeginFrame();
  }

  // Remove the old DisplayLinkMac.
  display_link_mac_.reset();

  // Get DisplayLinkMac with the new CGDirectDisplayID.
  display_link_mac_ = ui::DisplayLinkMac::GetForDisplay(
      base::checked_cast<CGDirectDisplayID>(display_id));

  // For debugging only. Use the timer for BeginFrameSource.
  if (base::FeatureList::IsEnabled(kForceMacVSyncTimerForDebugging)) {
    display_link_mac_.reset();
  }

  if (display_link_mac_) {
    auto refresh_rate = display_link_mac_->GetRefreshRate();
    if (refresh_rate) {
      nominal_refresh_period_ = base::Seconds(1) / refresh_rate;
    } else {
      nominal_refresh_period_ = BeginFrameArgs::DefaultInterval();
    }
  } else {
    DLOG(ERROR) << "Fail to create DisplayLinkMac for DisplayID: "
                << display_id_ << ". Use the timer as BeginFrameSource";
  }

  if (needs_begin_frames_) {
    StartBeginFrame();
  }
}

void ExternalBeginFrameSourceMac::StartBeginFrame() {
  if (display_link_mac_) {
    DCHECK(!vsync_callback_mac_);
    // Request the callback to be called on the register thread.
    vsync_callback_mac_ = display_link_mac_->RegisterCallback(
        base::BindRepeating(&ExternalBeginFrameSourceMac::OnDisplayLinkCallback,
                            base::Unretained(this)),
        /*do_callback_on_register_thread=*/true);
    if (vsync_callback_mac_) {
      // RegisterCallback succeeded.
      return;
    }

    // Failed. Destroy DisplayLinkMac and switch to the timer.
    DLOG(ERROR) << "Fail to start CVDisplayLink callback for DisplayID: "
                << display_id_ << ". Switch to the timer";
    display_link_mac_.reset();
  }

  // Start the timer.
  CreateDelayBasedTimeSourceIfNeeded();
  time_source_->SetActive(/*active=*/true);
}

void ExternalBeginFrameSourceMac::StopBeginFrame() {
  if (display_link_mac_) {
    DCHECK(vsync_callback_mac_);
    // Remove and unregister VSyncCallbackMac.
    vsync_callback_mac_.reset();
    return;
  }

  // Stop the timer.
  DCHECK(time_source_);
  time_source_->SetActive(/*active=*/false);
}

void ExternalBeginFrameSourceMac::OnNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames) {
    return;
  }
  needs_begin_frames_ = needs_begin_frames;

  // TODO: Try to prevent constant switching between callback register and
  // unregister.
  if (needs_begin_frames_) {
    StartBeginFrame();
  } else {
    StopBeginFrame();
  }
}

// Called on the Viz thread.
void ExternalBeginFrameSourceMac::OnDisplayLinkCallback(
    ui::VSyncParamsMac params) {
  if (!needs_begin_frames_) {
    return;
  }

  base::TimeTicks frame_time;
  base::TimeDelta interval;

  if (params.callback_times_valid) {
    DCHECK(params.callback_timebase != base::TimeTicks());
    DCHECK(!params.callback_interval.is_zero());
    frame_time = params.callback_timebase;
    interval = params.callback_interval;
  } else {
    // Invalid parameters should be rare. Use the default refresh rate.
    frame_time = base::TimeTicks::Now();
    interval = params.display_times_valid ? params.display_interval
                                          : nominal_refresh_period_;
  }

  OnBeginFrame(begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, frame_time + interval, interval));

  last_frame_time_ = frame_time;
  if (last_interval_ != interval) {
    last_interval_ = interval;
    nominal_refresh_period_ = interval;
    DCHECK(update_vsync_params_callback_);
    update_vsync_params_callback_.Run(frame_time, interval);
  }
}

// Timer callbacks when DisplayLink is not available.
void ExternalBeginFrameSourceMac::OnTimerTick() {
  if (!needs_begin_frames_) {
    return;
  }

  // See comments in DelayBasedBeginFrameSource::OnTimerTick regarding the
  // computation of `frame_time`.
  base::TimeTicks frame_time =
      std::max(time_source_->LastTickTime(),
               time_source_->NextTickTime() - time_source_->Interval());
  auto interval = time_source_->Interval();

  OnBeginFrame(begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, time_source_->NextTickTime(), interval));

  last_frame_time_ = frame_time;
  if (last_interval_ != interval) {
    last_interval_ = interval;
    DCHECK(update_vsync_params_callback_);
    update_vsync_params_callback_.Run(frame_time, interval);
  }
}

void ExternalBeginFrameSourceMac::SetPreferredInterval(
    base::TimeDelta interval) {
  if (!display_link_mac_) {
    time_source_->SetTimebaseAndInterval(last_frame_time_, interval);
  }

  // TODO: Implement run_at_half_refresh_rate_ for display_link_mac_.
  preferred_interval_ = interval;
}

base::TimeDelta ExternalBeginFrameSourceMac::GetMaximumRefreshFrameInterval() {
  return nominal_refresh_period_;
}

void ExternalBeginFrameSourceMac::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_params_callback_ = callback;
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

}  // namespace viz

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mac.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/cpu_reduction_experiment.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"

namespace viz {
namespace {

// Output level for VLOG. TODO(crbug.com/40062488): Remove loggings after
// CVDisplayLinkBeginFrameSource is cleaned up.
constexpr int kOutputLevel = 4;

constexpr base::TimeDelta kMaxSupportedFrameInterval = base::Hertz(14);
constexpr auto kDeltaAlmostEqual = base::Microseconds(10);

bool AlmostEqual(base::TimeDelta a, base::TimeDelta b) {
  return (a - b).magnitude() < kDeltaAlmostEqual;
}

BASE_FEATURE(kForceMacVSyncTimerForDebugging,
             "ForceMacVSyncTimerForDebugging",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow CADisplayLink to handle refresh rate within the range based on the app
// work load.
BASE_FEATURE(kUseRefreshRateRange,
             "UseRefreshRateRange",
             base::FEATURE_DISABLED_BY_DEFAULT);

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "DisplayLinkResult" in src/tools/metrics/histograms/enums.xml.
enum class DisplayLinkResult {
  kSuccess = 0,
  kFailedInvalidDisplayId = 1,
  kFailedCreateDisplayLink = 2,
  kFailedRegisterCallback = 3,
  kMaxValue = kFailedRegisterCallback,
};

void RecordDisplayLinkCreateStatus(DisplayLinkResult result) {
  UMA_HISTOGRAM_ENUMERATION("Viz.ExternalBeginFrameSourceMac.DisplayLink",
                            result);
}

// Record the delay from the system CVDisplayLink or CADisplaylink source to
// VizCompositorThread OnDisplayLinkCallback().
void RecordVSyncCallbackDelay(base::TimeDelta delay) {
  if (!base::ShouldLogHistogramForCpuReductionExperiment()) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Viz.BeginFrameSource.VSyncCallbackDelay", delay,
      /*min=*/base::Microseconds(10),
      /*max=*/base::Milliseconds(33), /*bucket_count=*/50);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ExternalBeginFrameSourceMac

ExternalBeginFrameSourceMac::ExternalBeginFrameSourceMac(
    uint32_t restart_id,
    int64_t display_id,
    OutputSurface* output_surface)
    : ExternalBeginFrameSource(this, restart_id),
      output_surface_(output_surface) {
  VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                     << "::ExternalBeginFrameSourceMac() ID:" << display_id;

  if (display_id == display::kInvalidDisplayId) {
    RecordDisplayLinkCreateStatus(DisplayLinkResult::kFailedInvalidDisplayId);
    DLOG(ERROR)
        << "DisplayLinkMac ID is not available. "
           "Switch to DelayBasedTimeSource(Timer) for BeginFrameSource.";
  } else {
    SetVSyncDisplayID(display_id);
  }
}

ExternalBeginFrameSourceMac::~ExternalBeginFrameSourceMac() {
  VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                     << "::~ExternalBeginFrameSourceMac() ID:" << display_id_;
}

void ExternalBeginFrameSourceMac::CreateDelayBasedTimeSourceIfNeeded() {
  if (!time_source_) {
    time_source_ = std::make_unique<DelayBasedTimeSource>(
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
    time_source_->SetClient(this);
    time_source_->SetTimebaseAndInterval(base::TimeTicks::Now(),
                                         preferred_interval_);
  }
}

void ExternalBeginFrameSourceMac::SetVSyncDisplayID(int64_t display_id) {
  if (display_id_ == display_id) {
    return;
  }

  // Forward the |display_id| to output surface for frame presentation.
  output_surface_->SetVSyncDisplayID(display_id);

  // Remove the current callback from display_link_mac_ or from the timer.
  if (needs_begin_frames_) {
    StopBeginFrame();
  }

  // Remove the old DisplayLinkMac.
  display_link_mac_.reset();

  display_id_ = display_id;

  // Get DisplayLinkMac with the new CGDirectDisplayID.
  if (display_id != display::kInvalidDisplayId) {
    display_link_mac_ = ui::DisplayLinkMac::GetForDisplay(display_id);
  }

  // For debugging only. Use the timer for BeginFrameSource.
  if (base::FeatureList::IsEnabled(kForceMacVSyncTimerForDebugging)) {
    display_link_mac_.reset();
  }

  if (display_link_mac_) {
    nominal_refresh_period_ = GetMaximumRefreshFrameInterval();
    preferred_interval_ = nominal_refresh_period_;
    VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                       << "::SetVSyncDisplayID: " << display_id_
                       << ", refresh_period_: " << nominal_refresh_period_;

    display_link_mac_->GetRefreshIntervalRange(
        min_refresh_interval_, max_refresh_interval_, granularity_);

    // Call multiple_hw_refresh_rates_callback_ to notify FrameRateDecider
    // whether the supported refresh rate list will be provided. If set to
    // true, there will not be a list.
    if (base::FeatureList::IsEnabled(kUseRefreshRateRange)) {
      hw_takes_any_refresh_rate_ =
          granularity_ <= base::Milliseconds(1) &&
          min_refresh_interval_ != max_refresh_interval_;

      if (multiple_hw_refresh_rates_callback_) {
        multiple_hw_refresh_rates_callback_.Run(hw_takes_any_refresh_rate_);
      }
    }

    if (update_vsync_params_callback_) {
      update_vsync_params_callback_.Run(display_link_mac_->GetCurrentTime(),
                                        nominal_refresh_period_);
    }

    RecordDisplayLinkCreateStatus(DisplayLinkResult::kSuccess);
  } else {
    DisplayLinkResult display_link_result =
        display_id == display::kInvalidDisplayId
            ? DisplayLinkResult::kFailedInvalidDisplayId
            : DisplayLinkResult::kFailedCreateDisplayLink;
    RecordDisplayLinkCreateStatus(display_link_result);

    DLOG(ERROR) << "Fail to create DisplayLinkMac with DisplayID: "
                << display_id_ << ". Switch to DelayBasedTimeSource.";

    hw_takes_any_refresh_rate_ = false;
    if (multiple_hw_refresh_rates_callback_) {
      multiple_hw_refresh_rates_callback_.Run(false);
    }
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
                            weak_ptr_factory_.GetWeakPtr()));
    if (vsync_callback_mac_) {
      // RegisterCallback succeeded.
      return;
    }

    // Failed. Destroy DisplayLinkMac and switch to the timer.
    display_link_mac_.reset();
    RecordDisplayLinkCreateStatus(DisplayLinkResult::kFailedRegisterCallback);
    DLOG(ERROR) << "Fail to start CVDisplayLink callback for DisplayID: "
                << display_id_ << ". Switch to the timer";
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
    vsyncs_to_skip_ = 0;
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
  just_started_begin_frame_ = true;

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

  if (vsyncs_to_skip_ > 0) {
    TRACE_EVENT_INSTANT0(
        "viz",
        "ExternalBeginFrameSourceMac::OnDisplayLinkCallback - skip_vsync",
        TRACE_EVENT_SCOPE_THREAD);
    vsyncs_to_skip_--;
    return;
  }

  // Calculate the parameters.
  base::TimeTicks frame_time;
  base::TimeDelta interval;
  auto now = base::TimeTicks::Now();

  if (params.callback_times_valid) {
    DCHECK(params.callback_timebase != base::TimeTicks());
    DCHECK(!params.callback_interval.is_zero());
    frame_time = params.callback_timebase;
    interval = params.callback_interval;
  } else {
    // Invalid parameters should be rare. Use the default refresh rate.
    frame_time = now;
    interval = params.display_times_valid ? params.display_interval
                                          : nominal_refresh_period_;
  }

  auto callback_delay =
      params.callback_times_valid ? (now - frame_time) : base::Microseconds(0);
  auto time_to_display = params.display_times_valid
                             ? (params.display_timebase - frame_time)
                             : base::Microseconds(0);
  TRACE_EVENT2("viz", "ExternalBeginFrameSourceMac::OnDisplayLinkCallback",
               "time_to_display", time_to_display.InMicroseconds(),
               "callback_delay", callback_delay.InMicroseconds());
  RecordVSyncCallbackDelay(callback_delay);

  bool display_link_frame_interval_changed =
      !AlmostEqual(nominal_refresh_period_, interval);

  nominal_refresh_period_ = interval;

  // If the preferred frame interval is not equal to |nominal_refresh_period_|,
  // vsync_subsampling_factor_ is bigger than 1.
  vsyncs_to_skip_ = vsync_subsampling_factor_ - 1;
  interval *= vsync_subsampling_factor_;

  OnBeginFrame(begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, frame_time + interval, interval));

  // Notify Display FrameRateDecider of the frame interval change.
  if (display_link_frame_interval_changed) {
    DCHECK(update_vsync_params_callback_);
    VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                       << "::OnDisplayLinkCallback: " << display_id_
                       << ", nominal_refresh_period_: "
                       << nominal_refresh_period_;
    update_vsync_params_callback_.Run(frame_time, nominal_refresh_period_);
  } else if (!just_started_begin_frame_) {
    // There might be delay between the system CVDisplayLink thread and
    // the VizCompositorThread for the CVDisplayLink Callback. This histogram
    // has accounted for the delays in the VizCompositorThread
    base::TimeDelta delta = now - (last_frame_time_ + last_interval_);
    RecordBeginFrameSourceAccuracy(delta);
  }
  just_started_begin_frame_ = false;

  last_frame_time_ = frame_time;
  last_interval_ = interval;
}

BeginFrameArgs ExternalBeginFrameSourceMac::GetMissedBeginFrameArgs(
    BeginFrameObserver* obs) {
  auto frame_time = last_begin_frame_args_.frame_time;
  auto interval = last_begin_frame_args_.interval;

  // Create BeginFrameArgs for now so that we don't have to wait until vsync.
  if (display_link_mac_) {
    base::TimeTicks now = display_link_mac_->GetCurrentTime();
    if (last_begin_frame_args_.IsValid()) {
      frame_time = now.SnappedToNextTick(frame_time, interval) - interval;
    } else {
      frame_time = now;
      interval = nominal_refresh_period_ * vsync_subsampling_factor_;
    }
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    if (last_begin_frame_args_.IsValid()) {
      frame_time = now.SnappedToNextTick(frame_time, interval) - interval;
    } else {
      frame_time = now;
      interval = preferred_interval_;
    }
  }

  // Don't create new args unless we've actually moved past the previous frame.
  if (!last_begin_frame_args_.IsValid() ||
      frame_time > last_begin_frame_args_.frame_time) {
    last_begin_frame_args_ = begin_frame_args_generator_.GenerateBeginFrameArgs(
        source_id(), frame_time, frame_time + interval, interval);
  }

  return ExternalBeginFrameSource::GetMissedBeginFrameArgs(obs);
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

  if (last_interval_ != interval) {
    DCHECK(update_vsync_params_callback_);
    update_vsync_params_callback_.Run(frame_time, interval);
  }

  last_frame_time_ = frame_time;
  last_interval_ = interval;
}

void ExternalBeginFrameSourceMac::SetPreferredInterval(
    base::TimeDelta interval) {
  preferred_interval_ = interval;

  VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                     << "::SetPreferredInterval: ID: " << display_id_
                     << ", Interval: " << interval;

  if (!display_link_mac_) {
    time_source_->SetTimebaseAndInterval(last_frame_time_, interval);
    return;
  }

  // For the monitor with multitple refresh rates and CVDisplayLink
  // SetPreferredInterval is supported. Just set the preferred interval without
  // skipping VSyncs.
  if (min_refresh_interval_ != max_refresh_interval_) {
    if (base::FeatureList::IsEnabled(kUseRefreshRateRange)) {
      // Request a dynamic refrate rate with a range.
      display_link_mac_->SetPreferredIntervalRange(
          min_refresh_interval_, max_refresh_interval_, interval);
    } else {
      // Request a fixed refresh rate.
      display_link_mac_->SetPreferredInterval(interval);
    }
    nominal_refresh_period_ = interval;
    vsync_subsampling_factor_ = 1;
    vsyncs_to_skip_ = 0;
    return;
  }

  // Here is for the monitor with a fixed refresh rate.
  // Cap the preferred refresh interval if it's out of the range.
  base::TimeDelta adjusted_interval = interval;
  if (interval < nominal_refresh_period_) {
    adjusted_interval = nominal_refresh_period_;
  } else if (interval > kMaxSupportedFrameInterval &&
             !AlmostEqual(interval, nominal_refresh_period_)) {
    adjusted_interval = kMaxSupportedFrameInterval;
  }

  // Keep |vsyncs_to_skip_| unchanged so it will complete the whole frame
  // interal.

  vsync_subsampling_factor_ =
      adjusted_interval.IntDiv((nominal_refresh_period_ - kDeltaAlmostEqual));

  TRACE_EVENT1("gpu", "ExternalBeginFrameSourceMac::SetPreferredInterval",
               "vsync_subsampling_factor", vsync_subsampling_factor_);
}

base::TimeDelta ExternalBeginFrameSourceMac::GetMaximumRefreshFrameInterval() {
  if (display_link_mac_) {
    auto refresh_rate = display_link_mac_->GetRefreshRate();
    if (refresh_rate) {
      return base::Seconds(1) / refresh_rate;
    }
  }

  return BeginFrameArgs::DefaultInterval();
}

void ExternalBeginFrameSourceMac::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_params_callback_ = callback;
}

void ExternalBeginFrameSourceMac::SetMultipleHWRefreshRatesCallback(
    MultipleHWRefreshRatesCallback callback) {
  multiple_hw_refresh_rates_callback_ = callback;
}

base::flat_set<base::TimeDelta>
ExternalBeginFrameSourceMac::GetSupportedFrameIntervals(
    base::TimeDelta current_interval) {
  VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                     << "::GetSupportedFrameIntervals: ID: " << display_id_;

  // When CAdisplayLink will take any preferred refresh rate, return an empty
  // supported_intervals list.
  if (display_link_mac_ && hw_takes_any_refresh_rate_) {
    return {};
  }

  if (nominal_refresh_period_ > kMaxSupportedFrameInterval &&
      min_refresh_interval_ == max_refresh_interval_) {
    VLOG(kOutputLevel) << "nominal_refresh_period_: "
                       << nominal_refresh_period_;
    return {nominal_refresh_period_};
  }

  if (granularity_.is_zero()) {
    return {nominal_refresh_period_};
  }

  base::flat_set<base::TimeDelta> supported_intervals;

  // Check if we can set various preferred intervals within the range.
  if (display_link_mac_ && min_refresh_interval_ != max_refresh_interval_) {
    // |max_refresh_interval_| might not be the same as
    // (|min_refresh_interval_| + n*|granularity_|), so add
    // |max_refresh_interval_| separately after the loop.
    auto upper_bound = max_refresh_interval_ - granularity_ / 2;
    for (base::TimeDelta interval = min_refresh_interval_;
         interval < upper_bound; interval += granularity_) {
      supported_intervals.insert(interval);
    }
    supported_intervals.insert(max_refresh_interval_);

    return supported_intervals;
  }

  // Can only do fixed refresh rates. Now try to implement 2^n refresh
  // rates by skipping VSyncs.
  nominal_refresh_period_ = GetMaximumRefreshFrameInterval();
  base::TimeDelta interval = nominal_refresh_period_;
  while (interval <= kMaxSupportedFrameInterval) {
    VLOG(kOutputLevel) << interval;
    supported_intervals.insert(interval);
    interval *= 2;
  }

  return supported_intervals;
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
  display_link_ = ui::DisplayLinkMac::GetForDisplay(display_id_);
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

  VLOG(kOutputLevel) << "DelayBasedBeginFrameSourceMac(" << this << ")"
                     << "::OnTimeSourceParamsUpdate: ID: " << display_id_
                     << "last_hw_interval_," << last_hw_interval_
                     << "display_interval," << params.display_interval;

  CHECK(update_vsync_params_callback_);
  if (last_hw_interval_ == params.display_interval) {
    // No hw display interval change. Keep the current preferred interval in
    // DelayBasedTimeSource.
    // (ex. HW interval is 60Hz but the preferred interval is 30Hz.)
    OnUpdateVSyncParameters(params.display_timebase, time_source()->Interval());
  } else {
    // Notify Display FrameRateDecider of the new display interval.
    // FrameRateDecider will set the preferred frame interval.
    update_vsync_params_callback_.Run(params.display_timebase,
                                      params.display_interval);
  }

  last_hw_interval_ = params.display_interval;
}

void DelayBasedBeginFrameSourceMac::AddObserver(BeginFrameObserver* obs) {
  if (!time_source()->Active()) {
    just_started_begin_frame_ = true;
  }

  DelayBasedBeginFrameSource::AddObserver(obs);
}

void DelayBasedBeginFrameSourceMac::OnTimerTick() {
  if (last_begin_frame_args().interval == time_source()->Interval() &&
      !just_started_begin_frame_) {
    base::TimeDelta delta =
        base::TimeTicks::Now() - last_begin_frame_args().deadline;
    RecordBeginFrameSourceAccuracy(delta);
  }
  just_started_begin_frame_ = false;

  // The VSync parameters skew over time (astonishingly quickly -- 0.1 msec per
  // second). If too much time has elapsed since the last time the vsync
  // parameters were calculated, re-calculate them.
  if (base::TimeTicks::Now() >= time_source_next_update_time_) {
    RequestTimeSourceParamsUpdate();
  }
  DelayBasedBeginFrameSource::OnTimerTick();
}

void DelayBasedBeginFrameSourceMac::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_params_callback_ = callback;
}

}  // namespace viz

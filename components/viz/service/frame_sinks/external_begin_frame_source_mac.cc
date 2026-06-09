// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mac.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
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
             base::FEATURE_DISABLED_BY_DEFAULT);

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "DisplayLinkResult" in src/tools/metrics/histograms/enums.xml.
enum class DisplayLinkResult {
  kSuccess = 0,
  kFailedInvalidDisplayId = 1,
  kFailedCreateDisplayLink = 2,
  kFailedRegisterCallback = 3,
  kSuccessForcedUpdate = 4,
  kFailedForcedUpdateCreateDisplayLink = 5,
  kMaxValue = kFailedForcedUpdateCreateDisplayLink,
};

void RecordDisplayLinkCreateStatus(DisplayLinkResult result) {
  UMA_HISTOGRAM_ENUMERATION("Viz.ExternalBeginFrameSourceMac.DisplayLink",
                            result);
}

// Record the delay from the system CVDisplayLink or CADisplaylink source to
// VizCompositorThread OnDisplayLinkCallback().
void RecordVSyncCallbackDelay(base::TimeDelta delay) {
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
      create_time_(base::TimeTicks::Now()),
      output_surface_(output_surface) {
  VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                     << "::ExternalBeginFrameSourceMac() ID:" << display_id;

  if (ui::DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
    base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  }

  // The |display_id_| default is display::kInvalidDisplayId. If the incoming
  // display_id happens to be invalid, force SetVSyncDisplayID() to set up the
  // timer correctly.
  bool force_update = display_id == display_id_;
  SetVSyncDisplayID(display_id, force_update);
}

ExternalBeginFrameSourceMac::~ExternalBeginFrameSourceMac() {
  VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                     << "::~ExternalBeginFrameSourceMac() ID:" << display_id_;
  if (ui::DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
    base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  }
}

void ExternalBeginFrameSourceMac::CreateDelayBasedTimeSourceIfNeeded() {
  if (time_source_) {
    return;
  }

  TRACE_EVENT("viz", "ExternalBeginFrameSourceMac::CreateDelayBasedTimeSource");
  time_source_ = std::make_unique<DelayBasedTimeSource>(
      base::SingleThreadTaskRunner::GetCurrentDefault().get());
  time_source_->SetClient(this);
  last_frame_time_ = base::TimeTicks::Now();
  last_interval_ = preferred_interval_;
  time_source_->SetTimebaseAndInterval(last_frame_time_, preferred_interval_);
}

// Forces an update of the DisplayLinkMac for the specified display. This is
// called when the browser-side CADisplayLink state changes (e.g., becomes
// valid or invalid) or when a display is added or removed.
void ExternalBeginFrameSourceMac::UpdateVSyncDisplay(
    int64_t display_id,
    bool is_browser_vsync_supported) {
  if (display_id_ == display_id) {
    // If the browser-side DisplayLink becomes valid and we haven't deferred a
    // VSync update yet, defer the update to ensure a smooth transition.
    // If the browser-side DisplayLink fails (|is_browser_vsync_supported| is
    // false), call SetVSyncDisplayID() immediately to update display_link_
    // for a fallback.
    if (is_browser_vsync_supported && !did_defer_vsync_update_once_) {
      did_defer_vsync_update_once_ = true;

      // Defer the transition to DisplayLinkMac if we are currently needing
      // begin frames or if the first frame hasn't swapped yet. This ensures
      // a smooth transition without jank during startup or active rendering.
      if (!has_swapped_frame_ || needs_begin_frames_) {
        vsync_display_id_update_deferred_ = true;
        return;
      }
    }

    vsync_display_id_update_deferred_ = false;
    SetVSyncDisplayID(display_id_, /*force_update=*/true);
  }
}

void ExternalBeginFrameSourceMac::DidReceiveNewCALayerParams() {
  has_swapped_frame_ = true;

  // Switch to External BeginFrame source in a few seconds if using a timer.
  // Don't wait for no NeedsBeginFrames if it's a nonstop rendering.
  if (!display_link_mac_ && vsync_display_id_update_deferred_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ExternalBeginFrameSourceMac::UpdateDeferredVSyncDisplayIfNeeded,
            weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(2));
  }
}

void ExternalBeginFrameSourceMac::UpdateDeferredVSyncDisplayIfNeeded() {
  if (!vsync_display_id_update_deferred_ || !has_swapped_frame_) {
    return;
  }

  vsync_display_id_update_deferred_ = false;
  SetVSyncDisplayID(display_id_, /*force_update=*/true);
}

void ExternalBeginFrameSourceMac::SetVSyncDisplayID(int64_t display_id,
                                                    bool force_update) {
  TRACE_EVENT2("viz", "ExternalBeginFrameSourceMac::SetVSyncDisplayID",
               "display_id", display_id, "force_update", force_update);

  if (display_id_ == display_id && !force_update) {
    return;
  }

  // Forward the |display_id| to output surface for frame presentation.
  output_surface_->SetVSyncDisplayID(display_id, force_update);

  // Remove the current callback from display_link_mac_ or from the timer.
  if (needs_begin_frames_ || vsync_callback_mac_) {
    StopBeginFrame(/*force_stop=*/true);
  }
  vsync_callback_keep_alive_counter_ = 0;

  // Remove the old DisplayLinkMac.
  display_link_mac_.reset();

  display_id_ = display_id;

  // Get DisplayLinkMac with the new CGDirectDisplayID.
  display_link_mac_ = GetForDisplay(display_id);

  // For debugging only. Use the timer for BeginFrameSource.
  if (base::FeatureList::IsEnabled(kForceMacVSyncTimerForDebugging)) {
    display_link_mac_.reset();
  }

  if (display_link_mac_) {
    // if DisplayLink fails, the same time will continue to run. Now remove the
    // existing time after switching to DisplayLink successfully.
    if (time_source_) {
      time_source_->SetActive(/*active=*/false);
      time_source_->SetClient(nullptr);
      time_source_.reset();
    }

    preferred_interval_ = min_refresh_interval_ = GetMinimumFrameInterval();
    VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                       << "::SetVSyncDisplayID: " << display_id_
                       << ", min_refresh_interval: " << min_refresh_interval_;

    display_link_mac_->GetRefreshIntervalRange(
        min_refresh_interval_, max_refresh_interval_, granularity_);

    if (update_vsync_params_callback_) {
      update_vsync_params_callback_.Run(display_link_mac_->GetCurrentTime(),
                                        min_refresh_interval_);
    }

    DisplayLinkResult display_link_result =
        force_update ? DisplayLinkResult::kSuccessForcedUpdate
                     : DisplayLinkResult::kSuccess;
    RecordDisplayLinkCreateStatus(display_link_result);
  } else {
    DisplayLinkResult display_link_result =
        display_id < 0
            ? DisplayLinkResult::kFailedInvalidDisplayId
            : (force_update
                   ? DisplayLinkResult::kFailedForcedUpdateCreateDisplayLink
                   : DisplayLinkResult::kFailedCreateDisplayLink);
    RecordDisplayLinkCreateStatus(display_link_result);

    preferred_interval_ = min_refresh_interval_ = max_refresh_interval_ =
        GetMinimumFrameInterval();
    CreateDelayBasedTimeSourceIfNeeded();

    if (update_vsync_params_callback_) {
      update_vsync_params_callback_.Run(base::TimeTicks::Now(),
                                        min_refresh_interval_);
    }

    DLOG(WARNING) << "Switch to DelayBasedTimeSource. DisplayID "
                  << display_id_;
    TRACE_EVENT("viz", "ExternalBeginFrameSourceMac DisplayLinkMac failed.");

    // TODO: Set hw_takes_any_refresh_rate_ to true for Timer.
    hw_takes_any_refresh_rate_ = false;
    if (multiple_hw_refresh_rates_callback_) {
      multiple_hw_refresh_rates_callback_.Run(false);
    }
  }

  if (needs_begin_frames_) {
    StartBeginFrame();
  }
}

void ExternalBeginFrameSourceMac::RefreshRateChangedOnSameDisplay() {
  if (!ui::DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
    return;
  }

  // Forward the notification to output surface for frame presentation.
  output_surface_->RefreshRateChangedOnSameDisplay();

  if (display_link_mac_ && !display_link_mac_->NotifyEventAndCheckValidity()) {
    // Recreate a new one.
    SetVSyncDisplayID(display_id_, /*force_update=*/true);
  }
}
void ExternalBeginFrameSourceMac::StartBeginFrame() {
  if (display_link_mac_) {
    if (vsync_callback_mac_) {
      // The callback is already registered and running (likely in keep-alive
      // mode). Reset the counter and return.
      vsync_callback_keep_alive_counter_ = 0;
      return;
    }
    vsync_callback_keep_alive_counter_ = 0;
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
  time_source_->SetActive(/*active=*/true);
}

void ExternalBeginFrameSourceMac::StopBeginFrame(bool force_stop) {
  if (display_link_mac_) {
    CHECK(vsync_callback_mac_);
    vsyncs_to_skip_ = 0;
    // If not force_update, wait until the keep-alive counter has reached
    // `kMaxKeepAliveCount` in `OnDisplayLinkCallback()`.
    if (force_stop) {
      // Remove and unregister VSyncCallbackMac immediately after display
      // switch.
      vsync_callback_mac_.reset();
    }
    return;
  }

  // Stop the timer.
  CHECK(time_source_);
  time_source_->SetActive(/*active=*/false);
}

void ExternalBeginFrameSourceMac::OnNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("viz", "ExternalBeginFrameSourceMac::OnNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  if (needs_begin_frames_ == needs_begin_frames) {
    return;
  }

  if (needs_begin_frames && first_needs_begin_frames_time_.is_null()) {
    first_needs_begin_frames_time_ = base::TimeTicks::Now();
  }

  needs_begin_frames_ = needs_begin_frames;
  just_started_begin_frame_ = true;

  if (needs_begin_frames_) {
    StartBeginFrame();
  } else {
    StopBeginFrame(/*force_stop=*/false);

    UpdateDeferredVSyncDisplayIfNeeded();
  }
}

// Called on the Viz thread.
void ExternalBeginFrameSourceMac::OnDisplayLinkCallback(
    ui::VSyncParamsMac params) {
  RecordFirstFrameHistograms(/*is_timer=*/false);
  // If we have reached `kMaxKeepAliveCount` consecutive callbacks without
  // needing a begin frame, stop the display link.
  if (!needs_begin_frames_) {
    vsync_callback_keep_alive_counter_++;
    if (vsync_callback_keep_alive_counter_ >= kMaxKeepAliveCount) {
      vsync_callback_mac_.reset();
    }
    return;
  }
  vsync_callback_keep_alive_counter_ = 0;

  if (vsyncs_to_skip_ > 0) {
    TRACE_EVENT_INSTANT(
        "viz",
        "ExternalBeginFrameSourceMac::OnDisplayLinkCallback - skip_vsync");
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
                                          : min_refresh_interval_;
  }

  auto callback_delay =
      params.callback_times_valid ? (now - frame_time) : base::Microseconds(0);
  auto callback_timebase_to_display =
      params.display_times_valid ? (params.display_timebase - frame_time)
                                 : base::Microseconds(0);
  TRACE_EVENT2("viz", "ExternalBeginFrameSourceMac::OnDisplayLinkCallback",
               "callback_timebase_to_display",
               callback_timebase_to_display.InMicroseconds(), "callback_delay",
               callback_delay.InMicroseconds());
  if (base::ShouldRecordSubsampledMetric(0.001)) {
    RecordVSyncCallbackDelay(callback_delay);
  }

  bool display_link_frame_interval_changed =
      !AlmostEqual(min_refresh_interval_, interval);

  min_refresh_interval_ = interval;

  // If the preferred frame interval is not equal to |min_refresh_interval_|,
  // vsync_subsampling_factor_ is bigger than 1.
  vsyncs_to_skip_ = vsync_subsampling_factor_ - 1;
  interval *= vsync_subsampling_factor_;

  // |min_refresh_interval_| here in BeginFrameArgs is the unthrottled minimum
  // refresh interval the display can support. Be careful not to set it to a
  // throttled frame interval as it can cause a recursive loop where it halves
  // the frame rate repeatedly until it becomes zero.
  OnBeginFrame(begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, frame_time + interval, interval,
      min_refresh_interval_));

  // Notify Display FrameRateDecider of the frame interval change.
  if (display_link_frame_interval_changed) {
    DCHECK(update_vsync_params_callback_);
    VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                       << "::OnDisplayLinkCallback: " << display_id_
                       << ", min_refresh_interval_: " << min_refresh_interval_;
    update_vsync_params_callback_.Run(frame_time, min_refresh_interval_);
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
      interval = min_refresh_interval_ * vsync_subsampling_factor_;
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
        source_id(), frame_time, frame_time + interval, interval,
        min_refresh_interval_);
  }

  return ExternalBeginFrameSource::GetMissedBeginFrameArgs(obs);
}

// Timer callbacks when DisplayLink is not available.
void ExternalBeginFrameSourceMac::OnTimerTick() {
  if (display_link_mac_) {
    return;
  }

  RecordFirstFrameHistograms(/*is_timer=*/true);
  if (!needs_begin_frames_) {
    return;
  }
  DCHECK(time_source_);

  // See comments in DelayBasedBeginFrameSource::OnTimerTick regarding the
  // computation of `frame_time`.
  base::TimeTicks frame_time =
      std::max(time_source_->LastTickTime(),
               time_source_->NextTickTime() - time_source_->Interval());
  auto interval = time_source_->Interval();

  OnBeginFrame(begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, time_source_->NextTickTime(), interval,
      min_refresh_interval_));

  if (last_interval_ != interval) {
    DCHECK(update_vsync_params_callback_);
    update_vsync_params_callback_.Run(frame_time, interval);
  }

  last_frame_time_ = frame_time;
  last_interval_ = interval;
}

void ExternalBeginFrameSourceMac::SetPreferredInterval(
    base::TimeDelta interval) {
  if (interval.is_zero()) {
    interval = GetMinimumFrameInterval();
  }
  preferred_interval_ = interval;

  VLOG(kOutputLevel) << "ExternalBeginFrameSourceMac(" << this << ")"
                     << "::SetPreferredInterval: ID: " << display_id_
                     << ", Interval: " << interval;

  if (!display_link_mac_) {
    DCHECK(time_source_);
    time_source_->SetTimebaseAndInterval(last_frame_time_, interval);
    return;
  }

  // Here is for the monitor with a fixed refresh rate.
  // Cap the preferred refresh interval if it's out of the range.
  base::TimeDelta adjusted_interval = interval;
  if (interval < min_refresh_interval_) {
    adjusted_interval = min_refresh_interval_;
  } else if (interval > kMaxSupportedFrameInterval &&
             !AlmostEqual(interval, min_refresh_interval_)) {
    adjusted_interval = kMaxSupportedFrameInterval;
  }

  // Keep |vsyncs_to_skip_| unchanged so it will complete the whole frame
  // interal.

  vsync_subsampling_factor_ =
      adjusted_interval.IntDiv((min_refresh_interval_ - kDeltaAlmostEqual));

  TRACE_EVENT1("gpu", "ExternalBeginFrameSourceMac::SetPreferredInterval",
               "vsync_subsampling_factor", vsync_subsampling_factor_);
}

scoped_refptr<ui::DisplayLinkMac> ExternalBeginFrameSourceMac::GetForDisplay(
    int64_t display_id) {
  // Directly delegates to ui::DisplayLinkMac::GetForDisplay. Overridden by
  // ExternalBeginFrameSourceMacWrapper in unit tests to inject a mock
  // DisplayLink.
  return ui::DisplayLinkMac::GetForDisplay(display_id);
}

base::TimeDelta ExternalBeginFrameSourceMac::GetMinimumFrameInterval() {
  if (display_link_mac_) {
    return display_link_mac_->GetRefreshInterval();
  }

  if (ui::DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
    return ui::DisplayLinkMac::GetScreenDefaultRefreshInterval(display_id_);
  } else {
    return BeginFrameArgs::DefaultInterval();
  }
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

  if (min_refresh_interval_ > kMaxSupportedFrameInterval &&
      min_refresh_interval_ == max_refresh_interval_) {
    VLOG(kOutputLevel) << "min_refresh_interval_: " << min_refresh_interval_;
    return {min_refresh_interval_};
  }

  base::flat_set<base::TimeDelta> supported_intervals;

  // Can only do fixed refresh rates. Now try to implement 2^n refresh
  // rates by skipping VSyncs.
  min_refresh_interval_ = GetMinimumFrameInterval();
  base::TimeDelta interval = min_refresh_interval_;
  while (interval <= kMaxSupportedFrameInterval) {
    VLOG(kOutputLevel) << interval;
    supported_intervals.insert(interval);
    interval *= 2;
  }

  // FrameIntervalDecider::UpdateSettings() requires non-empty supported
  // intervals for FixedIntervalSettings.
  if (supported_intervals.empty()) {
    supported_intervals.insert(min_refresh_interval_);
  }

  return supported_intervals;
}

void ExternalBeginFrameSourceMac::RecordFirstFrameHistograms(bool is_timer) {
  if (!first_callback_time_.is_null()) {
    return;
  }
  first_callback_time_ = base::TimeTicks::Now();

  static constexpr char kTimeFromConstruction[] =
      "Viz.ExternalBeginFrameSourceMac.TimeFromConstructionToFirstFrame";
  static constexpr char kTimeFromFirstNeedsBeginFrames[] =
      "Viz.ExternalBeginFrameSourceMac."
      "TimeFromFirstNeedsBeginFramesToFirstFrame";

  const char* suffix = is_timer ? ".Timer" : ".DisplayLink";

  base::TimeDelta construction_to_first_frame =
      first_callback_time_ - create_time_;
  base::TimeDelta min = base::Milliseconds(1);
  base::TimeDelta max = base::Minutes(1);
  size_t bucket_count = 50;

  base::UmaHistogramCustomTimes(kTimeFromConstruction,
                                construction_to_first_frame, min, max,
                                bucket_count);
  base::UmaHistogramCustomTimes(base::StrCat({kTimeFromConstruction, suffix}),
                                construction_to_first_frame, min, max,
                                bucket_count);

  if (!first_needs_begin_frames_time_.is_null()) {
    base::TimeDelta needs_begin_frames_to_first_frame =
        first_callback_time_ - first_needs_begin_frames_time_;
    base::UmaHistogramCustomTimes(kTimeFromFirstNeedsBeginFrames,
                                  needs_begin_frames_to_first_frame, min, max,
                                  bucket_count);
    base::UmaHistogramCustomTimes(
        base::StrCat({kTimeFromFirstNeedsBeginFrames, suffix}),
        needs_begin_frames_to_first_frame, min, max, bucket_count);
  }
}

void ExternalBeginFrameSourceMac::OnSuspend() {
  if (first_callback_time_.is_null()) {
    first_callback_time_ = base::TimeTicks::Max();
  }
}

void ExternalBeginFrameSourceMac::OnResume() {
  if (display_link_mac_ && !display_link_mac_->NotifyEventAndCheckValidity()) {
    // Recreate a new one.
    SetVSyncDisplayID(display_id_, /*force_update=*/true);
  }
}
}  // namespace viz

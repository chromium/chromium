// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"

namespace viz {

namespace {
// kDoubleTickDivisor prevents the SyntheticBFS from sending BeginFrames too
// often to an observer.
constexpr double kDoubleTickDivisor = 2.0;

base::AtomicSequenceNumber g_next_source_id;

// Generates a source_id with upper 32 bits from |restart_id| and lower 32 bits
// from an atomic sequence.
uint64_t GenerateSourceId(uint32_t restart_id) {
  return static_cast<uint64_t>(restart_id) << 32 | g_next_source_id.GetNext();
}

// Notifies the observer of the BeginFrame. If the BeginFrame is a
// animate_only BeginFrame, the observer may not be notified of the
// BeginFrame.
void FilterAndIssueBeginFrame(BeginFrameObserver* observer,
                              const BeginFrameArgs& args) {
  if (args.animate_only && !observer->WantsAnimateOnlyBeginFrames())
    return;
  observer->OnBeginFrame(args);
}

// Checks |args| for continuity with our last args.  It is possible that the
// source in which |args| originate changes, or that our hookup to this source
// changes, so we have to check for continuity.  See also
// https://crbug.com/690127 for what may happen without this check.
bool CheckBeginFrameContinuity(BeginFrameObserver* observer,
                               const BeginFrameArgs& args) {
  const BeginFrameArgs& last_args = observer->LastUsedBeginFrameArgs();
  if (!last_args.IsValid() || (args.frame_time > last_args.frame_time)) {
    DCHECK(!last_args.frame_id.IsNextInSequenceTo(args.frame_id))
        << "current " << args.ToString() << ", last " << last_args.ToString();
    return true;
  }
  return false;
}
}  // namespace

// BeginFrameObserver -----------------------------------------------------
bool BeginFrameObserver::IsRoot() const {
  return false;
}

// BeginFrameObserverBase -------------------------------------------------
BeginFrameObserverBase::BeginFrameObserverBase() = default;

BeginFrameObserverBase::~BeginFrameObserverBase() = default;

const BeginFrameArgs& BeginFrameObserverBase::LastUsedBeginFrameArgs() const {
  return last_begin_frame_args_;
}

bool BeginFrameObserverBase::WantsAnimateOnlyBeginFrames() const {
  return wants_animate_only_begin_frames_;
}

void BeginFrameObserverBase::OnBeginFrame(const BeginFrameArgs& args) {
  DCHECK(args.IsValid());
  DCHECK_GE(args.frame_time, last_begin_frame_args_.frame_time);
  DCHECK(!last_begin_frame_args_.frame_id.IsNextInSequenceTo(args.frame_id))
      << "current " << args.ToString() << ", last "
      << last_begin_frame_args_.ToString();
  bool used = OnBeginFrameDerivedImpl(args);
  if (used) {
    last_begin_frame_args_ = args;
  } else {
    ++dropped_begin_frame_args_;
  }
}

void BeginFrameObserverBase::AsProtozeroInto(
    perfetto::EventContext& ctx,
    perfetto::protos::pbzero::BeginFrameObserverStateV2* state) const {
  state->set_dropped_begin_frame_args(dropped_begin_frame_args_);

  last_begin_frame_args_.AsProtozeroInto(ctx,
                                         state->set_last_begin_frame_args());
}

BeginFrameArgs
BeginFrameSource::BeginFrameArgsGenerator::GenerateBeginFrameArgs(
    uint64_t source_id,
    base::TimeTicks frame_time,
    base::TimeTicks deadline,
    base::TimeDelta vsync_interval) {
  uint64_t sequence_number =
      next_sequence_number_ +
      EstimateTickCountsBetween(frame_time, next_expected_frame_time_,
                                vsync_interval);
  next_expected_frame_time_ = deadline;
  next_sequence_number_ = sequence_number + 1;
  return BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, source_id,
                                sequence_number, frame_time, deadline,
                                vsync_interval, BeginFrameArgs::NORMAL);
}

uint64_t BeginFrameSource::BeginFrameArgsGenerator::EstimateTickCountsBetween(
    base::TimeTicks frame_time,
    base::TimeTicks next_expected_frame_time,
    base::TimeDelta vsync_interval) {
  if (next_expected_frame_time.is_null())
    return 0;

  // kErrorMarginIntervalPct used to determine what percentage of the time tick
  // interval should be used as a margin of error when comparing times to
  // deadlines.
  constexpr double kErrorMarginIntervalPct = 0.05;
  base::TimeDelta error_margin = vsync_interval * kErrorMarginIntervalPct;
  int ticks_since_estimated_frame_time = base::ClampFloor(
      (frame_time + error_margin - next_expected_frame_time) / vsync_interval);
  return std::max(0, ticks_since_estimated_frame_time);
}

// BeginFrameSource -------------------------------------------------------

// static
constexpr uint32_t BeginFrameSource::kNotRestartableId;

BeginFrameSource::BeginFrameSource(uint32_t restart_id)
    : source_id_(GenerateSourceId(restart_id)) {}

BeginFrameSource::~BeginFrameSource() = default;

void BeginFrameSource::SetIsGpuBusy(bool busy) {
  if (is_gpu_busy_ == busy)
    return;
  is_gpu_busy_ = busy;
  if (is_gpu_busy_) {
    DCHECK_EQ(gpu_busy_response_state_, GpuBusyThrottlingState::kIdle);
    gpu_busy_start_time_ = base::TimeTicks::Now();
    return;
  }

  const bool was_throttled =
      gpu_busy_response_state_ == GpuBusyThrottlingState::kThrottled;
  gpu_busy_response_state_ = GpuBusyThrottlingState::kIdle;
  if (was_throttled) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Viz.FrameSink.GpuBusyDuration",
        base::TimeTicks::Now() - gpu_busy_start_time_, base::Microseconds(1),
        base::Seconds(5), /*bucket_count=*/100);
    OnGpuNoLongerBusy();
  }
}

bool BeginFrameSource::RequestCallbackOnGpuAvailable() {
  if (!is_gpu_busy_) {
    DCHECK_EQ(gpu_busy_response_state_, GpuBusyThrottlingState::kIdle);
    return false;
  }

  switch (gpu_busy_response_state_) {
    case GpuBusyThrottlingState::kIdle:
        gpu_busy_response_state_ =
            GpuBusyThrottlingState::kOneBeginFrameAfterBusySent;
        return false;
    case GpuBusyThrottlingState::kOneBeginFrameAfterBusySent:
      gpu_busy_response_state_ = GpuBusyThrottlingState::kThrottled;
      return true;
    case GpuBusyThrottlingState::kThrottled:
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

void BeginFrameSource::AsProtozeroInto(
    perfetto::EventContext&,
    perfetto::protos::pbzero::BeginFrameSourceStateV2* state) const {
  // The lower 32 bits of source_id are the interesting piece of |source_id_|.
  state->set_source_id(static_cast<uint32_t>(source_id_));
}

#if BUILDFLAG(IS_MAC)
void BeginFrameSource::RecordBeginFrameSourceAccuracy(base::TimeDelta delta) {
  total_delta_ += delta.magnitude();
  frames_since_last_recording_++;

  // Emit the histogram every 3600 frames.
  constexpr int kFramesToEmitHistogram = 3600;
  if (frames_since_last_recording_ < kFramesToEmitHistogram) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Viz.BeginFrameSource.Accuracy.AverageDelta2",
      total_delta_ / kFramesToEmitHistogram,
      /*min=*/base::Microseconds(100),
      /*max=*/base::Milliseconds(33), /*bucket_count=*/30);
  frames_since_last_recording_ = 0;
  total_delta_ = base::TimeDelta();
}
#endif

// StubBeginFrameSource ---------------------------------------------------
StubBeginFrameSource::StubBeginFrameSource()
    : BeginFrameSource(kNotRestartableId) {}

// SyntheticBeginFrameSource ----------------------------------------------
SyntheticBeginFrameSource::SyntheticBeginFrameSource(uint32_t restart_id)
    : BeginFrameSource(restart_id) {}

SyntheticBeginFrameSource::~SyntheticBeginFrameSource() = default;

// BackToBackBeginFrameSource ---------------------------------------------
BackToBackBeginFrameSource::BackToBackBeginFrameSource(
    std::unique_ptr<DelayBasedTimeSource> time_source)
    : SyntheticBeginFrameSource(kNotRestartableId),
      time_source_(std::move(time_source)),
      next_sequence_number_(BeginFrameArgs::kStartingFrameNumber) {
  time_source_->SetClient(this);
  // The time_source_ ticks immediately, so we SetActive(true) for a single
  // tick when we need it, and keep it as SetActive(false) otherwise.
  time_source_->SetTimebaseAndInterval(base::TimeTicks(), base::TimeDelta());
}

BackToBackBeginFrameSource::~BackToBackBeginFrameSource() = default;

void BackToBackBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!base::Contains(observers_, obs));
  observers_.insert(obs);
  pending_begin_frame_observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  time_source_->SetActive(true);
}

void BackToBackBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(base::Contains(observers_, obs));
  observers_.erase(obs);
  pending_begin_frame_observers_.erase(obs);
  if (pending_begin_frame_observers_.empty()) {
    time_source_->SetActive(false);
  }
}

void BackToBackBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs) {
  if (base::Contains(observers_, obs)) {
    pending_begin_frame_observers_.insert(obs);
    time_source_->SetActive(true);
  }
}

void BackToBackBeginFrameSource::OnGpuNoLongerBusy() {
  OnTimerTick();
}

void BackToBackBeginFrameSource::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (interval.is_zero()) {
    interval = BeginFrameArgs::DefaultInterval();
  }
  vsync_interval_ = interval;
}

void BackToBackBeginFrameSource::SetMaxVrrInterval(
    const std::optional<base::TimeDelta>& max_vrr_interval) {
  DCHECK(!max_vrr_interval.has_value() || max_vrr_interval->is_positive());
  max_vrr_interval_ = max_vrr_interval;
}

void BackToBackBeginFrameSource::OnTimerTick() {
  if (RequestCallbackOnGpuAvailable())
    return;
  if (!time_source_->Active()) {
    return;
  }
  base::TimeTicks frame_time = time_source_->LastTickTime();
  base::TimeDelta interval = max_vrr_interval_.value_or(vsync_interval_);
  BeginFrameArgs args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), next_sequence_number_, frame_time,
      frame_time + interval, interval, BeginFrameArgs::NORMAL);
  next_sequence_number_++;

  // This must happen after getting the LastTickTime() from the time source.
  time_source_->SetActive(false);

  base::flat_set<raw_ptr<BeginFrameObserver, CtnExperimental>>
      pending_observers;
  pending_observers.swap(pending_begin_frame_observers_);
  DCHECK(!pending_observers.empty());
  for (BeginFrameObserver* obs : pending_observers)
    FilterAndIssueBeginFrame(obs, args);
}

// DelayBasedBeginFrameSource ---------------------------------------------
DelayBasedBeginFrameSource::DelayBasedBeginFrameSource(
    std::unique_ptr<DelayBasedTimeSource> time_source,
    uint32_t restart_id)
    : SyntheticBeginFrameSource(restart_id),
      time_source_(std::move(time_source)) {
  time_source_->SetClient(this);
  last_vsync_interval_ = BeginFrameArgs::DefaultInterval();
}

DelayBasedBeginFrameSource::~DelayBasedBeginFrameSource() {
  if (max_vrr_interval_.has_value()) {
    UMA_HISTOGRAM_COUNTS_10M("Viz.BeginFrameSource.VrrFrameCount",
                             vrr_tick_count_);
  }
}

void DelayBasedBeginFrameSource::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = BeginFrameArgs::DefaultInterval();
  }

  last_timebase_ = timebase;
  time_source_->SetTimebaseAndInterval(timebase, interval);
}

BeginFrameArgs DelayBasedBeginFrameSource::CreateBeginFrameArgs(
    base::TimeTicks frame_time) {
  base::TimeDelta interval =
      max_vrr_interval_.value_or(time_source_->Interval());
  // Use `Next-` instead of `LastTickTime` because it is snapped to
  // `last_timebase_`
  base::TimeTicks deadline =
      time_source_->NextTickTime() - time_source_->Interval() + interval;
  return begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, deadline, interval);
}

void DelayBasedBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!base::Contains(observers_, obs));

  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  SetActive(true);

  // Missed args should correspond to |last_begin_frame_args_| (particularly,
  // have the same sequence number) if |last_begin_frame_args_| still correspond
  // to the last time the time source should have ticked. This may not be the
  // case if the time source was inactive before AddObserver() was called. In
  // such a case, we create new args with a new sequence number only if
  // sufficient time has passed since the last tick.
  base::TimeTicks last_or_missed_tick_time =
      time_source_->NextTickTime() - time_source_->Interval();
  const base::TimeDelta double_tick_margin =
      max_vrr_interval_.has_value()
          ? base::TimeDelta()
          : time_source_->Interval() / kDoubleTickDivisor;
  if (!last_begin_frame_args_.IsValid() ||
      last_or_missed_tick_time >
          last_begin_frame_args_.frame_time + double_tick_margin) {
    last_begin_frame_args_ = CreateBeginFrameArgs(last_or_missed_tick_time);
  }
  BeginFrameArgs missed_args = last_begin_frame_args_;
  missed_args.type = BeginFrameArgs::MISSED;
  IssueBeginFrameToObserver(obs, missed_args);
}

void DelayBasedBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(base::Contains(observers_, obs));

  observers_.erase(obs);
  if (observers_.empty())
    SetActive(false);
}

void DelayBasedBeginFrameSource::OnGpuNoLongerBusy() {
  OnTimerTick();
}

void DelayBasedBeginFrameSource::SetMaxVrrInterval(
    const std::optional<base::TimeDelta>& max_vrr_interval) {
  DCHECK(!max_vrr_interval.has_value() || max_vrr_interval->is_positive());

  // If VRR is deactivating, record the number of frames produced.
  if (max_vrr_interval_.has_value() && !max_vrr_interval.has_value()) {
    UMA_HISTOGRAM_COUNTS_10M("Viz.BeginFrameSource.VrrFrameCount",
                             vrr_tick_count_);
    vrr_tick_count_ = 0;
  }

  max_vrr_interval_ = max_vrr_interval;
}

void DelayBasedBeginFrameSource::OnTimerTick() {
  if (RequestCallbackOnGpuAvailable())
    return;
  // In case of gpu back pressure LastTickTime can fall behind, and in case of
  // a change in vsync using (NextTickTime-interval) could be before
  // LastTickTime, so should use the latest of the two.
  last_begin_frame_args_ = CreateBeginFrameArgs(
      std::max(time_source_->LastTickTime(),
               time_source_->NextTickTime() - time_source_->Interval()));
  TRACE_EVENT2(
      "viz", "DelayBasedBeginFrameSource::OnTimerTick", "frame_time",
      last_begin_frame_args_.frame_time.since_origin().InMicroseconds(),
      "interval", last_begin_frame_args_.interval.InMicroseconds());
  if (max_vrr_interval_.has_value()) {
    vrr_tick_count_++;
  }
  base::flat_set<raw_ptr<BeginFrameObserver, CtnExperimental>> observers(
      observers_);
  for (BeginFrameObserver* obs : observers) {
    IssueBeginFrameToObserver(obs, last_begin_frame_args_);
  }
  last_vsync_interval_ = time_source_->Interval();
}

void DelayBasedBeginFrameSource::IssueBeginFrameToObserver(
    BeginFrameObserver* obs,
    const BeginFrameArgs& args) {
  BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();

  // * If a FrameSink is throttled, |last_args.interval| is the throttled
  //   interval (e.g. 50ms) while the frame_time delta is still the actual
  //   vsync (OnTimerTick) interval.
  // * If the vsync (OnTimerTick) interval is throttled, at the first tick after
  //   throttling, the |args.interval| is updated to the throttled interval
  //   while the frame_time delta is not.
  //
  // Both cases can cause the double tick check below to fail and an unexpected
  // frame drop. To avoid this, we use the cached |last_vsync_interval_| here.
  auto interval_for_margin =
      base::FeatureList::IsEnabled(features::kLastVSyncArgsKillswitch)
          ? args.interval
          : last_vsync_interval_;
  const base::TimeDelta double_tick_margin =
      max_vrr_interval_.has_value() ? base::TimeDelta()
                                    : interval_for_margin / kDoubleTickDivisor;
  if (!last_args.IsValid() ||
      (args.frame_time > last_args.frame_time + double_tick_margin)) {
    if (args.type == BeginFrameArgs::MISSED) {
      DCHECK(!last_args.frame_id.IsNextInSequenceTo(args.frame_id))
          << "missed " << args.ToString() << ", last " << last_args.ToString();
    }
    FilterAndIssueBeginFrame(obs, args);
  }
}

void DelayBasedBeginFrameSource::SetActive(bool active) {
  if (time_source_->Active() == active)
    return;
  time_source_->SetActive(active);
}

// ExternalBeginFrameSource -----------------------------------------------
ExternalBeginFrameSource::ExternalBeginFrameSource(
    ExternalBeginFrameSourceClient* client,
    uint32_t restart_id)
    : BeginFrameSource(restart_id), client_(client) {
  DCHECK(client_);
}

ExternalBeginFrameSource::~ExternalBeginFrameSource() {
  DCHECK(observers_.empty());
}

void ExternalBeginFrameSource::AsProtozeroInto(
    perfetto::EventContext& ctx,
    perfetto::protos::pbzero::BeginFrameSourceStateV2* state) const {
  BeginFrameSource::AsProtozeroInto(ctx, state);

  state->set_paused(paused_);
  state->set_num_observers(observers_.size());
  last_begin_frame_args_.AsProtozeroInto(ctx,
                                         state->set_last_begin_frame_args());
}

void ExternalBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!base::Contains(observers_, obs));

  if (observers_.empty()) {
    client_->OnNeedsBeginFrames(true);
  }

  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(paused_);

  // Send a MISSED begin frame if necessary.
  BeginFrameArgs missed_args = GetMissedBeginFrameArgs(obs);
  if (missed_args.IsValid()) {
    DCHECK_EQ(BeginFrameArgs::MISSED, missed_args.type);
    FilterAndIssueBeginFrame(obs, missed_args);
  }
}

void ExternalBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(base::Contains(observers_, obs));

  observers_.erase(obs);
  if (observers_.empty()) {
    client_->OnNeedsBeginFrames(false);
  }
}

void ExternalBeginFrameSource::OnGpuNoLongerBusy() {
  OnBeginFrame(pending_begin_frame_args_);
  pending_begin_frame_args_ = BeginFrameArgs();
}

void ExternalBeginFrameSource::OnSetBeginFrameSourcePaused(bool paused) {
  if (paused_ == paused)
    return;
  paused_ = paused;
  base::flat_set<raw_ptr<BeginFrameObserver, CtnExperimental>> observers(
      observers_);
  for (BeginFrameObserver* obs : observers) {
    obs->OnBeginFrameSourcePausedChanged(paused_);
  }
}

void ExternalBeginFrameSource::OnBeginFrame(const BeginFrameArgs& args) {
  // Ignore out of order begin frames because of layer tree frame sink being
  // recreated.
  if (last_begin_frame_args_.IsValid() &&
      (args.frame_time <= last_begin_frame_args_.frame_time ||
       (args.frame_id.source_id == last_begin_frame_args_.frame_id.source_id &&
        args.frame_id.sequence_number <=
            last_begin_frame_args_.frame_id.sequence_number)))
    return;

  if (RequestCallbackOnGpuAvailable()) {
    pending_begin_frame_args_ = args;
    return;
  }

  TRACE_EVENT2("viz,input.scrolling", "ExternalBeginFrameSource::OnBeginFrame",
               "frame_time", args.frame_time.since_origin().InMicroseconds(),
               "interval", args.interval.InMicroseconds());

  last_begin_frame_args_ = args;
  base::flat_set<raw_ptr<BeginFrameObserver, CtnExperimental>> observers(
      observers_);

  // Process non-root observers.
  // TODO(ericrk): Remove root/non-root handling once a better workaround
  // exists. https://crbug.com/947717
  for (BeginFrameObserver* obs : observers) {
    if (obs->IsRoot())
      continue;
    if (!CheckBeginFrameContinuity(obs, args))
      continue;
    FilterAndIssueBeginFrame(obs, args);
  }
  // Process root observers.
  for (BeginFrameObserver* obs : observers) {
    if (!obs->IsRoot())
      continue;
    if (!CheckBeginFrameContinuity(obs, args))
      continue;
    FilterAndIssueBeginFrame(obs, args);
  }
}

BeginFrameArgs ExternalBeginFrameSource::GetMissedBeginFrameArgs(
    BeginFrameObserver* obs) {
  if (!last_begin_frame_args_.IsValid())
    return BeginFrameArgs();
  if (!CheckBeginFrameContinuity(obs, last_begin_frame_args_))
    return BeginFrameArgs();

  BeginFrameArgs missed_args = last_begin_frame_args_;
  missed_args.type = BeginFrameArgs::MISSED;
  return missed_args;
}

base::TimeDelta ExternalBeginFrameSource::GetMaximumRefreshFrameInterval() {
  return BeginFrameArgs::DefaultInterval();
}

base::flat_set<base::TimeDelta>
ExternalBeginFrameSource::GetSupportedFrameIntervals(base::TimeDelta interval) {
  return {interval, interval * 2};
}

}  // namespace viz

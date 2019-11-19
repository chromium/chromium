// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include <stddef.h>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"

namespace viz {

namespace {
// kDoubleTickDivisor prevents the SyntheticBFS from sending BeginFrames too
// often to an observer.
constexpr double kDoubleTickDivisor = 2.0;

// kErrorMarginIntervalPct used to determine what percentage of the time tick
// interval should be used as a margin of error when comparing times to
// deadlines.
constexpr double kErrorMarginIntervalPct = 0.05;

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
    DCHECK((args.source_id != last_args.source_id) ||
           (args.sequence_number > last_args.sequence_number))
        << "current " << args.AsValue()->ToString() << ", last "
        << last_args.AsValue()->ToString();
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
  DCHECK(args.sequence_number > last_begin_frame_args_.sequence_number ||
         args.source_id != last_begin_frame_args_.source_id)
      << "current " << args.AsValue()->ToString() << ", last "
      << last_begin_frame_args_.AsValue()->ToString();
  bool used = OnBeginFrameDerivedImpl(args);
  if (used) {
    last_begin_frame_args_ = args;
  } else {
    ++dropped_begin_frame_args_;
  }
}

void BeginFrameObserverBase::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("dropped_begin_frame_args", dropped_begin_frame_args_);

  state->BeginDictionary("last_begin_frame_args");
  last_begin_frame_args_.AsValueInto(state);
  state->EndDictionary();
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
    return;
  }

  const bool was_throttled =
      gpu_busy_response_state_ == GpuBusyThrottlingState::kThrottled;
  gpu_busy_response_state_ = GpuBusyThrottlingState::kIdle;
  if (was_throttled)
    OnGpuNoLongerBusy();
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

  NOTREACHED();
  return false;
}

void BeginFrameSource::AsValueInto(
    base::trace_event::TracedValue* state) const {
  // The lower 32 bits of source_id are the interesting piece of |source_id_|.
  state->SetInteger("source_id", static_cast<uint32_t>(source_id_));
}

// StubBeginFrameSource ---------------------------------------------------
StubBeginFrameSource::StubBeginFrameSource()
    : BeginFrameSource(kNotRestartableId) {}

bool StubBeginFrameSource::IsThrottled() const {
  return true;
}

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
  if (pending_begin_frame_observers_.empty())
    time_source_->SetActive(false);
}

void BackToBackBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs) {
  if (base::Contains(observers_, obs)) {
    pending_begin_frame_observers_.insert(obs);
    time_source_->SetActive(true);
  }
}

bool BackToBackBeginFrameSource::IsThrottled() const {
  return false;
}

void BackToBackBeginFrameSource::OnGpuNoLongerBusy() {
  OnTimerTick();
}

void BackToBackBeginFrameSource::OnTimerTick() {
  if (RequestCallbackOnGpuAvailable())
    return;
  base::TimeTicks frame_time = time_source_->LastTickTime();
  base::TimeDelta default_interval = BeginFrameArgs::DefaultInterval();
  BeginFrameArgs args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), next_sequence_number_, frame_time,
      frame_time + default_interval, default_interval, BeginFrameArgs::NORMAL);
  next_sequence_number_++;

  // This must happen after getting the LastTickTime() from the time source.
  time_source_->SetActive(false);

  base::flat_set<BeginFrameObserver*> pending_observers;
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
      time_source_(std::move(time_source)),
      next_sequence_number_(BeginFrameArgs::kStartingFrameNumber) {
  time_source_->SetClient(this);
}

DelayBasedBeginFrameSource::~DelayBasedBeginFrameSource() = default;

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
  base::TimeDelta interval = time_source_->Interval();
  uint64_t sequence_number = next_sequence_number_;

  base::TimeDelta error_margin = interval * kErrorMarginIntervalPct;

  // We expect |sequence_number| to be the number for the frame at
  // |expected_frame_time|. We adjust this sequence number according to the
  // actual frame time in case it is later than expected.
  if (next_expected_frame_time_ != base::TimeTicks()) {
    // Add |error_margin| to round |frame_time| up to the next tick if it is
    // close to the end of an interval. This happens when a timebase is a bit
    // off because of an imperfect presentation timestamp that may be a bit
    // later than the beginning of the next interval.
    int ticks_since_estimated_frame_time =
        (frame_time + error_margin - next_expected_frame_time_) / interval;
    sequence_number += std::max(0, ticks_since_estimated_frame_time);
  }

  next_expected_frame_time_ = time_source_->NextTickTime();
  next_sequence_number_ = sequence_number + 1;

  return BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), sequence_number, frame_time,
      time_source_->NextTickTime(), interval, BeginFrameArgs::NORMAL);
}

void DelayBasedBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!base::Contains(observers_, obs));

  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  time_source_->SetActive(true);

  // Missed args should correspond to |last_begin_frame_args_| (particularly,
  // have the same sequence number) if |last_begin_frame_args_| still correspond
  // to the last time the time source should have ticked. This may not be the
  // case if the time source was inactive before AddObserver() was called. In
  // such a case, we create new args with a new sequence number only if
  // sufficient time has passed since the last tick.
  base::TimeTicks last_or_missed_tick_time =
      time_source_->NextTickTime() - time_source_->Interval();
  if (!last_begin_frame_args_.IsValid() ||
      last_or_missed_tick_time >
          last_begin_frame_args_.frame_time +
              last_begin_frame_args_.interval / kDoubleTickDivisor) {
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
    time_source_->SetActive(false);
}

bool DelayBasedBeginFrameSource::IsThrottled() const {
  return true;
}

void DelayBasedBeginFrameSource::OnGpuNoLongerBusy() {
  OnTimerTick();
}

void DelayBasedBeginFrameSource::OnTimerTick() {
  if (RequestCallbackOnGpuAvailable())
    return;
  last_begin_frame_args_ = CreateBeginFrameArgs(time_source_->LastTickTime());
  TRACE_EVENT2(
      "viz", "DelayBasedBeginFrameSource::OnTimerTick", "frame_time",
      last_begin_frame_args_.frame_time.since_origin().InMicroseconds(),
      "interval", last_begin_frame_args_.interval.InMicroseconds());
  base::flat_set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers)
    IssueBeginFrameToObserver(obs, last_begin_frame_args_);
}

void DelayBasedBeginFrameSource::IssueBeginFrameToObserver(
    BeginFrameObserver* obs,
    const BeginFrameArgs& args) {
  BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
  if (!last_args.IsValid() ||
      (args.frame_time >
       last_args.frame_time + args.interval / kDoubleTickDivisor)) {
    if (args.type == BeginFrameArgs::MISSED) {
      DCHECK(args.sequence_number > last_args.sequence_number ||
             args.source_id != last_args.source_id)
          << "missed " << args.AsValue()->ToString() << ", last "
          << last_args.AsValue()->ToString();
    }
    FilterAndIssueBeginFrame(obs, args);
  }
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

void ExternalBeginFrameSource::AsValueInto(
    base::trace_event::TracedValue* state) const {
  BeginFrameSource::AsValueInto(state);

  state->SetBoolean("paused", paused_);
  state->SetInteger("num_observers", observers_.size());

  state->BeginDictionary("last_begin_frame_args");
  last_begin_frame_args_.AsValueInto(state);
  state->EndDictionary();
}

void ExternalBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(!base::Contains(observers_, obs));

  bool observers_was_empty = observers_.empty();
  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(paused_);
  if (observers_was_empty)
    client_->OnNeedsBeginFrames(true);

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
  if (observers_.empty())
    client_->OnNeedsBeginFrames(false);
}

bool ExternalBeginFrameSource::IsThrottled() const {
  return true;
}

void ExternalBeginFrameSource::OnGpuNoLongerBusy() {
  OnBeginFrame(pending_begin_frame_args_);
  pending_begin_frame_args_ = BeginFrameArgs();
}

void ExternalBeginFrameSource::OnSetBeginFrameSourcePaused(bool paused) {
  if (paused_ == paused)
    return;
  paused_ = paused;
  base::flat_set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers)
    obs->OnBeginFrameSourcePausedChanged(paused_);
}

void ExternalBeginFrameSource::OnBeginFrame(const BeginFrameArgs& args) {
  // Ignore out of order begin frames because of layer tree frame sink being
  // recreated.
  if (last_begin_frame_args_.IsValid() &&
      (args.frame_time <= last_begin_frame_args_.frame_time ||
       (args.source_id == last_begin_frame_args_.source_id &&
        args.sequence_number <= last_begin_frame_args_.sequence_number)))
    return;

  if (RequestCallbackOnGpuAvailable()) {
    pending_begin_frame_args_ = args;
    return;
  }

  TRACE_EVENT2(
      "viz", "ExternalBeginFrameSource::OnBeginFrame", "frame_time",
      last_begin_frame_args_.frame_time.since_origin().InMicroseconds(),
      "interval", last_begin_frame_args_.interval.InMicroseconds());

  last_begin_frame_args_ = args;
  base::flat_set<BeginFrameObserver*> observers(observers_);

  // Process non-root observers.
  // TODO(ericrk): Remove root/non-root handling once a better workaround
  // exists. https://crbug.com/947717
  for (auto* obs : observers) {
    if (obs->IsRoot())
      continue;
    if (!CheckBeginFrameContinuity(obs, args))
      continue;
    FilterAndIssueBeginFrame(obs, args);
  }
  // Process root observers.
  for (auto* obs : observers) {
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

}  // namespace viz

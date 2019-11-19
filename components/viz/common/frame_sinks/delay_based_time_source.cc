// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/delay_based_time_source.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace viz {

// The following methods correspond to the DelayBasedTimeSource that uses
// the base::TimeTicks::Now as the timebase.
DelayBasedTimeSource::DelayBasedTimeSource(
    base::SingleThreadTaskRunner* task_runner)
    : client_(nullptr),
      active_(false),
      timebase_(base::TimeTicks()),
      interval_(BeginFrameArgs::DefaultInterval()),
      last_tick_time_(base::TimeTicks() - interval_),
      next_tick_time_(base::TimeTicks()),
      task_runner_(task_runner) {}

DelayBasedTimeSource::~DelayBasedTimeSource() = default;

void DelayBasedTimeSource::SetActive(bool active) {
  TRACE_EVENT1("viz", "DelayBasedTimeSource::SetActive", "active", active);

  if (active == active_)
    return;

  active_ = active;

  if (active_) {
    PostNextTickTask(Now());
  } else {
    last_tick_time_ = base::TimeTicks();
    next_tick_time_ = base::TimeTicks();
    tick_closure_.Cancel();
  }
}

base::TimeDelta DelayBasedTimeSource::Interval() const {
  return interval_;
}

bool DelayBasedTimeSource::Active() const {
  return active_;
}

base::TimeTicks DelayBasedTimeSource::LastTickTime() const {
  return last_tick_time_;
}

base::TimeTicks DelayBasedTimeSource::NextTickTime() const {
  return next_tick_time_;
}

void DelayBasedTimeSource::OnTimerTick() {
  DCHECK(active_);

  last_tick_time_ = next_tick_time_;

  PostNextTickTask(Now());

  // Fire the tick.
  if (client_)
    client_->OnTimerTick();
}

void DelayBasedTimeSource::SetClient(DelayBasedTimeSourceClient* client) {
  client_ = client;
}

void DelayBasedTimeSource::SetTimebaseAndInterval(base::TimeTicks timebase,
                                                  base::TimeDelta interval) {
  interval_ = interval;
  timebase_ = timebase;
}

base::TimeTicks DelayBasedTimeSource::Now() const {
  return base::TimeTicks::Now();
}

// This code tries to achieve an average tick rate as close to interval_ as
// possible.  To do this, it has to deal with a few basic issues:
//   1. PostDelayedTask can delay only at a millisecond granularity. So, 16.666
//   has to posted as 16 or 17.
//   2. A delayed task may come back a bit late (a few ms), or really late
//   (frames later)
//
// The basic idea with this scheduler here is to keep track of where we *want*
// to run in tick_target_. We update this with the exact interval.
//
// Then, when we post our task, we take the floor of (tick_target_ and Now()).
// If we started at now=0, and 60FPs (all times in milliseconds):
//      now=0    target=16.667   PostDelayedTask(16)
//
// When our callback runs, we figure out how far off we were from that goal.
// Because of the flooring operation, and assuming our timer runs exactly when
// it should, this yields:
//      now=16   target=16.667
//
// Since we can't post a 0.667 ms task to get to now=16, we just treat this as a
// tick. Then, we update target to be 33.333. We now post another task based on
// the difference between our target and now:
//      now=16   tick_target=16.667  new_target=33.333   -->
//          PostDelayedTask(floor(33.333 - 16)) --> PostDelayedTask(17)
//
// Over time, with no late tasks, this leads to us posting tasks like this:
//      now=0    tick_target=0       new_target=16.667   -->
//          tick(), PostDelayedTask(16)
//      now=16   tick_target=16.667  new_target=33.333   -->
//          tick(), PostDelayedTask(17)
//      now=33   tick_target=33.333  new_target=50.000   -->
//          tick(), PostDelayedTask(17)
//      now=50   tick_target=50.000  new_target=66.667   -->
//          tick(), PostDelayedTask(16)
//
// We treat delays in tasks differently depending on the amount of delay we
// encounter. Suppose we posted a task with a target=16.667:
//   Case 1: late but not unrecoverably-so
//      now=18 tick_target=16.667
//
//   Case 2: so late we obviously missed the tick
//      now=25.0 tick_target=16.667
//
// We treat the first case as a tick anyway, and assume the delay was unusual.
// Thus, we compute the new_target based on the old timebase:
//      now=18   tick_target=16.667  new_target=33.333   -->
//          tick(), PostDelayedTask(floor(33.333-18)) --> PostDelayedTask(15)
// This brings us back to 18+15 = 33, which was where we would have been if the
// task hadn't been late.
//
// For the really late delay, we we move to the next logical tick. The timebase
// is not reset.
//      now=37   tick_target=16.667  new_target=50.000  -->
//          tick(), PostDelayedTask(floor(50.000-37)) --> PostDelayedTask(13)
void DelayBasedTimeSource::PostNextTickTask(base::TimeTicks now) {
  if (interval_.is_zero()) {
    next_tick_time_ = now;
  } else {
    next_tick_time_ = now.SnappedToNextTick(timebase_, interval_);
    if (next_tick_time_ == now)
      next_tick_time_ += interval_;
    DCHECK_GT(next_tick_time_, now);
  }
  tick_closure_.Reset(base::BindOnce(&DelayBasedTimeSource::OnTimerTick,
                                     weak_factory_.GetWeakPtr()));
  task_runner_->PostDelayedTask(FROM_HERE, tick_closure_.callback(),
                                next_tick_time_ - now);
}

std::string DelayBasedTimeSource::TypeString() const {
  return "DelayBasedTimeSource";
}

void DelayBasedTimeSource::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetString("type", TypeString());
  state->SetDouble("last_tick_time_us",
                   LastTickTime().since_origin().InMicroseconds());
  state->SetDouble("next_tick_time_us",
                   NextTickTime().since_origin().InMicroseconds());
  state->SetDouble("interval_us", interval_.InMicroseconds());
  state->SetDouble("timebase_us", timebase_.since_origin().InMicroseconds());
  state->SetBoolean("active", active_);
}

}  // namespace viz

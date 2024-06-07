// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/timeout_monitor.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"

using base::TimeTicks;

namespace input {

TimeoutMonitor::TimeoutMonitor(
    const TimeoutHandler& timeout_handler,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : timeout_handler_(timeout_handler) {
  DCHECK(!timeout_handler_.is_null());
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  timeout_timer_.SetTaskRunner(task_runner);
}

TimeoutMonitor::~TimeoutMonitor() {
  Stop();
}

void TimeoutMonitor::Start(base::TimeDelta delay) {
  if (!IsRunning()) {
    TRACE_EVENT_ASYNC_BEGIN0("renderer_host", "TimeoutMonitor", this);
    TRACE_EVENT_INSTANT0("renderer_host", "TimeoutMonitor::Start",
                         TRACE_EVENT_SCOPE_THREAD);
  }

  StartImpl(delay);
}

void TimeoutMonitor::Restart(base::TimeDelta delay) {
  if (!IsRunning()) {
    Start(delay);
    return;
  }

  TRACE_EVENT_INSTANT0("renderer_host", "TimeoutMonitor::Restart",
                       TRACE_EVENT_SCOPE_THREAD);
  // Setting to null will cause StartTimeoutMonitor to restart the timer.
  time_when_considered_timed_out_ = TimeTicks();
  StartImpl(delay);
}

void TimeoutMonitor::Stop() {
  if (!IsRunning())
    return;

  // We do not bother to stop the timeout_timer_ here in case it will be
  // started again shortly, which happens to be the common use case.
  TRACE_EVENT_INSTANT0("renderer_host", "TimeoutMonitor::Stop",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_ASYNC_END1("renderer_host", "TimeoutMonitor", this,
                         "result", "stopped");
  time_when_considered_timed_out_ = TimeTicks();
}

void TimeoutMonitor::StartImpl(base::TimeDelta delay) {
  // Set time_when_considered_timed_out_ if it's null. Also, update
  // time_when_considered_timed_out_ if the caller's request is sooner than the
  // existing one. This will have the side effect that the existing timeout will
  // be forgotten.
  TimeTicks requested_end_time = TimeTicks::Now() + delay;
  if (time_when_considered_timed_out_.is_null() ||
      time_when_considered_timed_out_ > requested_end_time)
    time_when_considered_timed_out_ = requested_end_time;

  // If we already have a timer with the same or shorter duration, then we can
  // wait for it to finish.
  if (timeout_timer_.IsRunning() && timeout_timer_.GetCurrentDelay() <= delay) {
    // If time_when_considered_timed_out_ was null, this timer may fire early.
    // CheckTimedOut handles that that by calling Start with the remaining time.
    // If time_when_considered_timed_out_ was non-null, it means we still
    // haven't been stopped, so we leave time_when_considered_timed_out_ as is.
    return;
  }

  // Either the timer is not yet running, or we need to adjust the timer to
  // fire sooner.
  time_when_considered_timed_out_ = requested_end_time;
  timeout_timer_.Stop();
  timeout_timer_.Start(FROM_HERE, delay, this, &TimeoutMonitor::CheckTimedOut);
}

void TimeoutMonitor::CheckTimedOut() {
  // If we received a call to |Stop()|.
  if (time_when_considered_timed_out_.is_null())
    return;

  // If we have not waited long enough, then wait some more.
  TimeTicks now = TimeTicks::Now();
  if (now < time_when_considered_timed_out_) {
    TRACE_EVENT_INSTANT0("renderer_host", "TimeoutMonitor::Reschedule",
                         TRACE_EVENT_SCOPE_THREAD);
    StartImpl(time_when_considered_timed_out_ - now);
    return;
  }

  TRACE_EVENT_ASYNC_END1("renderer_host", "TimeoutMonitor", this,
                         "result", "timed_out");
  TRACE_EVENT0("renderer_host", "TimeoutMonitor::TimeOutHandler");
  time_when_considered_timed_out_ = TimeTicks();
  timeout_handler_.Run();
}

bool TimeoutMonitor::IsRunning() const {
  return timeout_timer_.IsRunning() &&
         !time_when_considered_timed_out_.is_null();
}

base::TimeDelta TimeoutMonitor::GetCurrentDelay() {
  DCHECK(!time_when_considered_timed_out_.is_null());
  return time_when_considered_timed_out_ - TimeTicks::Now();
}

}  // namespace input

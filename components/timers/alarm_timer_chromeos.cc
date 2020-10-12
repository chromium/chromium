// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/timers/alarm_timer_chromeos.h"

#include <stdint.h>
#include <sys/timerfd.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/pending_task.h"
#include "base/task/common/task_annotator.h"
#include "base/trace_event/trace_event.h"

namespace timers {

// static
std::unique_ptr<SimpleAlarmTimer> SimpleAlarmTimer::Create() {
  return CreateInternal(CLOCK_REALTIME_ALARM);
}

// static
std::unique_ptr<SimpleAlarmTimer> SimpleAlarmTimer::CreateForTesting() {
  // For unittest, use CLOCK_REALTIME in order to run the tests without
  // CAP_WAKE_ALARM.
  return CreateInternal(CLOCK_REALTIME);
}

// static
std::unique_ptr<SimpleAlarmTimer> SimpleAlarmTimer::CreateInternal(
    int clockid) {
  base::ScopedFD alarm_fd(timerfd_create(clockid, TFD_CLOEXEC));
  if (!alarm_fd.is_valid()) {
    PLOG(ERROR) << "Failed to create timer fd";
    return nullptr;
  }

  // Note: std::make_unique<> cannot be used because the constructor is
  // private.
  return base::WrapUnique(new SimpleAlarmTimer(std::move(alarm_fd)));
}

SimpleAlarmTimer::SimpleAlarmTimer(base::ScopedFD alarm_fd)
    : alarm_fd_(std::move(alarm_fd)) {}

SimpleAlarmTimer::~SimpleAlarmTimer() {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  Stop();
}

void SimpleAlarmTimer::Stop() {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  if (!IsRunning())
    return;

  // Cancel any previous callbacks.
  weak_factory_.InvalidateWeakPtrs();

  base::RetainingOneShotTimer::set_is_running(false);
  alarm_fd_watcher_.reset();
  pending_task_.reset();
}

void SimpleAlarmTimer::Reset() {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!base::RetainingOneShotTimer::user_task().is_null());

  // Cancel any previous callbacks and stop watching |alarm_fd_|.
  weak_factory_.InvalidateWeakPtrs();
  alarm_fd_watcher_.reset();

  // Ensure that the delay is not negative.
  const base::TimeDelta delay = std::max(
      base::TimeDelta(), base::RetainingOneShotTimer::GetCurrentDelay());

  // Set up the pending task.
  base::RetainingOneShotTimer::set_desired_run_time(
      delay.is_zero() ? base::TimeTicks() : base::TimeTicks::Now() + delay);
  pending_task_ = std::make_unique<base::PendingTask>(
      base::RetainingOneShotTimer::posted_from(),
      base::RetainingOneShotTimer::user_task(),
      base::RetainingOneShotTimer::desired_run_time());

  // Set |alarm_fd_| to be signaled when the delay expires. If the delay is
  // zero, |alarm_fd_| will never be signaled. This overrides the previous
  // delay, if any.
  itimerspec alarm_time = {};
  alarm_time.it_value.tv_sec = delay.InSeconds();
  alarm_time.it_value.tv_nsec =
      (delay.InMicroseconds() % base::Time::kMicrosecondsPerSecond) *
      base::Time::kNanosecondsPerMicrosecond;
  if (timerfd_settime(alarm_fd_.get(), 0, &alarm_time, NULL) < 0)
    PLOG(ERROR) << "Error while setting alarm time.  Timer will not fire";

  // The timer is running.
  base::RetainingOneShotTimer::set_is_running(true);

  // If the delay is zero, post the task now.
  if (delay.is_zero()) {
    origin_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SimpleAlarmTimer::OnTimerFired,
                                  weak_factory_.GetWeakPtr()));
  } else {
    // Otherwise, if the delay is not zero, generate a tracing event to indicate
    // that the task was posted and watch |alarm_fd_|.
    base::TaskAnnotator().WillQueueTask("SimpleAlarmTimer::Reset",
                                        pending_task_.get(), "");
    alarm_fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        alarm_fd_.get(),
        base::BindRepeating(&SimpleAlarmTimer::OnAlarmFdReadableWithoutBlocking,
                            weak_factory_.GetWeakPtr()));
  }
}

void SimpleAlarmTimer::OnAlarmFdReadableWithoutBlocking() {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(base::RetainingOneShotTimer::IsRunning());

  // Read from |alarm_fd_| to ack the event.
  char val[sizeof(uint64_t)];
  if (!base::ReadFromFD(alarm_fd_.get(), val, sizeof(uint64_t)))
    PLOG(DFATAL) << "Unable to read from timer file descriptor.";

  OnTimerFired();
}

void SimpleAlarmTimer::OnTimerFired() {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(base::RetainingOneShotTimer::IsRunning());
  DCHECK(pending_task_.get());

  // Take ownership of the PendingTask to prevent it from being deleted if the
  // SimpleAlarmTimer is deleted.
  const auto pending_user_task = std::move(pending_task_);

  base::WeakPtr<SimpleAlarmTimer> weak_ptr = weak_factory_.GetWeakPtr();

  // Run the task.
  TRACE_TASK_EXECUTION("SimpleAlarmTimer::OnTimerFired", *pending_user_task);
  base::TaskAnnotator().RunTask("SimpleAlarmTimer::Reset",
                                pending_user_task.get());

  // If the timer wasn't deleted, stopped or reset by the callback, stop it.
  if (weak_ptr)
    Stop();
}

}  // namespace timers

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TIMERS_ALARM_TIMER_CHROMEOS_H_
#define COMPONENTS_TIMERS_ALARM_TIMER_CHROMEOS_H_

#include <memory>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
struct PendingTask;
}

namespace timers {
// The class implements a timer that is capable of waking the system up from a
// suspended state. For example, this is useful for running tasks that are
// needed for maintaining network connectivity, like sending heartbeat messages.
// Currently, this feature is only available on Chrome OS systems running linux
// version 3.11 or higher.
//
// A SimpleAlarmTimer instance can only be used from the sequence on which it
// was instantiated. Start() and Stop() must be called from a thread that
// supports FileDescriptorWatcher.
//
// A SimpleAlarmTimer only fires once but remembers the task that it was given
// even after it has fired.  Useful if you want to run the same task multiple
// times but not at a regular interval.
class SimpleAlarmTimer : public base::RetainingOneShotTimer {
 public:
  // Creates the SimpleAlarmTimer instance, or returns null on failure, e.g.,
  // on a platform without timerfd_* system calls support, or missing
  // capability (CAP_WAKE_ALARM).
  static std::unique_ptr<SimpleAlarmTimer> Create();

  // Similar to Create(), but for unittests without capability.
  // Specifically, uses CLOCK_REALTIME instead of CLOCK_REALTIME_ALARM.
  static std::unique_ptr<SimpleAlarmTimer> CreateForTesting();

  ~SimpleAlarmTimer() override;

  // Timer overrides.
  void Stop() override;
  void Reset() override;

 private:
  // Shared implementation of Create and CreateForTesting.
  static std::unique_ptr<SimpleAlarmTimer> CreateInternal(int clockid);

  explicit SimpleAlarmTimer(base::ScopedFD alarm_fd);

  // Called when |alarm_fd_| is readable without blocking. Reads data from
  // |alarm_fd_| and calls OnTimerFired().
  void OnAlarmFdReadableWithoutBlocking();

  // Called when the timer fires. Runs the callback.
  void OnTimerFired();

  // Timer file descriptor.
  const base::ScopedFD alarm_fd_;

  // Watches |alarm_fd_|.
  std::unique_ptr<base::FileDescriptorWatcher::Controller> alarm_fd_watcher_;

  // Posts tasks to the sequence on which this AlarmTimer was instantiated.
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_ =
      base::SequencedTaskRunnerHandle::Get();

  // Keeps track of the user task we want to run. A new one is constructed every
  // time Reset() is called.
  std::unique_ptr<base::PendingTask> pending_task_;

  // Used to invalidate pending callbacks.
  base::WeakPtrFactory<SimpleAlarmTimer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SimpleAlarmTimer);
};

}  // namespace timers

#endif  // COMPONENTS_TIMERS_ALARM_TIMER_CHROMEOS_H_

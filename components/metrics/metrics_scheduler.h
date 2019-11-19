// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SCHEDULER_H_
#define COMPONENTS_METRICS_METRICS_SCHEDULER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace metrics {

// Scheduler task to drive a MetricsService object's uploading.
class MetricsScheduler {
 public:
  // Creates MetricsScheduler object with the given |task_callback|
  // callback to call when a task should happen.
  MetricsScheduler(const base::Closure& task_callback,
                   bool fast_startup_for_testing);
  virtual ~MetricsScheduler();

  // Starts scheduling uploads. This in a no-op if the scheduler is already
  // running, so it is safe to call more than once.
  void Start();

  // Stops scheduling uploads.
  void Stop();

 protected:
  // Subclasses should provide task_callback with a wrapper to call this with.
  // This indicates the triggered task was completed/cancelled and the next
  // call can be scheduled.
  void TaskDone(base::TimeDelta next_interval);

  // Called by the Timer when it's time to run the task.
  virtual void TriggerTask();

 private:
  // Schedules a future call to TriggerTask if one isn't already pending.
  void ScheduleNextTask();

  // The method to call when task should happen.
  const base::Closure task_callback_;

  // Uses a one-shot timer rather than a repeating one because the task may be
  // async, and the length of the interval may change.
  base::OneShotTimer timer_;

  // The interval between being told an task is done and starting the next task.
  base::TimeDelta interval_;

  // Indicates that the scheduler is running (i.e., that Start has been called
  // more recently than Stop).
  bool running_;

  // Indicates that the last triggered task hasn't resolved yet.
  bool callback_pending_;

  DISALLOW_COPY_AND_ASSIGN(MetricsScheduler);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SCHEDULER_H_

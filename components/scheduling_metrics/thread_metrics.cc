// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduling_metrics/thread_metrics.h"

namespace scheduling_metrics {

namespace {

// Threshold for discarding ultra-long tasks. It is assumed that ultra-long
// tasks are reporting glitches (e.g. system falling asleep on the middle of the
// task).
constexpr base::TimeDelta kLongTaskDiscardingThreshold =
    base::TimeDelta::FromSeconds(30);

}  // namespace

ThreadMetrics::ThreadMetrics(ThreadType thread_type,
                             bool has_cpu_timing_for_each_task)
    : thread_type_(thread_type),
      has_cpu_timing_for_each_task_(has_cpu_timing_for_each_task),
      last_known_time_(has_cpu_timing_for_each_task_ ? base::ThreadTicks::Now()
                                                     : base::ThreadTicks()),
      thread_task_duration_reporter_(
          "Scheduler.Experimental.WallTimePerThread"),
      thread_task_cpu_duration_reporter_(
          "Scheduler.Experimental.CPUTimePerThread"),
      tracked_cpu_duration_reporter_(
          "Scheduler.Experimental.CPUTimePerThread.Tracked"),
      non_tracked_cpu_duration_reporter_(
          "Scheduler.Experimental.CPUTimePerThread.Untracked") {}

ThreadMetrics::~ThreadMetrics() {}

bool ThreadMetrics::ShouldDiscardTask(
    base::sequence_manager::TaskQueue* queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  // TODO(altimin): Investigate the relationship between thread time and
  // wall time for discarded tasks.
  return task_timing.wall_duration() > kLongTaskDiscardingThreshold;
}

void ThreadMetrics::RecordTaskMetrics(
    base::sequence_manager::TaskQueue* queue,
    const base::sequence_manager::Task& task,
    const base::sequence_manager::TaskQueue::TaskTiming& task_timing) {
  DCHECK(!has_cpu_timing_for_each_task_ || task_timing.has_thread_time());
  thread_task_duration_reporter_.RecordTask(thread_type_,
                                            task_timing.wall_duration());

  if (!task_timing.has_thread_time())
    return;
  thread_task_cpu_duration_reporter_.RecordTask(thread_type_,
                                                task_timing.thread_duration());
  if (has_cpu_timing_for_each_task_) {
    non_tracked_cpu_duration_reporter_.RecordTask(
        thread_type_, task_timing.start_thread_time() - last_known_time_);
    tracked_cpu_duration_reporter_.RecordTask(thread_type_,
                                              task_timing.thread_duration());

    last_known_time_ = task_timing.end_thread_time();
  }
}

}  // namespace scheduling_metrics

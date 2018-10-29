// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULING_METRICS_THREAD_METRICS_H_
#define COMPONENTS_SCHEDULING_METRICS_THREAD_METRICS_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/time.h"
#include "components/scheduling_metrics/task_duration_metric_reporter.h"
#include "components/scheduling_metrics/thread_type.h"
#include "components/scheduling_metrics/total_duration_metric_reporter.h"

namespace scheduling_metrics {

// Helper class to take care of task metrics shared between different threads
// in Chrome, including but not limited to browser UI and IO threads, renderer
// main thread and blink worker threads.
//
class COMPONENT_EXPORT(SCHEDULING_METRICS) ThreadMetrics {
 public:
  ThreadMetrics(ThreadType thread_type, bool has_cpu_timing_for_each_task);
  ~ThreadMetrics();

  bool ShouldDiscardTask(
      base::sequence_manager::TaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

  // Record task metrics which are shared between threads.
  void RecordTaskMetrics(
      base::sequence_manager::TaskQueue* queue,
      const base::sequence_manager::Task& task,
      const base::sequence_manager::TaskQueue::TaskTiming& task_timing);

 protected:
  const ThreadType thread_type_;
  const bool has_cpu_timing_for_each_task_;

 private:
  base::ThreadTicks last_known_time_;

  TaskDurationMetricReporter<ThreadType> thread_task_duration_reporter_;
  TaskDurationMetricReporter<ThreadType> thread_task_cpu_duration_reporter_;
  TaskDurationMetricReporter<ThreadType> tracked_cpu_duration_reporter_;
  TaskDurationMetricReporter<ThreadType> non_tracked_cpu_duration_reporter_;

  DISALLOW_COPY_AND_ASSIGN(ThreadMetrics);
};

}  // namespace scheduling_metrics

#endif  // COMPONENTS_SCHEDULING_METRICS_THREAD_METRICS_H_

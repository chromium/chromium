// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_SCHEDULER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_SCHEDULER_H_

#include <stdint.h>

#include "components/download/public/task/download_task_types.h"

namespace download {

// A helper class backed by system APIs to schedule jobs in the background. The
// tasks can run independently of each other as long as they have different
// |task_type|. Scheduling another task of same |task_type| before the task is
// fired will cancel the previous task.
class TaskScheduler {
 public:
  // Schedules a task with the operating system. The system has the liberty of
  // firing the task any time between |window_start_time_seconds| and
  // |window_end_time_seconds|. If the trigger conditions are not met, the
  // behavior is unknown.
  virtual void ScheduleTask(DownloadTaskType task_type,
                            bool require_unmetered_network,
                            bool require_charging,
                            int optimal_battery_percentage,
                            int64_t window_start_time_seconds,
                            int64_t window_end_time_seconds) = 0;

  // Cancels a pre-scheduled task of type |task_type|.
  virtual void CancelTask(DownloadTaskType task_type) = 0;

  virtual ~TaskScheduler() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_TASK_TASK_SCHEDULER_H_

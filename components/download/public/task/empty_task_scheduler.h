// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_TASK_EMPTY_TASK_SCHEDULER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_TASK_EMPTY_TASK_SCHEDULER_H_

#include <stdint.h>

#include "components/download/public/task/task_scheduler.h"

namespace download {

// Task scheduler that does nothing for incognito mode.
class EmptyTaskScheduler : public TaskScheduler {
 public:
  EmptyTaskScheduler();

  EmptyTaskScheduler(const EmptyTaskScheduler&) = delete;
  EmptyTaskScheduler& operator=(const EmptyTaskScheduler&) = delete;

  ~EmptyTaskScheduler() override;

 private:
  // TaskScheduler implementation.
  void ScheduleTask(DownloadTaskType task_type,
                    bool require_unmetered_network,
                    bool require_charging,
                    int optimal_battery_percentage,
                    int64_t window_start_time_seconds,
                    int64_t window_end_time_seconds) override;
  void CancelTask(DownloadTaskType task_type) override;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_TASK_EMPTY_TASK_SCHEDULER_H_

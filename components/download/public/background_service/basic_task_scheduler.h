// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BASIC_TASK_SCHEDULER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BASIC_TASK_SCHEDULER_H_

#include <map>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/task/task_scheduler.h"

namespace download {

class BackgroundDownloadService;

// A TaskScheduler implementation that doesn't do anything but posts the task
// after the specified delay.
class BasicTaskScheduler : public download::TaskScheduler {
 public:
  explicit BasicTaskScheduler(
      const base::RepeatingCallback<BackgroundDownloadService*()>&
          get_download_service);
  BasicTaskScheduler(const BasicTaskScheduler& other) = delete;
  BasicTaskScheduler& operator=(const BasicTaskScheduler& other) = delete;
  ~BasicTaskScheduler() override;

  // TaskScheduler implementation.
  void ScheduleTask(download::DownloadTaskType task_type,
                    bool require_unmetered_network,
                    bool require_charging,
                    int optimal_battery_percentage,
                    int64_t window_start_time_seconds,
                    int64_t window_end_time_seconds) override;
  void CancelTask(download::DownloadTaskType task_type) override;

 private:
  void RunScheduledTask(download::DownloadTaskType task_type);
  void OnTaskFinished(bool reschedule);

  // Keeps track of scheduled tasks so that they can be cancelled.
  std::map<download::DownloadTaskType, base::CancelableOnceClosure>
      scheduled_tasks_;

  base::RepeatingCallback<BackgroundDownloadService*()> get_download_service_;

  base::WeakPtrFactory<BasicTaskScheduler> weak_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BASIC_TASK_SCHEDULER_H_

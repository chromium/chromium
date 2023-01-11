// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_H_

#include "base/functional/callback.h"
#include "components/background_task_scheduler/task_info.h"
#include "components/keyed_service/core/keyed_service.h"

namespace background_task {

// A BackgroundTaskScheduler is used to schedule jobs that run in the
// background. It is backed by system APIs which have different implementations
// on different android versions. For more information, please refer
// BackgroundTaskScheduler.java.
class BackgroundTaskScheduler : public KeyedService {
 public:
  BackgroundTaskScheduler(const BackgroundTaskScheduler&) = delete;
  BackgroundTaskScheduler& operator=(const BackgroundTaskScheduler&) = delete;

  // Schedules a background task with various scheduling related params
  // contained in |task_info|.
  virtual bool Schedule(const TaskInfo& task_info) = 0;

  // Cancels the task specified by the |task_id}.
  virtual void Cancel(int task_id) = 0;

 protected:
  BackgroundTaskScheduler() = default;
  ~BackgroundTaskScheduler() override = default;
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_INTERNAL_ANDROID_NATIVE_TASK_SCHEDULER_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_INTERNAL_ANDROID_NATIVE_TASK_SCHEDULER_H_

#include "components/background_task_scheduler/background_task_scheduler.h"

namespace background_task {

// A bridge class that forwards the scheduling calls to Java.
class NativeTaskScheduler : public BackgroundTaskScheduler {
 public:
  NativeTaskScheduler();

  NativeTaskScheduler(const NativeTaskScheduler&) = delete;
  NativeTaskScheduler& operator=(const NativeTaskScheduler&) = delete;

  ~NativeTaskScheduler() override;

  // BackgroundTaskScheduler overrides.
  bool Schedule(const TaskInfo& task_info) override;
  void Cancel(int task_id) override;
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_INTERNAL_ANDROID_NATIVE_TASK_SCHEDULER_H_

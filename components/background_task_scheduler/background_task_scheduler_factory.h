// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_FACTORY_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_FACTORY_H_

namespace background_task {
class BackgroundTaskScheduler;

// A factory for creating BackgroundTaskScheduler.
class BackgroundTaskSchedulerFactory {
 public:
  // Returns the singleton BackgroundTaskScheduler.
  static BackgroundTaskScheduler* GetScheduler();

  BackgroundTaskSchedulerFactory(const BackgroundTaskSchedulerFactory&) =
      delete;
  BackgroundTaskSchedulerFactory& operator=(
      const BackgroundTaskSchedulerFactory&) = delete;

 private:
  BackgroundTaskSchedulerFactory();
  ~BackgroundTaskSchedulerFactory();
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_FACTORY_H_

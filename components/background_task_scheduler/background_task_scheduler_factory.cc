// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_task_scheduler/background_task_scheduler_factory.h"

#include "build/build_config.h"
#include "components/background_task_scheduler/background_task_scheduler.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/no_destructor.h"
#include "components/background_task_scheduler/internal/android/native_task_scheduler.h"
#endif

namespace background_task {

// static
BackgroundTaskScheduler* BackgroundTaskSchedulerFactory::GetScheduler() {
#if BUILDFLAG(IS_ANDROID)
  static base::NoDestructor<NativeTaskScheduler> scheduler;
  return scheduler.get();
#else
  return nullptr;
#endif
}

BackgroundTaskSchedulerFactory::BackgroundTaskSchedulerFactory() = default;

BackgroundTaskSchedulerFactory::~BackgroundTaskSchedulerFactory() = default;

}  // namespace background_task

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/task/empty_task_scheduler.h"

namespace download {

EmptyTaskScheduler::EmptyTaskScheduler() = default;

EmptyTaskScheduler::~EmptyTaskScheduler() = default;

void EmptyTaskScheduler::ScheduleTask(DownloadTaskType task_type,
                                      bool require_unmetered_network,
                                      bool require_charging,
                                      int optimal_battery_percentage,
                                      int64_t window_start_time_seconds,
                                      int64_t window_end_time_seconds) {}

void EmptyTaskScheduler::CancelTask(DownloadTaskType task_type) {}

}  // namespace download

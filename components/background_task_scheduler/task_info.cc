// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_task_scheduler/task_info.h"

namespace background_task {

PeriodicInfo::PeriodicInfo()
    : interval_ms(0), flex_ms(0), expires_after_window_end_time(false) {}

PeriodicInfo::~PeriodicInfo() = default;

OneOffInfo::OneOffInfo()
    : window_start_time_ms(0),
      window_end_time_ms(0),
      expires_after_window_end_time(false) {}

OneOffInfo::~OneOffInfo() = default;

TaskInfo::TaskInfo(int task_id, const PeriodicInfo& timing_info)
    : task_id(task_id), periodic_info(timing_info) {}

TaskInfo::TaskInfo(int task_id, const OneOffInfo& timing_info)
    : task_id(task_id), one_off_info(timing_info) {}

TaskInfo::~TaskInfo() = default;

}  // namespace background_task

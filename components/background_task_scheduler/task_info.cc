// Copyright 2020 The Chromium Authors. All rights reserved.
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

ExactInfo::ExactInfo() : trigger_at_ms(0) {}

ExactInfo::~ExactInfo() = default;

TaskInfo::TaskInfo(int task_id, const PeriodicInfo& timing_info)
    : task_id(task_id),
      network_type(NetworkType::NONE),
      requires_charging(false),
      is_persisted(false),
      update_current(false),
      periodic_info(timing_info) {}

TaskInfo::TaskInfo(int task_id, const OneOffInfo& timing_info)
    : task_id(task_id),
      network_type(NetworkType::NONE),
      requires_charging(false),
      is_persisted(false),
      update_current(false),
      one_off_info(timing_info) {}

TaskInfo::TaskInfo(int task_id, const ExactInfo& timing_info)
    : task_id(task_id),
      network_type(NetworkType::NONE),
      requires_charging(false),
      is_persisted(false),
      update_current(false),
      exact_info(timing_info) {}

TaskInfo::~TaskInfo() = default;

}  // namespace background_task

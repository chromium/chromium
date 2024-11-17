// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/scheduler_stub.h"

namespace {
const int kBatteryPercentageHigh = 75;
const bool kPowerRequired = true;
}  // namespace

namespace offline_pages {

SchedulerStub::SchedulerStub()
    : schedule_called_(false),
      backup_schedule_called_(false),
      unschedule_called_(false),
      get_current_device_conditions_called_(false),
      schedule_delay_(0L),
      device_conditions_(kPowerRequired,
                         kBatteryPercentageHigh,
                         net::NetworkChangeNotifier::CONNECTION_2G),
      trigger_conditions_(false, 0, false) {}

SchedulerStub::~SchedulerStub() = default;

void SchedulerStub::Schedule(const TriggerConditions& trigger_conditions) {
  schedule_called_ = true;
  trigger_conditions_ = trigger_conditions;
}

void SchedulerStub::BackupSchedule(const TriggerConditions& trigger_conditions,
                                   int64_t delay_in_seconds) {
  backup_schedule_called_ = true;
  schedule_delay_ = delay_in_seconds;
  trigger_conditions_ = trigger_conditions;
}

void SchedulerStub::Unschedule() {
  unschedule_called_ = true;
}

const DeviceConditions& SchedulerStub::GetCurrentDeviceConditions() {
  get_current_device_conditions_called_ = true;
  return device_conditions_;
}

}  // namespace offline_pages

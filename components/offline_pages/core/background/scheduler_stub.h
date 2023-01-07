// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SCHEDULER_STUB_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SCHEDULER_STUB_H_

#include <stdint.h>

#include "components/offline_pages/core/background/scheduler.h"

namespace offline_pages {

// Test class stubbing out the functionality of Scheduler.
// It is only used for test support.
class SchedulerStub : public Scheduler {
 public:
  SchedulerStub();
  ~SchedulerStub() override;

  void Schedule(const TriggerConditions& trigger_conditions) override;

  void BackupSchedule(const TriggerConditions& trigger_conditions,
                      int64_t delay_in_seconds) override;

  // Unschedules the currently scheduled task, if any.
  void Unschedule() override;

  // Get the current device conditions from the android APIs.
  const DeviceConditions& GetCurrentDeviceConditions() override;

  bool schedule_called() const { return schedule_called_; }

  bool backup_schedule_called() const { return backup_schedule_called_; }

  bool unschedule_called() const { return unschedule_called_; }

  TriggerConditions const* trigger_conditions() const {
    return &trigger_conditions_;
  }

  int64_t schedule_delay() const { return schedule_delay_; }

 private:
  bool schedule_called_;
  bool backup_schedule_called_;
  bool unschedule_called_;
  bool get_current_device_conditions_called_;
  int64_t schedule_delay_;
  DeviceConditions device_conditions_;
  TriggerConditions trigger_conditions_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SCHEDULER_STUB_H_

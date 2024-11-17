// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SCHEDULER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SCHEDULER_H_

#include <stdint.h>

#include "components/offline_pages/core/background/device_conditions.h"

namespace offline_pages {

// Interface of a class responsible for scheduling a task to initiate
// processing of background offlining requests upon select system conditions
// (such as having a network connection).
class Scheduler {
 public:
  // Defines a set of system conditions to trigger background processing.
  struct TriggerConditions {
    TriggerConditions(bool power, int battery, bool unmetered)
        : require_power_connected(power),
          minimum_battery_percentage(battery),
          require_unmetered_network(unmetered) {}
    bool require_power_connected;
    int minimum_battery_percentage;
    bool require_unmetered_network;
  };

  Scheduler() = default;
  virtual ~Scheduler() = default;

  // Schedules the triggering of a task subject to |trigger_conditions|.
  // This may overwrite any previous scheduled task with a new one for
  // these conditions. That is, only one set of triggering conditions
  // is scheduled at a time.
  virtual void Schedule(const TriggerConditions& trigger_conditions) = 0;

  // Schedules the triggering of a task in case Chromium is killed,
  // so we can continue processing background download requests.  This will
  // not overwrite existing tasks.
  virtual void BackupSchedule(const TriggerConditions& trigger_conditions,
                              int64_t delay_in_seconds) = 0;

  // Unschedules the currently scheduled task, if any.
  virtual void Unschedule() = 0;

  // Get the current device conditions from the android APIs.
  virtual const DeviceConditions& GetCurrentDeviceConditions() = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_SCHEDULER_H_

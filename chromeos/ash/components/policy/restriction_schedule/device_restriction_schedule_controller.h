// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

#include "base/component_export.h"

class PrefRegistrySimple;

namespace policy {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    DeviceRestrictionScheduleController {
 public:
  DeviceRestrictionScheduleController();
  ~DeviceRestrictionScheduleController();

  DeviceRestrictionScheduleController(
      const DeviceRestrictionScheduleController&) = delete;
  DeviceRestrictionScheduleController& operator=(
      const DeviceRestrictionScheduleController&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

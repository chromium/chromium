// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

// This class observes the pref `kDeviceRestrictionSchedule`, and handles
// restricting the device access when the schedule is active.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    DeviceRestrictionScheduleController {
 public:
  explicit DeviceRestrictionScheduleController(PrefService& pref_service);
  ~DeviceRestrictionScheduleController();

  DeviceRestrictionScheduleController(
      const DeviceRestrictionScheduleController&) = delete;
  DeviceRestrictionScheduleController& operator=(
      const DeviceRestrictionScheduleController&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  // Handles policy updates.
  void OnPolicyUpdated();

  // Monitor `kDeviceRestrictionSchedule` pref for changes.
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<DeviceRestrictionScheduleController> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_RESTRICTION_SCHEDULE_DEVICE_RESTRICTION_SCHEDULE_CONTROLLER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"

#include "chromeos/constants/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

DeviceRestrictionScheduleController::DeviceRestrictionScheduleController() =
    default;

DeviceRestrictionScheduleController::~DeviceRestrictionScheduleController() =
    default;

// static
void DeviceRestrictionScheduleController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(chromeos::prefs::kDeviceRestrictionSchedule);
}

}  // namespace policy

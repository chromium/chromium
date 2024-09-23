// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH_SYSTEM_INFO_LAUNCHER_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH_SYSTEM_INFO_LAUNCHER_UTIL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/launcher_search/system_info/system_info_keyword_input.h"
#include "chromeos/ash/components/system_info/battery_health.h"
#include "chromeos/ash/components/system_info/cpu_data.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace launcher_search {

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH)
    PopulatePowerStatus(
        const power_manager::PowerSupplyProperties& power_supply_properties,
        system_info::BatteryHealth& battery_health);

std::vector<SystemInfoKeywordInput> COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH) GetSystemInfoKeywordVector();

}  // namespace launcher_search

#endif  // CHROMEOS_ASH_COMPONENTS_LAUNCHER_SEARCH_SYSTEM_INFO_LAUNCHER_UTIL_H_

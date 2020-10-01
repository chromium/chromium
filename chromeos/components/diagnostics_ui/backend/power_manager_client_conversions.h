// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_POWER_MANAGER_CLIENT_CONVERSIONS_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_POWER_MANAGER_CLIENT_CONVERSIONS_H_

#include "base/strings/string16.h"
#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace chromeos {
namespace diagnostics {

mojom::BatteryState ConvertBatteryStateFromProto(
    power_manager::PowerSupplyProperties::BatteryState battery_state);

mojom::ExternalPowerSource ConvertPowerSourceFromProto(
    power_manager::PowerSupplyProperties::ExternalPower power_source);

// Constructs a time-formatted string representing the amount of time remaining
// to either charge or discharge the battery. If the battery is full or the
// amount of time is unreliable / still being calculated, this returns an
// empty string. Otherwise, the time is returned in DURATION_WIDTH_NARROW
// format.
base::string16 ConstructPowerTime(
    mojom::BatteryState battery_state,
    const power_manager::PowerSupplyProperties& power_supply_props);

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_POWER_MANAGER_CLIENT_CONVERSIONS_H_

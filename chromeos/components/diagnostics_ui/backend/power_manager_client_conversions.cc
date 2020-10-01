// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/power_manager_client_conversions.h"

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"

namespace chromeos {
namespace diagnostics {

mojom::BatteryState ConvertBatteryStateFromProto(
    power_manager::PowerSupplyProperties::BatteryState battery_state) {
  DCHECK_NE(power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT,
            battery_state);

  switch (battery_state) {
    case power_manager::PowerSupplyProperties_BatteryState_CHARGING:
      return mojom::BatteryState::kCharging;
    case power_manager::PowerSupplyProperties_BatteryState_DISCHARGING:
      return mojom::BatteryState::kDischarging;
    case power_manager::PowerSupplyProperties_BatteryState_FULL:
      return mojom::BatteryState::kFull;
    default:
      NOTREACHED();
      return mojom::BatteryState::kFull;
  }
}

mojom::ExternalPowerSource ConvertPowerSourceFromProto(
    power_manager::PowerSupplyProperties::ExternalPower power_source) {
  switch (power_source) {
    case power_manager::PowerSupplyProperties_ExternalPower_AC:
      return mojom::ExternalPowerSource::kAc;
    case power_manager::PowerSupplyProperties_ExternalPower_USB:
      return mojom::ExternalPowerSource::kUsb;
    case power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED:
      return mojom::ExternalPowerSource::kDisconnected;
    default:
      NOTREACHED();
      return mojom::ExternalPowerSource::kDisconnected;
  }
}

base::string16 ConstructPowerTime(
    mojom::BatteryState battery_state,
    const power_manager::PowerSupplyProperties& power_supply_props) {
  if (battery_state == mojom::BatteryState::kFull) {
    // Return an empty string if the battery is full.
    return base::string16();
  }

  int64_t time_in_seconds;
  if (battery_state == mojom::BatteryState::kCharging) {
    time_in_seconds = power_supply_props.has_battery_time_to_full_sec()
                          ? power_supply_props.battery_time_to_full_sec()
                          : -1;
  } else {
    DCHECK(battery_state == mojom::BatteryState::kDischarging);
    time_in_seconds = power_supply_props.has_battery_time_to_empty_sec()
                          ? power_supply_props.battery_time_to_empty_sec()
                          : -1;
  }

  if (power_supply_props.is_calculating_battery_time() || time_in_seconds < 0) {
    // If power manager is still calculating battery time or |time_in_seconds|
    // is negative (meaning power manager couldn't compute a reasonable time)
    // return an empty string.
    return base::string16();
  }

  const base::TimeDelta as_time_delta =
      base::TimeDelta::FromSeconds(time_in_seconds);
  base::string16 time_duration;
  if (!base::TimeDurationFormat(as_time_delta, base::DURATION_WIDTH_NARROW,
                                &time_duration)) {
    LOG(ERROR) << "Failed to format time duration " << as_time_delta;
    return base::string16();
  }

  return time_duration;
}

}  // namespace diagnostics
}  // namespace chromeos

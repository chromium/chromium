// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/power/battery_level_provider_chromeos.h"

#include <optional>

#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace performance_manager::power {

const double kmAhPerAh = 1000.0;

BatteryLevelProviderChromeOS::BatteryLevelProviderChromeOS(
    chromeos::PowerManagerClient* power_manager_client)
    : power_manager_client_(power_manager_client) {
  DCHECK(power_manager_client_);
}

BatteryLevelProviderChromeOS::~BatteryLevelProviderChromeOS() = default;

// static
std::unique_ptr<base::BatteryLevelProvider>
BatteryLevelProviderChromeOS::Create() {
  return std::make_unique<
      performance_manager::power::BatteryLevelProviderChromeOS>(
      chromeos::PowerManagerClient::Get());
}

void BatteryLevelProviderChromeOS::GetBatteryState(
    base::OnceCallback<
        void(const std::optional<base::BatteryLevelProvider::BatteryState>&)>
        callback) {
  DCHECK(power_manager_client_);

  const std::optional<power_manager::PowerSupplyProperties>& power_status =
      power_manager_client_->GetLastStatus();
  if (!power_status) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::BatteryLevelProvider::BatteryState state;

  if (power_status->battery_state() ==
      power_manager::PowerSupplyProperties::NOT_PRESENT) {
    state = {
        .battery_count = 0,
        .is_external_power_connected = true,
        .current_capacity = std::nullopt,
        .full_charged_capacity = std::nullopt,
        .charge_unit = std::nullopt,
        // TODO(anthonyvd): This should be the time that the state was last
        // sampled from the system, but PowerManagerClient doesn't yet provide
        // this information.
        .capture_time = base::TimeTicks::Now(),
    };
  } else {
    bool has_charge_in_ah = power_status->has_battery_charge() &&
                            power_status->has_battery_charge_full();
    bool has_charge_in_percent = power_status->has_battery_percent();

    std::optional<uint64_t> charge;
    std::optional<uint64_t> charge_full;
    std::optional<base::BatteryLevelProvider::BatteryLevelUnit> charge_unit =
        std::nullopt;

    // There are 3 ways the power manager could report the battery capacity,
    // affecting how the battery state will be constructed. In order of
    // preference:
    //
    // 1- Power Manager reports the charge in Ah, and reports voltage. We report
    // the capacity in mWh.
    //
    // 2- Power Manager reports the charge in Ah, and does not report voltage.
    // We report the capacity in mAh.
    //
    // 3- Power Manager reports the charge as a percentage. We report the charge
    // as relative (percentage).
    if (has_charge_in_ah) {
      if (power_status->has_battery_voltage()) {
        charge = power_status->battery_charge() * kmAhPerAh *
                 power_status->battery_voltage();
        charge_full = power_status->battery_charge_full() * kmAhPerAh *
                      power_status->battery_voltage();
        charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMWh;
      } else {
        charge = power_status->battery_charge() * kmAhPerAh;
        charge_full = power_status->battery_charge_full() * kmAhPerAh;
        charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kMAh;
      }
    } else if (has_charge_in_percent) {
      charge = power_status->battery_percent();
      charge_full = 100;
      charge_unit = base::BatteryLevelProvider::BatteryLevelUnit::kRelative;
    }

    state = {
        .battery_count = 1,
        .is_external_power_connected =
            power_status->external_power() !=
            power_manager::PowerSupplyProperties::DISCONNECTED,
        .current_capacity = charge,
        .full_charged_capacity = charge_full,
        .charge_unit = charge_unit,
        // TODO(anthonyvd): This should be the time that the state was last
        // sampled from the system, but PowerManagerClient doesn't yet provide
        // this information.
        .capture_time = base::TimeTicks::Now(),
    };
  }

  std::move(callback).Run(state);
}

}  // namespace performance_manager::power
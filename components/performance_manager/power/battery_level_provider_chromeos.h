// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_POWER_BATTERY_LEVEL_PROVIDER_CHROMEOS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_POWER_BATTERY_LEVEL_PROVIDER_CHROMEOS_H_

#include "base/memory/raw_ptr.h"
#include "base/power_monitor/battery_level_provider.h"

namespace chromeos {
class PowerManagerClient;
}

namespace performance_manager::power {

class BatteryLevelProviderChromeOS : public base::BatteryLevelProvider {
 public:
  explicit BatteryLevelProviderChromeOS(
      chromeos::PowerManagerClient* power_manager_client);
  ~BatteryLevelProviderChromeOS() override;

  static std::unique_ptr<base::BatteryLevelProvider> Create();

 private:
  friend class BatteryLevelProviderChromeOSTest;

  // base::BatteryLevelProvider:
  void GetBatteryState(
      base::OnceCallback<
          void(const std::optional<base::BatteryLevelProvider::BatteryState>&)>
          callback) override;

  raw_ptr<chromeos::PowerManagerClient, DanglingUntriaged>
      power_manager_client_;
};

}  // namespace performance_manager::power

#endif  // COMPONENTS_PERFORMANCE_MANAGER_POWER_BATTERY_LEVEL_PROVIDER_CHROMEOS_H_

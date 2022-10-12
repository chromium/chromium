// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_POWER_BATTERY_LEVEL_PROVIDER_CREATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_POWER_BATTERY_LEVEL_PROVIDER_CREATOR_H_

#include <memory>

namespace base {
class BatteryLevelProvider;
}  // namespace base

namespace performance_manager::power {

std::unique_ptr<base::BatteryLevelProvider> CreateBatteryLevelProvider();

}  // namespace performance_manager::power

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_POWER_BATTERY_LEVEL_PROVIDER_CREATOR_H_

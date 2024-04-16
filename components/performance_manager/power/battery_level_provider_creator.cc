// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/power/battery_level_provider_creator.h"

#include <utility>

#include "base/power_monitor/battery_level_provider.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/performance_manager/power/battery_level_provider_chromeos.h"
#endif

namespace performance_manager::power {

std::unique_ptr<base::BatteryLevelProvider> CreateBatteryLevelProvider() {
  // TODO(crbug.com/40871810): Move all of the creation code into the
  // platform-specific implementations once they're moved to components.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return performance_manager::power::BatteryLevelProviderChromeOS::Create();
#else
  return base::BatteryLevelProvider::Create();
#endif
}

}  // namespace performance_manager::power

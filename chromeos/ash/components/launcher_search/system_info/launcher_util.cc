// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/launcher_search/system_info/launcher_util.h"

#include <string>
#include <string_view>
#include <vector>

#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/components/system_info/system_info_util.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace launcher_search {

void PopulatePowerStatus(const power_manager::PowerSupplyProperties& proto,
                         system_info::BatteryHealth& battery_health) {
  bool charging = proto.battery_state() ==
                  power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  bool calculating = proto.is_calculating_battery_time();
  int percent = system_info::GetRoundedBatteryPercent(proto.battery_percent());
  DCHECK(percent <= 100 && percent >= 0);
  base::TimeDelta time_left;
  bool show_time = false;

  if (!calculating) {
    time_left = base::Seconds(charging ? proto.battery_time_to_full_sec()
                                       : proto.battery_time_to_empty_sec());
    show_time = system_info::ShouldDisplayBatteryTime(time_left);
  }

  std::u16string status_text;
  std::u16string accessibility_string;
  if (show_time) {
    status_text = l10n_util::GetStringFUTF16(
        charging ? IDS_ASH_BATTERY_STATUS_CHARGING_IN_LAUNCHER_DESCRIPTION_LEFT
                 : IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_DESCRIPTION_LEFT,
        base::NumberToString16(percent),
        system_info::GetBatteryTimeText(time_left));
    accessibility_string = l10n_util::GetStringFUTF16(
        charging
            ? IDS_ASH_BATTERY_STATUS_CHARGING_IN_LAUNCHER_ACCESSIBILITY_LABEL
            : IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_ACCESSIBILITY_LABEL,
        base::NumberToString16(percent),
        system_info::GetBatteryTimeText(time_left));
  } else {
    status_text = l10n_util::GetStringFUTF16(
        IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_DESCRIPTION_LEFT_SHORT,
        base::NumberToString16(percent));
    accessibility_string = l10n_util::GetStringFUTF16(
        IDS_ASH_BATTERY_STATUS_IN_LAUNCHER_ACCESSIBILITY_LABEL_SHORT,
        base::NumberToString16(percent));
  }

  battery_health.SetPowerTime(status_text);
  battery_health.SetAccessibilityLabel(accessibility_string);
  battery_health.SetBatteryPercentage(percent);
}

std::vector<SystemInfoKeywordInput> GetSystemInfoKeywordVector() {
  return {
      SystemInfoKeywordInput(
          SystemInfoInputType::kVersion,
          l10n_util::GetStringUTF16(IDS_ASH_VERSION_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kVersion,
          l10n_util::GetStringUTF16(IDS_ASH_DEVICE_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kVersion,
          l10n_util::GetStringUTF16(IDS_ASH_ABOUT_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kBattery,
          l10n_util::GetStringUTF16(IDS_ASH_BATTERY_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kMemory,
          l10n_util::GetStringUTF16(IDS_ASH_MEMORY_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kMemory,
          l10n_util::GetStringUTF16(IDS_ASH_RAM_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kCPU,
          l10n_util::GetStringUTF16(
              IDS_ASH_ACTIVITY_MONITOR_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kStorage,
          l10n_util::GetStringUTF16(IDS_ASH_STORAGE_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kCPU,
          l10n_util::GetStringUTF16(IDS_ASH_CPU_KEYWORD_FOR_LAUNCHER)),
      SystemInfoKeywordInput(
          SystemInfoInputType::kCPU,
          l10n_util::GetStringUTF16(IDS_ASH_DEVICE_SLOW_KEYWORD_FOR_LAUNCHER))};
}

}  // namespace launcher_search

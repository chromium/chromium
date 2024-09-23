// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_SYSTEM_INFO_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_SYSTEM_INFO_UTIL_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chromeos/ash/components/system_info/battery_health.h"
#include "chromeos/ash/components/system_info/cpu_data.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace system_info {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml: CrosDiagnosticsDataError.
enum class BatteryDataError {
  // Null or nullptr value.
  kNoData = 0,
  // For numeric values that are NaN.
  kNotANumber = 1,
  // Expectation about data not met. Ex. routing prefix is between zero and
  // thirty-two.
  kExpectationNotMet = 2,
  kMaxValue = kExpectationNotMet,
};

// Checks if current battery time is excessively long (more than 1 day) or short
// (less than 1 minute). If so, the time should not be displayed as if the
// current is close to 0, then the time estimates can be excessively large.
bool COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    ShouldDisplayBatteryTime(const base::TimeDelta& time);

// Converts the double of the battery percentage into an int, which has a
// minimum value of 1.
int COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    GetRoundedBatteryPercent(double battery_percent);

// Copies the hour and minute components of `time` to `hours` and `minutes`.
// The minute component is rounded rather than truncated: a `time` value
// corresponding to 92 seconds will produce a `minutes` value of 2, for
// example.
void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    SplitTimeIntoHoursAndMinutes(const base::TimeDelta& time,
                                 int* hours,
                                 int* minutes);

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    EmitBatteryDataError(BatteryDataError error,
                         const std::string& histogram_prefix);

// Extracts MemoryInfo
//         from `info`.Logs and returns a nullptr if MemoryInfo in not present
ash::cros_healthd::mojom::MemoryInfo* COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    GetMemoryInfo(const ash::cros_healthd::mojom::TelemetryInfo& info,
                  const std::string& metric_name_for_histogram);

// Extracts CpuInfo from `info`. Logs and returns a nullptr if CpuInfo
// in not present.
ash::cros_healthd::mojom::CpuInfo* COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    GetCpuInfo(const ash::cros_healthd::mojom::TelemetryInfo& info,
               const std::string& metric_name_for_histogram);

// Extracts BatteryInfo from `info`. Logs and returns a nullptr if
// BatteryInfo in not present.
const ash::cros_healthd::mojom::BatteryInfo* COMPONENT_EXPORT(
    CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    GetBatteryInfo(const ash::cros_healthd::mojom::TelemetryInfo& info,
                   const std::string& metric_name_for_histogram,
                   const std::string& battery_error_histogram);

CpuUsageData COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    CalculateCpuUsage(
        const std::vector<ash::cros_healthd::mojom::LogicalCpuInfoPtr>&
            logical_cpu_infos);

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    PopulateCpuUsage(CpuUsageData new_cpu_usage_data,
                     CpuUsageData previous_cpu_usage_data,
                     CpuData& cpu_usage);
void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    PopulateAverageCpuTemperature(
        const ash::cros_healthd::mojom::CpuInfo& cpu_info,
        CpuData& cpu_usage);
void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    PopulateAverageScaledClockSpeed(
        const ash::cros_healthd::mojom::CpuInfo& cpu_info,
        CpuData& out_cpu_usage);

void COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    PopulateBatteryHealth(
        const ash::cros_healthd::mojom::BatteryInfo& battery_info,
        BatteryHealth& battery_health);

std::u16string COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO)
    GetBatteryTimeText(base::TimeDelta time_left);

}  // namespace system_info

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_INFO_SYSTEM_INFO_UTIL_H_

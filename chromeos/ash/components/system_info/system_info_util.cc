// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system_info/system_info_util.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace system_info {
namespace {

namespace healthd = ash::cros_healthd::mojom;

constexpr int kMilliampsInAnAmp = 1000;

void EmitCrosHealthdProbeError(std::string_view source_type,
                               healthd::ErrorType error_type,
                               const std::string& metric_name_for_histogram) {
  // `metric_name` may be empty in which case we do not want a metric send
  // attempted.
  if (metric_name_for_histogram.empty()) {
    LOG(WARNING)
        << "Ignoring request to record metric for ProbeError of error_type: "
        << error_type << " for unknown source_stuct: " << source_type;
    return;
  }

  base::UmaHistogramEnumeration(metric_name_for_histogram, error_type);
}

template <typename TResult, typename TTag>

bool CheckResponse(const TResult& result,
                   TTag expected_tag,
                   std::string_view type_name,
                   const std::string& metric_name_for_histogram) {
  if (result.is_null()) {
    LOG(ERROR) << type_name << "not found in croshealthd response.";
    return false;
  }

  auto tag = result->which();
  if (tag == TTag::kError) {
    EmitCrosHealthdProbeError(type_name, result->get_error()->type,
                              metric_name_for_histogram);
    LOG(ERROR) << "Error retrieving " << type_name
               << "from croshealthd: " << result->get_error()->msg;
    return false;
  }

  DCHECK_EQ(tag, expected_tag);

  return true;
}

}  // namespace

bool ShouldDisplayBatteryTime(const base::TimeDelta& time) {
  // Put limits on the maximum and minimum battery time-to-full or time-to-empty
  // that should be displayed in the UI. If the current is close to zero,
  // battery time estimates can get very large; avoid displaying these large
  // numbers.
  return time >= base::Minutes(1) && time <= base::Days(1);
}

int GetRoundedBatteryPercent(double battery_percent) {
  // Minimum battery percentage rendered in UI.
  constexpr int kMinBatteryPercent = 1;
  return std::max(kMinBatteryPercent, base::ClampRound(battery_percent));
}

void SplitTimeIntoHoursAndMinutes(const base::TimeDelta& time,
                                  int* hours,
                                  int* minutes) {
  DCHECK(hours);
  DCHECK(minutes);
  *minutes = base::ClampRound(time / base::Minutes(1));
  *hours = *minutes / 60;
  *minutes %= 60;
}

void EmitBatteryDataError(BatteryDataError error,
                          const std::string& histogram_prefix) {
  if (!histogram_prefix.empty()) {
    base::UmaHistogramEnumeration(histogram_prefix, error);
  }
}

healthd::MemoryInfo* GetMemoryInfo(
    const healthd::TelemetryInfo& info,
    const std::string& metric_name_for_histogram) {
  const healthd::MemoryResultPtr& memory_result = info.memory_result;
  if (!CheckResponse(memory_result, healthd::MemoryResult::Tag::kMemoryInfo,
                     "memory info", metric_name_for_histogram)) {
    return nullptr;
  }

  return memory_result->get_memory_info().get();
}

healthd::CpuInfo* GetCpuInfo(const healthd::TelemetryInfo& info,
                             const std::string& metric_name_for_histogram) {
  const healthd::CpuResultPtr& cpu_result = info.cpu_result;
  if (!CheckResponse(cpu_result, healthd::CpuResult::Tag::kCpuInfo, "cpu info",
                     metric_name_for_histogram)) {
    return nullptr;
  }

  return cpu_result->get_cpu_info().get();
}

const healthd::BatteryInfo* GetBatteryInfo(
    const healthd::TelemetryInfo& info,
    const std::string& metric_name_for_histogram,
    const std::string& battery_error_histogram) {
  const healthd::BatteryResultPtr& battery_result = info.battery_result;
  if (!CheckResponse(battery_result, healthd::BatteryResult::Tag::kBatteryInfo,
                     "battery info", metric_name_for_histogram)) {
    return nullptr;
  }

  const healthd::BatteryInfo* battery_info =
      battery_result->get_battery_info().get();
  if (battery_info->charge_full == 0) {
    LOG(ERROR) << "charge_full from battery_info should not be zero.";
    EmitBatteryDataError(BatteryDataError::kExpectationNotMet,
                         battery_error_histogram);
    return nullptr;
  }

  // Handle values in battery_info which could cause a SIGFPE. See b/227485637.
  if (isnan(battery_info->charge_full) ||
      isnan(battery_info->charge_full_design) ||
      battery_info->charge_full_design == 0) {
    LOG(ERROR) << "battery_info values could cause SIGFPE crash: { "
               << "charge_full_design: " << battery_info->charge_full_design
               << ", charge_full: " << battery_info->charge_full << " }";
    return nullptr;
  }

  return battery_info;
}

CpuUsageData CalculateCpuUsage(
    const std::vector<healthd::LogicalCpuInfoPtr>& logical_cpu_infos) {
  CpuUsageData new_usage_data;

  DCHECK_GE(logical_cpu_infos.size(), 1u);
  for (const auto& logical_cpu_ptr : logical_cpu_infos) {
    new_usage_data += CpuUsageData(logical_cpu_ptr->user_time_user_hz,
                                   logical_cpu_ptr->system_time_user_hz,
                                   logical_cpu_ptr->idle_time_user_hz);
  }

  return new_usage_data;
}

void PopulateCpuUsage(CpuUsageData new_cpu_usage_data,
                      CpuUsageData previous_cpu_usage_data,
                      CpuData& cpu_usage) {
  CpuUsageData delta = new_cpu_usage_data - previous_cpu_usage_data;

  const uint64_t total_delta = delta.GetTotalTime();
  if (total_delta == 0) {
    LOG(ERROR) << "Device reported having zero logical CPUs.";
    return;
  }
  cpu_usage.SetPercentUsageUser(100 * delta.GetUserTime() / total_delta);
  cpu_usage.SetPercentUsageSystem(100 * delta.GetSystemTime() / total_delta);
  cpu_usage.SetPercentUsageFree(100 * delta.GetIdleTime() / total_delta);
}

void PopulateAverageCpuTemperature(
    const ash::cros_healthd::mojom::CpuInfo& cpu_info,
    CpuData& cpu_usage) {
  if (cpu_info.temperature_channels.empty()) {
    LOG(ERROR) << "Device reported having 0 temperature channels.";
    return;
  }

  uint32_t cumulative_total = 0;
  for (const auto& temp_channel_ptr : cpu_info.temperature_channels) {
    cumulative_total += temp_channel_ptr->temperature_celsius;
  }

  // Integer division.
  cpu_usage.SetAverageCpuTempCelsius(cumulative_total /
                                     cpu_info.temperature_channels.size());
}

void PopulateAverageScaledClockSpeed(const healthd::CpuInfo& cpu_info,
                                     CpuData& cpu_usage) {
  if (cpu_info.physical_cpus.empty() ||
      cpu_info.physical_cpus[0]->logical_cpus.empty()) {
    LOG(ERROR) << "Device reported having 0 logical CPUs.";
    return;
  }

  uint32_t total_scaled_ghz = 0;
  for (const auto& logical_cpu_ptr : cpu_info.physical_cpus[0]->logical_cpus) {
    total_scaled_ghz += logical_cpu_ptr->scaling_current_frequency_khz;
  }

  // Integer division.
  cpu_usage.SetScalingAverageCurrentFrequencyKhz(
      total_scaled_ghz / cpu_info.physical_cpus[0]->logical_cpus.size());
}

void PopulateBatteryHealth(const healthd::BatteryInfo& battery_info,
                           BatteryHealth& battery_health) {
  battery_health.SetCycleCount(battery_info.cycle_count);

  double charge_full_now_milliamp_hours =
      battery_info.charge_full * kMilliampsInAnAmp;
  double charge_full_design_milliamp_hours =
      battery_info.charge_full_design * kMilliampsInAnAmp;
  battery_health.SetBatteryWearPercentage(
      std::min({(100 * charge_full_now_milliamp_hours /
                 charge_full_design_milliamp_hours),
                100.0}));
}

std::u16string GetBatteryTimeText(base::TimeDelta time_left) {
  int hour = 0;
  int min = 0;
  SplitTimeIntoHoursAndMinutes(time_left, &hour, &min);

  std::u16string time_text;
  if (hour == 0 || min == 0) {
    // Display only one unit ("2 hours" or "10 minutes").
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG, time_left);
  }

  return ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  -1,  // force hour and minute output
                                  time_left);
}

}  // namespace system_info

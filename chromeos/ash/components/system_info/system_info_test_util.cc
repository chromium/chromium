// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system_info/system_info_test_util.h"

#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"

namespace system_info {

namespace {

namespace healthd_mojom = ash::cros_healthd::mojom;

}  // namespace

void SetUpTest() {
  chromeos::PowerManagerClient::InitializeFake();
  ash::cros_healthd::FakeCrosHealthd::Initialize();
}

void TestDownTest() {
  ash::cros_healthd::FakeCrosHealthd::Shutdown();
  chromeos::PowerManagerClient::Shutdown();
}

void SetProbeTelemetryInfoResponse(healthd_mojom::BatteryInfoPtr battery_info,
                                   healthd_mojom::CpuInfoPtr cpu_info,
                                   healthd_mojom::MemoryInfoPtr memory_info) {
  auto info = healthd_mojom::TelemetryInfo::New();
  if (battery_info) {
    info->battery_result =
        healthd_mojom::BatteryResult::NewBatteryInfo(std::move(battery_info));
  }
  if (memory_info) {
    info->memory_result =
        healthd_mojom::MemoryResult::NewMemoryInfo(std::move(memory_info));
  }
  if (cpu_info) {
    info->cpu_result =
        healthd_mojom::CpuResult::NewCpuInfo(std::move(cpu_info));
  }

  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);
}

void SetCrosHealthdCpuResponse(
    const std::vector<system_info::CpuUsageData>& usage_data,
    const std::vector<int32_t>& cpu_temps,
    const std::vector<uint32_t>& scaled_cpu_clock_speed) {
  auto cpu_info_ptr = healthd_mojom::CpuInfo::New();
  auto physical_cpu_info_ptr = healthd_mojom::PhysicalCpuInfo::New();

  DCHECK_EQ(usage_data.size(), scaled_cpu_clock_speed.size());
  for (size_t i = 0; i < usage_data.size(); ++i) {
    const auto& data = usage_data[i];
    auto logical_cpu_info_ptr = healthd_mojom::LogicalCpuInfo::New();

    logical_cpu_info_ptr->user_time_user_hz = data.GetUserTime();
    logical_cpu_info_ptr->system_time_user_hz = data.GetSystemTime();
    logical_cpu_info_ptr->idle_time_user_hz = data.GetIdleTime();

    logical_cpu_info_ptr->scaling_current_frequency_khz =
        scaled_cpu_clock_speed[i];

    physical_cpu_info_ptr->logical_cpus.emplace_back(
        std::move(logical_cpu_info_ptr));
  }

  cpu_info_ptr->physical_cpus.push_back(std::move(physical_cpu_info_ptr));
  for (const auto& cpu_temp : cpu_temps) {
    auto cpu_temp_channel_ptr = healthd_mojom::CpuTemperatureChannel::New();
    cpu_temp_channel_ptr->temperature_celsius = cpu_temp;
    cpu_info_ptr->temperature_channels.emplace_back(
        std::move(cpu_temp_channel_ptr));
  }

  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr,
                                std::move(cpu_info_ptr),
                                /*memory_info=*/nullptr);
}

void SetCrosHealthdMemoryUsageResponse(uint32_t total_memory_kib,
                                       uint32_t free_memory_kib,
                                       uint32_t available_memory_kib) {
  healthd_mojom::MemoryInfoPtr memory_info = healthd_mojom::MemoryInfo::New(
      total_memory_kib, free_memory_kib, available_memory_kib,
      /*page_faults_since_last_boot=*/0);
  SetProbeTelemetryInfoResponse(/*battery_info=*/nullptr, /*cpu_info=*/nullptr,
                                /*memory_info=*/std::move(memory_info));
}

healthd_mojom::BatteryInfoPtr CreateCrosHealthdBatteryHealthResponse(
    double charge_full_now,
    double charge_full_design,
    int32_t cycle_count) {
  healthd_mojom::NullableUint64Ptr temp_value_ptr(
      healthd_mojom::NullableUint64::New());
  auto battery_info = healthd_mojom::BatteryInfo::New(
      /*cycle_count=*/cycle_count, /*voltage_now=*/0,
      /*vendor=*/"",
      /*serial_number=*/"", /*charge_full_design=*/charge_full_design,
      /*charge_full=*/charge_full_now,
      /*voltage_min_design=*/0,
      /*model_name=*/"",
      /*charge_now=*/0,
      /*current_now=*/0,
      /*technology=*/"",
      /*status=*/"",
      /*manufacture_date=*/std::nullopt, std::move(temp_value_ptr));
  return battery_info;
}

void SetCrosHealthdBatteryHealthResponse(double charge_full_now,
                                         double charge_full_design,
                                         int32_t cycle_count) {
  healthd_mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryHealthResponse(charge_full_now,
                                             charge_full_design, cycle_count);
  SetProbeTelemetryInfoResponse(std::move(battery_info),
                                /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr);
}

bool AreValidPowerTimes(int64_t time_to_full, int64_t time_to_empty) {
  // Exactly one of `time_to_full` or `time_to_empty` must be zero. The other
  // can be a positive integer to represent the time to charge/discharge or -1
  // to represent that the time is being calculated.
  return (time_to_empty == 0 && (time_to_full > 0 || time_to_full == -1)) ||
         (time_to_full == 0 && (time_to_empty > 0 || time_to_empty == -1));
}

power_manager::PowerSupplyProperties ConstructPowerSupplyProperties(
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty,
    double battery_percent) {
  power_manager::PowerSupplyProperties props;
  props.set_external_power(power_source);
  props.set_battery_state(battery_state);

  if (battery_state ==
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT) {
    // Leave `time_to_full` and `time_to_empty` unset.
    return props;
  }

  DCHECK(AreValidPowerTimes(time_to_full, time_to_empty));

  props.set_is_calculating_battery_time(is_calculating_battery_time);
  props.set_battery_time_to_full_sec(time_to_full);
  props.set_battery_time_to_empty_sec(time_to_empty);
  props.set_battery_percent(battery_percent);

  return props;
}

void SetPowerManagerProperties(
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty,
    double battery_percent) {
  power_manager::PowerSupplyProperties props = ConstructPowerSupplyProperties(
      power_source, battery_state, is_calculating_battery_time, time_to_full,
      time_to_empty, battery_percent);
  chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);
}

healthd_mojom::ProbeErrorPtr CreateProbeError(
    healthd_mojom::ErrorType error_type) {
  auto probe_error = healthd_mojom::ProbeError::New();
  probe_error->type = error_type;
  probe_error->msg = "probe error";
  return probe_error;
}

}  // namespace system_info

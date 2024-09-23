// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sparky/system_info_delegate_impl.h"

#include <memory>

#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/system_info/cpu_usage_data.h"
#include "chromeos/ash/components/system_info/system_info_util.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sparky {
namespace {

namespace healthd_mojom = ash::cros_healthd::mojom;

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

ash::cros_healthd::mojom::CpuInfoPtr GetCpuResponse(
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
  return cpu_info_ptr;
}

void SetCrosHealthdCpuResponse(
    const std::vector<system_info::CpuUsageData>& usage_data,
    const std::vector<int32_t>& cpu_temps,
    const std::vector<uint32_t>& scaled_cpu_clock_speed) {
  auto cpu_info_ptr =
      GetCpuResponse(usage_data, cpu_temps, scaled_cpu_clock_speed);

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

// Constructs a BatteryInfoPtr.
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

// Sets the PowerSupplyProperties on FakePowerManagerClient. Calling this
// method immediately notifies PowerManagerClient observers. One of
// `time_to_full` or `time_to_empty` must be either -1 or a positive number.
// The other must be 0. If `battery_state` is NOT_PRESENT, both `time_to_full`
// and `time_to_empty` will be left unset.
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

}  // namespace

class SystemInfoDelegateImplTest : public testing::Test {
 public:
  SystemInfoDelegateImplTest() = default;

  SystemInfoDelegateImplTest(const SystemInfoDelegateImplTest&) = delete;
  SystemInfoDelegateImplTest& operator=(const SystemInfoDelegateImplTest&) =
      delete;

  ~SystemInfoDelegateImplTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::cros_healthd::FakeCrosHealthd::Initialize();
    system_info_delegate_impl_ = std::make_unique<SystemInfoDelegateImpl>();
    Wait();
  }

  void TearDown() override {
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  void OnDiagnosticsReceived(
      std::unique_ptr<manta::DiagnosticsData> diagnostics_data) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  std::unique_ptr<SystemInfoDelegateImpl> system_info_delegate_impl_;
};

TEST_F(SystemInfoDelegateImplTest, Memory) {
  const uint32_t total_memory_kib = 8000000;
  const uint32_t free_memory_kib = 2000000;
  const uint32_t available_memory_kib = 4000000;

  SetCrosHealthdMemoryUsageResponse(total_memory_kib, free_memory_kib,
                                    available_memory_kib);
  Wait();
  auto quit_closure = task_environment_.QuitClosure();

  system_info_delegate_impl_->ObtainDiagnostics(
      {manta::Diagnostics::kMemory},
      base::BindLambdaForTesting(
          [&quit_closure,
           this](std::unique_ptr<manta::DiagnosticsData> diagnostics_data) {
            Wait();
            ASSERT_TRUE(diagnostics_data->memory_data);
            ASSERT_DOUBLE_EQ(diagnostics_data->memory_data->available_memory_gb,
                             3.814697265625);
            ASSERT_DOUBLE_EQ(diagnostics_data->memory_data->total_memory_gb,
                             7.62939453125);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(SystemInfoDelegateImplTest, CPU) {
  int temp_1 = 40;
  int temp_2 = 50;
  int temp_3 = 15;
  uint32_t core_1_speed = 4000000;
  uint32_t core_2_speed = 2000000;
  system_info::CpuUsageData core_1(1000, 1000, 1000);
  system_info::CpuUsageData core_2(2000, 2000, 2000);

  SetCrosHealthdCpuResponse({core_1, core_2}, {temp_1, temp_2, temp_3},
                            {core_1_speed, core_2_speed});
  Wait();
  auto quit_closure = task_environment_.QuitClosure();

  system_info_delegate_impl_->ObtainDiagnostics(
      {manta::Diagnostics::kCpu},
      base::BindLambdaForTesting(
          [&quit_closure,
           this](std::unique_ptr<manta::DiagnosticsData> diagnostics_data) {
            Wait();
            ASSERT_TRUE(diagnostics_data->cpu_data);
            ASSERT_EQ(diagnostics_data->cpu_data->average_cpu_temp_celsius, 35);
            ASSERT_EQ(diagnostics_data->cpu_data->scaling_current_frequency_ghz,
                      3.0);
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(SystemInfoDelegateImplTest, Battery) {
  const double charge_full_now = 20;
  const double charge_full_design = 26;
  const int32_t cycle_count = 500;

  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      cycle_count);

  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = false;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;
  const double battery_percent = 94.0;

  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs, battery_percent);
  Wait();
  auto quit_closure = task_environment_.QuitClosure();

  system_info_delegate_impl_->ObtainDiagnostics(
      {manta::Diagnostics::kBattery},
      base::BindLambdaForTesting(
          [&quit_closure,
           this](std::unique_ptr<manta::DiagnosticsData> diagnostics_data) {
            Wait();
            ASSERT_TRUE(diagnostics_data->battery_data);
            ASSERT_EQ(diagnostics_data->battery_data->battery_percentage, 94);
            ASSERT_EQ(diagnostics_data->battery_data->battery_wear_percentage,
                      76);
            ASSERT_EQ(diagnostics_data->battery_data->cycle_count, 500);
            ASSERT_EQ(diagnostics_data->battery_data->power_time,
                      "17 minutes until full");
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

TEST_F(SystemInfoDelegateImplTest, MultipleFields) {
  const uint32_t total_memory_kib = 8000000;
  const uint32_t free_memory_kib = 2000000;
  const uint32_t available_memory_kib = 4000000;

  int temp_1 = 40;
  int temp_2 = 50;
  int temp_3 = 15;
  uint32_t core_1_speed = 4000000;
  uint32_t core_2_speed = 2000000;
  system_info::CpuUsageData core_1(1000, 1000, 1000);
  system_info::CpuUsageData core_2(2000, 2000, 2000);

  healthd_mojom::MemoryInfoPtr memory_info = healthd_mojom::MemoryInfo::New(
      total_memory_kib, free_memory_kib, available_memory_kib,
      /*page_faults_since_last_boot=*/0);
  auto cpu_info = GetCpuResponse({core_1, core_2}, {temp_1, temp_2, temp_3},
                                 {core_1_speed, core_2_speed});
  const double charge_full_now = 20;

  const double charge_full_design = 26;
  const int32_t cycle_count = 500;

  healthd_mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryHealthResponse(charge_full_now,
                                             charge_full_design, cycle_count);

  SetProbeTelemetryInfoResponse(std::move(battery_info), std::move(cpu_info),
                                std::move(memory_info));

  Wait();

  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = false;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;
  const double battery_percent = 94.0;

  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs, battery_percent);
  Wait();
  auto quit_closure = task_environment_.QuitClosure();

  system_info_delegate_impl_->ObtainDiagnostics(
      {manta::Diagnostics::kMemory, manta::Diagnostics::kCpu,
       manta::Diagnostics::kBattery},
      base::BindLambdaForTesting(
          [&quit_closure,
           this](std::unique_ptr<manta::DiagnosticsData> diagnostics_data) {
            Wait();
            ASSERT_TRUE(diagnostics_data->memory_data);
            ASSERT_DOUBLE_EQ(diagnostics_data->memory_data->available_memory_gb,
                             3.814697265625);
            ASSERT_DOUBLE_EQ(diagnostics_data->memory_data->total_memory_gb,
                             7.62939453125);
            ASSERT_TRUE(diagnostics_data->cpu_data);
            ASSERT_EQ(diagnostics_data->cpu_data->average_cpu_temp_celsius, 35);
            ASSERT_EQ(diagnostics_data->cpu_data->scaling_current_frequency_ghz,
                      3.0);
            ASSERT_TRUE(diagnostics_data->battery_data);
            ASSERT_EQ(diagnostics_data->battery_data->battery_percentage, 94);
            ASSERT_EQ(diagnostics_data->battery_data->battery_wear_percentage,
                      76);
            ASSERT_EQ(diagnostics_data->battery_data->cycle_count, 500);
            ASSERT_EQ(diagnostics_data->battery_data->power_time,
                      "17 minutes until full");
            quit_closure.Run();
          }));
  task_environment_.RunUntilQuit();
}

}  // namespace sparky

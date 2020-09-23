// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_data_provider.h"

#include <cstdint>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace diagnostics {
namespace {

void SetProbeTelemetryInfoResponse(
    cros_healthd::mojom::BatteryInfoPtr battery_info,
    cros_healthd::mojom::CpuInfoPtr cpu_info,
    cros_healthd::mojom::MemoryInfoPtr memory_info,
    cros_healthd::mojom::SystemInfoPtr system_info) {
  auto info = cros_healthd::mojom::TelemetryInfo::New();
  if (system_info) {
    info->system_result = cros_healthd::mojom::SystemResult::NewSystemInfo(
        std::move(system_info));
  }
  if (battery_info) {
    info->battery_result = cros_healthd::mojom::BatteryResult::NewBatteryInfo(
        std::move(battery_info));
  }
  if (memory_info) {
    info->memory_result = cros_healthd::mojom::MemoryResult::NewMemoryInfo(
        std::move(memory_info));
  }
  if (cpu_info) {
    info->cpu_result =
        cros_healthd::mojom::CpuResult::NewCpuInfo(std::move(cpu_info));
  }

  cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(info);
}

void SetCrosHealthdSystemInfoResponse(const std::string& board_name,
                                      const std::string& cpu_model,
                                      uint32_t total_memory_kib,
                                      uint16_t cpu_threads_count,
                                      bool has_battery,
                                      const std::string& milestone_version) {
  // System info
  auto system_info = cros_healthd::mojom::SystemInfo::New();
  system_info->product_name = base::Optional<std::string>(board_name);
  auto os_version_info = cros_healthd::mojom::OsVersion::New();
  os_version_info->release_milestone = milestone_version;
  system_info->os_version = std::move(os_version_info);

  // Battery info
  auto battery_info =
      has_battery ? cros_healthd::mojom::BatteryInfo::New() : nullptr;

  // Memory info
  auto memory_info = cros_healthd::mojom::MemoryInfo::New();
  memory_info->total_memory_kib = total_memory_kib;

  // CPU info
  auto cpu_info = cros_healthd::mojom::CpuInfo::New();
  auto physical_cpu_info = cros_healthd::mojom::PhysicalCpuInfo::New();
  physical_cpu_info->model_name = cpu_model;
  cpu_info->num_total_threads = cpu_threads_count;
  cpu_info->physical_cpus.emplace_back(std::move(physical_cpu_info));

  SetProbeTelemetryInfoResponse(std::move(battery_info), std::move(cpu_info),
                                std::move(memory_info), std::move(system_info));
}

// Constructs a BatteryInfoPtr. If |temperature| = 0, it will be omitted from
// the response to simulate an empty temperature field.
cros_healthd::mojom::BatteryInfoPtr CreateCrosHealthdBatteryInfoResponse(
    int64_t cycle_count,
    double voltage_now,
    const std::string& vendor,
    const std::string& serial_number,
    double charge_full_design,
    double charge_full,
    double voltage_min_design,
    const std::string& model_name,
    double charge_now,
    double current_now,
    const std::string& technology,
    const std::string& status,
    const base::Optional<std::string>& manufacture_date,
    uint64_t temperature) {
  cros_healthd::mojom::UInt64ValuePtr temp_value_ptr(
      cros_healthd::mojom::UInt64Value::New());
  if (temperature != 0) {
    temp_value_ptr->value = temperature;
  }
  auto battery_info = cros_healthd::mojom::BatteryInfo::New(
      cycle_count, voltage_now, vendor, serial_number, charge_full_design,
      charge_full, voltage_min_design, model_name, charge_now, current_now,
      technology, status, manufacture_date, std::move(temp_value_ptr));
  return battery_info;
}

cros_healthd::mojom::BatteryInfoPtr CreateCrosHealthdBatteryInfoResponse(
    const std::string& vendor,
    double charge_full_design) {
  return CreateCrosHealthdBatteryInfoResponse(
      /*cycle_count=*/0,
      /*voltage_now=*/0,
      /*vendor=*/vendor,
      /*serial_number=*/"",
      /*charge_full_design=*/charge_full_design,
      /*charge_full=*/0,
      /*voltage_min_design=*/0,
      /*model_name=*/"",
      /*charge_now=*/0,
      /*current_now=*/0,
      /*technology=*/"",
      /*status=*/"",
      /*manufacture_date=*/base::nullopt,
      /*temperature=*/0);
}

void SetCrosHealthdBatteryInfoResponse(const std::string& vendor,
                                       double charge_full_design) {
  cros_healthd::mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryInfoResponse(vendor, charge_full_design);
  SetProbeTelemetryInfoResponse(std::move(battery_info), /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*memory_info=*/nullptr);
}

}  // namespace

class SystemDataProviderTest : public testing::Test {
 public:
  SystemDataProviderTest() {
    chromeos::CrosHealthdClient::InitializeFake();
    system_data_provider_ = std::make_unique<SystemDataProvider>();
  }

  ~SystemDataProviderTest() override {
    chromeos::CrosHealthdClient::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<SystemDataProvider> system_data_provider_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SystemDataProviderTest, GetSystemInfo) {
  const std::string expected_board_name = "board_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";

  SetCrosHealthdSystemInfoResponse(
      expected_board_name, expected_cpu_model, expected_total_memory_kib,
      expected_cpu_threads_count, expected_has_battery,
      expected_milestone_version);

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        EXPECT_EQ(expected_board_name, ptr->board_name);
        EXPECT_EQ(expected_cpu_model, ptr->cpu_model_name);
        EXPECT_EQ(expected_total_memory_kib, ptr->total_memory_kib);
        EXPECT_EQ(expected_cpu_threads_count, ptr->cpu_threads_count);
        EXPECT_EQ(expected_milestone_version,
                  ptr->version_info->milestone_version);
        EXPECT_EQ(expected_has_battery, ptr->device_capabilities->has_battery);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemDataProviderTest, NoBattery) {
  const std::string expected_board_name = "board_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const bool expected_has_battery = false;
  const std::string expected_milestone_version = "M99";

  SetCrosHealthdSystemInfoResponse(
      expected_board_name, expected_cpu_model, expected_total_memory_kib,
      expected_cpu_threads_count, expected_has_battery,
      expected_milestone_version);

  base::RunLoop run_loop;
  system_data_provider_->GetSystemInfo(
      base::BindLambdaForTesting([&](mojom::SystemInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        EXPECT_EQ(expected_board_name, ptr->board_name);
        EXPECT_EQ(expected_cpu_model, ptr->cpu_model_name);
        EXPECT_EQ(expected_total_memory_kib, ptr->total_memory_kib);
        EXPECT_EQ(expected_cpu_threads_count, ptr->cpu_threads_count);
        EXPECT_EQ(expected_milestone_version,
                  ptr->version_info->milestone_version);
        EXPECT_EQ(expected_has_battery, ptr->device_capabilities->has_battery);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SystemDataProviderTest, BatteryInfo) {
  const std::string expected_manufacturer = "manufacturer";
  const double charge_full_amp_hours = 25;

  SetCrosHealthdBatteryInfoResponse(expected_manufacturer,
                                    charge_full_amp_hours);

  const uint32_t expected_charge_full_design_milliamp_hours =
      charge_full_amp_hours * 1000;

  base::RunLoop run_loop;
  system_data_provider_->GetBatteryInfo(
      base::BindLambdaForTesting([&](mojom::BatteryInfoPtr ptr) {
        ASSERT_TRUE(ptr);
        EXPECT_EQ(expected_manufacturer, ptr->manufacturer);
        EXPECT_EQ(expected_charge_full_design_milliamp_hours,
                  ptr->charge_full_design_milliamp_hours);

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace diagnostics
}  // namespace chromeos

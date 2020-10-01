// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_data_provider.h"

#include <cstdint>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/diagnostics_ui/backend/power_manager_client_conversions.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
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

cros_healthd::mojom::BatteryInfoPtr
CreateCrosHealthdBatteryChargeStatusResponse(double charge_now,
                                             double current_now) {
  return CreateCrosHealthdBatteryInfoResponse(
      /*cycle_count=*/0,
      /*voltage_now=*/0,
      /*vendor=*/"",
      /*serial_number=*/"",
      /*charge_full_design=*/0,
      /*charge_full=*/0,
      /*voltage_min_design=*/0,
      /*model_name=*/"",
      /*charge_now=*/charge_now,
      /*current_now=*/current_now,
      /*technology=*/"",
      /*status=*/"",
      /*manufacture_date=*/base::nullopt,
      /*temperature=*/0);
}

cros_healthd::mojom::BatteryInfoPtr CreateCrosHealthdBatteryHealthResponse(
    double charge_full_now,
    double charge_full_design,
    int32_t cycle_count) {
  return CreateCrosHealthdBatteryInfoResponse(
      /*cycle_count=*/cycle_count,
      /*voltage_now=*/0,
      /*vendor=*/"",
      /*serial_number=*/"",
      /*charge_full_design=*/charge_full_design,
      /*charge_full=*/charge_full_now,
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

void SetCrosHealthdBatteryChargeStatusResponse(double charge_now,
                                               double current_now) {
  cros_healthd::mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryChargeStatusResponse(charge_now, current_now);
  SetProbeTelemetryInfoResponse(std::move(battery_info), /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*memory_info=*/nullptr);
}

void SetCrosHealthdBatteryHealthResponse(double charge_full_now,
                                         double charge_full_design,
                                         int32_t cycle_count) {
  cros_healthd::mojom::BatteryInfoPtr battery_info =
      CreateCrosHealthdBatteryHealthResponse(charge_full_now,
                                             charge_full_design, cycle_count);
  SetProbeTelemetryInfoResponse(std::move(battery_info), /*cpu_info=*/nullptr,
                                /*memory_info=*/nullptr,
                                /*memory_info=*/nullptr);
}

bool AreValidPowerTimes(int64_t time_to_full, int64_t time_to_empty) {
  // Exactly one of |time_to_full| or |time_to_empty| must be zero. The other
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
    int64_t time_to_empty) {
  power_manager::PowerSupplyProperties props;
  props.set_external_power(power_source);
  props.set_battery_state(battery_state);

  if (battery_state ==
      power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT) {
    // Leave |time_to_full| and |time_to_empty| unset.
    return props;
  }

  DCHECK(AreValidPowerTimes(time_to_full, time_to_empty));

  props.set_is_calculating_battery_time(is_calculating_battery_time);
  props.set_battery_time_to_full_sec(time_to_full);
  props.set_battery_time_to_empty_sec(time_to_empty);

  return props;
}

// Sets the PowerSupplyProperties on FakePowerManagerClient. Calling this
// method immediately notifies PowerManagerClient observers. One of
// |time_to_full| or |time_to_empty| must be either -1 or a positive number.
// The other must be 0. If |battery_state| is NOT_PRESENT, both |time_to_full|
// and |time_to_empty| will be left unset.
void SetPowerManagerProperties(
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty) {
  power_manager::PowerSupplyProperties props = ConstructPowerSupplyProperties(
      power_source, battery_state, is_calculating_battery_time, time_to_full,
      time_to_empty);
  FakePowerManagerClient::Get()->UpdatePowerProperties(props);
}

void VerifyChargeStatusResult(
    const mojom::BatteryChargeStatusPtr& update,
    double charge_now,
    double current_now,
    power_manager::PowerSupplyProperties::ExternalPower power_source,
    power_manager::PowerSupplyProperties::BatteryState battery_state,
    bool is_calculating_battery_time,
    int64_t time_to_full,
    int64_t time_to_empty) {
  const uint32_t expected_charge_now_milliamp_hours = charge_now * 1000;
  const int32_t expected_current_now_milliamps = current_now * 1000;
  mojom::ExternalPowerSource expected_power_source =
      ConvertPowerSourceFromProto(power_source);
  mojom::BatteryState expected_battery_state =
      ConvertBatteryStateFromProto(battery_state);

  EXPECT_EQ(expected_charge_now_milliamp_hours,
            update->charge_now_milliamp_hours);
  EXPECT_EQ(expected_current_now_milliamps, update->current_now_milliamps);
  EXPECT_EQ(expected_power_source, update->power_adapter_status);
  EXPECT_EQ(expected_battery_state, update->battery_state);

  if (expected_battery_state == mojom::BatteryState::kFull) {
    EXPECT_EQ(base::string16(), update->power_time);
    return;
  }

  DCHECK(AreValidPowerTimes(time_to_full, time_to_empty));

  const power_manager::PowerSupplyProperties props =
      ConstructPowerSupplyProperties(power_source, battery_state,
                                     is_calculating_battery_time, time_to_full,
                                     time_to_empty);
  base::string16 expected_power_time =
      ConstructPowerTime(expected_battery_state, props);

  EXPECT_EQ(expected_power_time, update->power_time);
}

void VerifyHealthResult(const mojom::BatteryHealthPtr& update,
                        double charge_full_now,
                        double charge_full_design,
                        int32_t expected_cycle_count) {
  const int32_t expected_charge_full_now_milliamp_hours =
      charge_full_now * 1000;
  const int32_t expected_charge_full_design_milliamp_hours =
      charge_full_design * 1000;
  const int8_t expected_battery_wear_percentage =
      expected_charge_full_now_milliamp_hours /
      expected_charge_full_design_milliamp_hours;

  EXPECT_EQ(expected_charge_full_now_milliamp_hours,
            update->charge_full_now_milliamp_hours);
  EXPECT_EQ(expected_charge_full_design_milliamp_hours,
            update->charge_full_design_milliamp_hours);
  EXPECT_EQ(expected_cycle_count, update->cycle_count);
  EXPECT_EQ(expected_battery_wear_percentage, update->battery_wear_percentage);
}

}  // namespace

struct FakeBatteryChargeStatusObserver
    : public mojom::BatteryChargeStatusObserver {
  // mojom::BatteryChargeStatusObserver
  void OnBatteryChargeStatusUpdated(
      mojom::BatteryChargeStatusPtr status_ptr) override {
    updates.emplace_back(std::move(status_ptr));
  }

  // Tracks calls to OnBatteryChargeStatusUpdated. Each call adds an element to
  // the vector.
  std::vector<mojom::BatteryChargeStatusPtr> updates;

  mojo::Receiver<mojom::BatteryChargeStatusObserver> receiver{this};
};

struct FakeBatteryHealthObserver : public mojom::BatteryHealthObserver {
  // mojom::BatteryHealthObserver
  void OnBatteryHealthUpdated(mojom::BatteryHealthPtr status_ptr) override {
    updates.emplace_back(std::move(status_ptr));
  }

  // Tracks calls to OnBatteryHealthUpdated. Each call adds an element to
  // the vector.
  std::vector<mojom::BatteryHealthPtr> updates;

  mojo::Receiver<mojom::BatteryHealthObserver> receiver{this};
};

class SystemDataProviderTest : public testing::Test {
 public:
  SystemDataProviderTest() {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::CrosHealthdClient::InitializeFake();
    system_data_provider_ = std::make_unique<SystemDataProvider>();
  }

  ~SystemDataProviderTest() override {
    system_data_provider_.reset();
    chromeos::CrosHealthdClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
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

TEST_F(SystemDataProviderTest, BatteryChargeStatusObserver) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetBatteryChargeStatusTimerForTesting(
      std::move(timer));

  // Setup initial data
  const double charge_now_amp_hours = 20;
  const double current_now_amps = 2;
  const auto power_source =
      power_manager::PowerSupplyProperties_ExternalPower_AC;
  const auto battery_state =
      power_manager::PowerSupplyProperties_BatteryState_CHARGING;
  const bool is_calculating_battery_time = false;
  const int64_t time_to_full_secs = 1000;
  const int64_t time_to_empty_secs = 0;

  SetCrosHealthdBatteryChargeStatusResponse(charge_now_amp_hours,
                                            current_now_amps);
  SetPowerManagerProperties(power_source, battery_state,
                            is_calculating_battery_time, time_to_full_secs,
                            time_to_empty_secs);

  // Registering as an observer should trigger one update.
  FakeBatteryChargeStatusObserver charge_status_observer;
  system_data_provider_->ObserveBatteryChargeStatus(
      charge_status_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, charge_status_observer.updates.size());
  VerifyChargeStatusResult(charge_status_observer.updates[0],
                           charge_now_amp_hours, current_now_amps, power_source,
                           battery_state, is_calculating_battery_time,
                           time_to_full_secs, time_to_empty_secs);

  // Firing the timer should trigger another.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, charge_status_observer.updates.size());
  VerifyChargeStatusResult(charge_status_observer.updates[0],
                           charge_now_amp_hours, current_now_amps, power_source,
                           battery_state, is_calculating_battery_time,
                           time_to_full_secs, time_to_empty_secs);

  // Updating the PowerManagerClient Properties should trigger yet another.
  const int64_t new_time_to_full_secs = time_to_full_secs - 10;
  SetPowerManagerProperties(
      power_manager::PowerSupplyProperties_ExternalPower_AC,
      power_manager::PowerSupplyProperties_BatteryState_CHARGING,
      is_calculating_battery_time, new_time_to_full_secs, time_to_empty_secs);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, charge_status_observer.updates.size());
  VerifyChargeStatusResult(charge_status_observer.updates[0],
                           charge_now_amp_hours, current_now_amps, power_source,
                           battery_state, is_calculating_battery_time,
                           new_time_to_full_secs, time_to_empty_secs);
}

TEST_F(SystemDataProviderTest, BatteryHealthObserver) {
  // Setup Timer
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  system_data_provider_->SetBatteryHealthTimerForTesting(std::move(timer));

  // Setup initial data
  const double charge_full_now = 20;
  const double charge_full_design = 26;
  const int32_t cycle_count = 500;

  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      cycle_count);

  // Registering as an observer should trigger one update.
  FakeBatteryHealthObserver health_observer;
  system_data_provider_->ObserveBatteryHealth(
      health_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, health_observer.updates.size());
  VerifyHealthResult(health_observer.updates[0], charge_full_now,
                     charge_full_design, cycle_count);

  // Firing the timer should trigger another.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, health_observer.updates.size());
  VerifyHealthResult(health_observer.updates[1], charge_full_now,
                     charge_full_design, cycle_count);

  // Updating the information in Croshealthd does not trigger an update until
  // the timer fires
  const int32_t new_cycle_count = cycle_count + 1;
  SetCrosHealthdBatteryHealthResponse(charge_full_now, charge_full_design,
                                      new_cycle_count);

  EXPECT_EQ(2u, health_observer.updates.size());

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, health_observer.updates.size());
  VerifyHealthResult(health_observer.updates[2], charge_full_now,
                     charge_full_design, new_cycle_count);
}

}  // namespace diagnostics
}  // namespace chromeos

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_data_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/diagnostics_ui/backend/cros_healthd_helpers.h"
#include "chromeos/components/diagnostics_ui/backend/power_manager_client_conversions.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace chromeos {
namespace diagnostics {
namespace {

namespace healthd = cros_healthd::mojom;
using PhysicalCpuInfos = std::vector<healthd::PhysicalCpuInfoPtr>;
using PowerSupplyProperties = power_manager::PowerSupplyProperties;
using ProbeCategories = healthd::ProbeCategoryEnum;

constexpr int kBatteryHealthRefreshIntervalInSeconds = 60;
constexpr int kChargeStatusRefreshIntervalInSeconds = 15;
constexpr int kMilliampsInAnAmp = 1000;

void PopulateBoardName(const healthd::SystemInfo& system_info,
                       mojom::SystemInfo& out_system_info) {
  const base::Optional<std::string>& product_name = system_info.product_name;

  if (!product_name.has_value()) {
    DVLOG(1) << "No board name in SystemInfo response.";
    return;
  }

  out_system_info.board_name = product_name.value();
}

void PopulateCpuInfo(const healthd::CpuInfo& cpu_info,
                     mojom::SystemInfo& out_system_info) {
  const PhysicalCpuInfos& physical_cpus = cpu_info.physical_cpus;
  DCHECK_GE(physical_cpus.size(), 1u);

  out_system_info.cpu_threads_count = cpu_info.num_total_threads;

  // If there is more than one physical cpu on the device, use the name of the
  // first CPU.
  out_system_info.cpu_model_name = physical_cpus[0]->model_name.value_or("");
}

void PopulateVersionInfo(const healthd::SystemInfo& system_info,
                         mojom::SystemInfo& out_system_info) {
  out_system_info.version_info =
      mojom::VersionInfo::New(system_info.os_version->release_milestone);
}

void PopulateMemorySize(const healthd::MemoryInfo& memory_info,
                        mojom::SystemInfo& out_system_info) {
  out_system_info.total_memory_kib = memory_info.total_memory_kib;
}

bool DoesDeviceHaveBattery(
    const PowerSupplyProperties& power_supply_properties) {
  return power_supply_properties.battery_state() !=
         power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
}

bool DoesDeviceHaveBattery(const healthd::TelemetryInfo& telemetry_info) {
  return GetBatteryInfo(telemetry_info) != nullptr;
}

void PopulateDeviceCapabilities(const healthd::TelemetryInfo& telemetry_info,
                                mojom::SystemInfo& out_system_info) {
  mojom::DeviceCapabilitiesPtr capabilities = mojom::DeviceCapabilities::New();
  capabilities->has_battery = DoesDeviceHaveBattery(telemetry_info);
  out_system_info.device_capabilities = std::move(capabilities);
}

void PopulateBatteryInfo(const healthd::BatteryInfo& battery_info,
                         mojom::BatteryInfo& out_battery_info) {
  out_battery_info.manufacturer = battery_info.vendor;
  out_battery_info.charge_full_design_milliamp_hours =
      battery_info.charge_full_design * kMilliampsInAnAmp;
}

void PopulatePowerInfo(const PowerSupplyProperties& power_supply_properties,
                       mojom::BatteryChargeStatus& out_charge_status) {
  const mojom::BatteryState battery_state =
      ConvertBatteryStateFromProto(power_supply_properties.battery_state());

  out_charge_status.battery_state = battery_state;
  out_charge_status.power_time =
      ConstructPowerTime(battery_state, power_supply_properties);
  out_charge_status.power_adapter_status =
      ConvertPowerSourceFromProto(power_supply_properties.external_power());
}

void PopulateBatteryChargeStatus(
    const healthd::BatteryInfo& battery_info,
    const PowerSupplyProperties& power_supply_properties,
    mojom::BatteryChargeStatus& out_charge_status) {
  PopulatePowerInfo(power_supply_properties, out_charge_status);

  out_charge_status.current_now_milliamps =
      battery_info.current_now * kMilliampsInAnAmp;
  out_charge_status.charge_now_milliamp_hours =
      battery_info.charge_now * kMilliampsInAnAmp;
}

void PopulateBatteryHealth(const healthd::BatteryInfo& battery_info,
                           mojom::BatteryHealth& out_battery_health) {
  out_battery_health.charge_full_now_milliamp_hours =
      battery_info.charge_full * kMilliampsInAnAmp;
  out_battery_health.charge_full_design_milliamp_hours =
      battery_info.charge_full_design * kMilliampsInAnAmp;
  out_battery_health.cycle_count = battery_info.cycle_count;
  out_battery_health.battery_wear_percentage =
      out_battery_health.charge_full_now_milliamp_hours /
      out_battery_health.charge_full_design_milliamp_hours;
}

}  // namespace

SystemDataProvider::SystemDataProvider() {
  battery_charge_status_timer_ = std::make_unique<base::RepeatingTimer>();
  battery_health_timer_ = std::make_unique<base::RepeatingTimer>();
  PowerManagerClient::Get()->AddObserver(this);
}

SystemDataProvider::~SystemDataProvider() {
  PowerManagerClient::Get()->RemoveObserver(this);
}

void SystemDataProvider::GetSystemInfo(GetSystemInfoCallback callback) {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery, ProbeCategories::kCpu,
       ProbeCategories::kMemory, ProbeCategories::kSystem},
      base::BindOnce(&SystemDataProvider::OnSystemInfoProbeResponse,
                     base::Unretained(this), std::move(callback)));
}

void SystemDataProvider::GetBatteryInfo(GetBatteryInfoCallback callback) {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryInfoProbeResponse,
                     base::Unretained(this), std::move(callback)));
}

void SystemDataProvider::ObserveBatteryChargeStatus(
    mojo::PendingRemote<mojom::BatteryChargeStatusObserver> observer) {
  battery_charge_status_observers_.Add(std::move(observer));

  if (!battery_charge_status_timer_->IsRunning()) {
    battery_charge_status_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kChargeStatusRefreshIntervalInSeconds),
        base::BindRepeating(&SystemDataProvider::UpdateBatteryChargeStatus,
                            base::Unretained(this)));
  }
  UpdateBatteryChargeStatus();
}

void SystemDataProvider::ObserveBatteryHealth(
    mojo::PendingRemote<mojom::BatteryHealthObserver> observer) {
  battery_health_observers_.Add(std::move(observer));

  if (!battery_health_timer_->IsRunning()) {
    battery_health_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kBatteryHealthRefreshIntervalInSeconds),
        base::BindRepeating(&SystemDataProvider::UpdateBatteryHealth,
                            base::Unretained(this)));
  }
  UpdateBatteryHealth();
}

void SystemDataProvider::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (battery_charge_status_observers_.empty()) {
    return;
  }

  // Fetch updated data from CrosHealthd
  BindCrosHealthdProbeServiceIfNeccessary();
  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryChargeStatusUpdated,
                     base::Unretained(this), proto));
}

void SystemDataProvider::SetBatteryChargeStatusTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  battery_charge_status_timer_ = std::move(timer);
}

void SystemDataProvider::SetBatteryHealthTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  battery_health_timer_ = std::move(timer);
}

void SystemDataProvider::OnSystemInfoProbeResponse(
    GetSystemInfoCallback callback,
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::SystemInfoPtr system_info = mojom::SystemInfo::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    std::move(callback).Run(std::move(system_info));
    return;
  }

  const healthd::SystemInfo* system_info_ptr =
      diagnostics::GetSystemInfo(*info_ptr);
  if (system_info_ptr) {
    PopulateBoardName(*system_info_ptr, *system_info.get());
    PopulateVersionInfo(*system_info_ptr, *system_info.get());
  } else {
    LOG(ERROR)
        << "Expected SystemInfo in croshealthd::ProbeTelemetryInfo response";
    std::move(callback).Run(std::move(system_info));
    return;
  }

  const healthd::CpuInfo* cpu_info_ptr = GetCpuInfo(*info_ptr);
  if (cpu_info_ptr) {
    PopulateCpuInfo(*cpu_info_ptr, *system_info.get());
  } else {
    LOG(ERROR)
        << "Expected CpuInfo in croshealthd::ProbeTelemetryInfo response";
  }

  const healthd::MemoryInfo* memory_info_ptr = GetMemoryInfo(*info_ptr);
  if (memory_info_ptr) {
    PopulateMemorySize(*memory_info_ptr, *system_info.get());
  } else {
    LOG(ERROR)
        << "Expected MemoryInfo in croshealthd::ProbeTelemetryInfo response";
  }

  PopulateDeviceCapabilities(*info_ptr, *system_info.get());

  std::move(callback).Run(std::move(system_info));
}

void SystemDataProvider::OnBatteryInfoProbeResponse(
    GetBatteryInfoCallback callback,
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::BatteryInfoPtr battery_info = mojom::BatteryInfo::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    std::move(callback).Run(std::move(battery_info));
    return;
  }

  const healthd::BatteryInfo* battery_info_ptr =
      diagnostics::GetBatteryInfo(*info_ptr);
  if (!battery_info_ptr) {
    LOG(ERROR) << "BatteryInfo requested by device does not have a battery.";
    std::move(callback).Run(std::move(battery_info));
    return;
  }

  PopulateBatteryInfo(*battery_info_ptr, *battery_info.get());
  std::move(callback).Run(std::move(battery_info));
}

void SystemDataProvider::UpdateBatteryChargeStatus() {
  // Fetch updated data from PowerManagerClient
  base::Optional<PowerSupplyProperties> properties =
      PowerManagerClient::Get()->GetLastStatus();

  // Fetch updated data from CrosHealthd
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryChargeStatusUpdated,
                     base::Unretained(this), properties));
}

void SystemDataProvider::UpdateBatteryHealth() {
  BindCrosHealthdProbeServiceIfNeccessary();

  probe_service_->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemDataProvider::OnBatteryHealthUpdated,
                     base::Unretained(this)));
}

void SystemDataProvider::OnBatteryChargeStatusUpdated(
    const base::Optional<PowerSupplyProperties>& power_supply_properties,
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::BatteryChargeStatusPtr battery_charge_status =
      mojom::BatteryChargeStatus::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    NotifyBatteryChargeStatusObservers(battery_charge_status);
    battery_charge_status_timer_.reset();
    return;
  }

  if (!power_supply_properties.has_value()) {
    LOG(ERROR) << "Null response from power_manager_client::GetLastStatus.";
    NotifyBatteryChargeStatusObservers(battery_charge_status);
    battery_charge_status_timer_.reset();
    return;
  }

  if (!DoesDeviceHaveBattery(*info_ptr) ||
      !DoesDeviceHaveBattery(*power_supply_properties)) {
    DCHECK_EQ(DoesDeviceHaveBattery(*info_ptr),
              DoesDeviceHaveBattery(*power_supply_properties))
        << "Sources should not disagree about whether there is a battery.";
    NotifyBatteryChargeStatusObservers(battery_charge_status);
    battery_charge_status_timer_.reset();
    return;
  }

  PopulateBatteryChargeStatus(*diagnostics::GetBatteryInfo(*info_ptr),
                              *power_supply_properties,
                              *battery_charge_status.get());
  NotifyBatteryChargeStatusObservers(battery_charge_status);
}

void SystemDataProvider::OnBatteryHealthUpdated(
    healthd::TelemetryInfoPtr info_ptr) {
  mojom::BatteryHealthPtr battery_health = mojom::BatteryHealth::New();

  if (info_ptr.is_null()) {
    LOG(ERROR) << "Null response from croshealthd::ProbeTelemetryInfo.";
    NotifyBatteryHealthObservers(battery_health);
    battery_health_timer_.reset();
    return;
  }

  if (!DoesDeviceHaveBattery(*info_ptr)) {
    NotifyBatteryHealthObservers(battery_health);
    battery_health_timer_.reset();
    return;
  }

  PopulateBatteryHealth(*diagnostics::GetBatteryInfo(*info_ptr),
                        *battery_health.get());
  NotifyBatteryHealthObservers(battery_health);
}

void SystemDataProvider::NotifyBatteryChargeStatusObservers(
    const mojom::BatteryChargeStatusPtr& battery_charge_status) {
  for (auto& observer : battery_charge_status_observers_) {
    observer->OnBatteryChargeStatusUpdated(battery_charge_status.Clone());
  }
}

void SystemDataProvider::NotifyBatteryHealthObservers(
    const mojom::BatteryHealthPtr& battery_health) {
  for (auto& observer : battery_health_observers_) {
    observer->OnBatteryHealthUpdated(battery_health.Clone());
  }
}

void SystemDataProvider::BindCrosHealthdProbeServiceIfNeccessary() {
  if (!probe_service_ || !probe_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(base::BindOnce(
        &SystemDataProvider::OnProbeServiceDisconnect, base::Unretained(this)));
  }
}

void SystemDataProvider::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

}  // namespace diagnostics
}  // namespace chromeos

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_data_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "chromeos/components/diagnostics_ui/backend/cros_healthd_helpers.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"

namespace chromeos {
namespace diagnostics {
namespace {

namespace healthd = cros_healthd::mojom;
using PhysicalCpuInfos = std::vector<healthd::PhysicalCpuInfoPtr>;
using ProbeCategories = healthd::ProbeCategoryEnum;

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
  out_system_info.cpu_model_name = physical_cpus[0]->model_name;
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

void PopulateDeviceCapabilities(const healthd::TelemetryInfo& telemetry_info,
                                mojom::SystemInfo& out_system_info) {
  mojom::DeviceCapabilitiesPtr capabilities = mojom::DeviceCapabilities::New();
  capabilities->has_battery = GetBatteryInfo(telemetry_info) != nullptr;
  out_system_info.device_capabilities = std::move(capabilities);
}

void PopulateBatteryInfo(const healthd::BatteryInfo& battery_info,
                         mojom::BatteryInfo& out_battery_info) {
  out_battery_info.manufacturer = battery_info.vendor;
  // Multiply by 1000 to convert amps to milliamps.
  out_battery_info.charge_full_design_milliamp_hours =
      battery_info.charge_full_design * 1000;
}

}  // namespace

SystemDataProvider::SystemDataProvider() = default;
SystemDataProvider::~SystemDataProvider() = default;

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

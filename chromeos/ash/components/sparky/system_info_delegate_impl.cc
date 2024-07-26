// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sparky/system_info_delegate_impl.h"

#include <memory>
#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/system_info/battery_health.h"
#include "chromeos/ash/components/system_info/memory_data.h"
#include "chromeos/ash/components/system_info/system_info_util.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "ui/base/text/bytes_formatting.h"

namespace sparky {

namespace {

using ProbeCategories = ::ash::cros_healthd::mojom::ProbeCategoryEnum;
using ::ash::cros_healthd::mojom::BatteryInfo;
using ::ash::cros_healthd::mojom::CpuInfo;
using ::ash::cros_healthd::mojom::PhysicalCpuInfoPtr;
using ::ash::cros_healthd::mojom::TelemetryInfoPtr;

constexpr int kCpuUsageRefreshIntervalInMilliseconds = 200;

double ConvertKBtoGB(uint32_t amount) {
  return static_cast<double>(amount) / 1024 / 1024;
}

void PopulatePowerStatus(const power_manager::PowerSupplyProperties& proto,
                         system_info::BatteryHealth& battery_health) {
  bool calculating = proto.is_calculating_battery_time();
  int percent = system_info::GetRoundedBatteryPercent(proto.battery_percent());
  CHECK(percent <= 100 && percent >= 0);

  if (!calculating) {
    bool charging =
        proto.battery_state() == power_manager::PowerSupplyProperties::CHARGING;
    base::TimeDelta time_left =
        base::Seconds(charging ? proto.battery_time_to_full_sec()
                               : proto.battery_time_to_empty_sec());
    if (system_info::ShouldDisplayBatteryTime(time_left)) {
      // TODO (b:342609231) replace with a translation string.
      battery_health.SetPowerTime(
          charging ? system_info::GetBatteryTimeText(time_left) + u" until full"
                   : system_info::GetBatteryTimeText(time_left) + u" left");
    }
  }
  battery_health.SetBatteryPercentage(percent);
}

}  // namespace

SystemInfoDelegateImpl::SystemInfoDelegateImpl()
    : cpu_usage_timer_(std::make_unique<base::RepeatingTimer>()) {}

SystemInfoDelegateImpl::~SystemInfoDelegateImpl() {}

void SystemInfoDelegateImpl::ObtainDiagnostics(
    const std::vector<manta::Diagnostics>& diagnostics,
    manta::DiagnosticsDataCallback diagnostics_callback) {
  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  diagnostics_callback_ = std::move(diagnostics_callback);
  diagnostics_error_ = false;
  for (manta::Diagnostics diagnostics_option : diagnostics) {
    switch (diagnostics_option) {
      case manta::Diagnostics::kMemory: {
        UpdateMemoryUsage();
        break;
      }
      case manta::Diagnostics::kBattery: {
        UpdateBatteryInfo();
        break;
      }
      case manta::Diagnostics::kCpu: {
        cpu_refreshes_left_ = 2;
        cpu_usage_timer_->Start(
            FROM_HERE,
            base::Milliseconds(kCpuUsageRefreshIntervalInMilliseconds),
            base::BindRepeating(&SystemInfoDelegateImpl::UpdateCpuUsage,
                                weak_factory_.GetWeakPtr()));
        break;
      }
      case manta::Diagnostics::kStorage: {
        // TODO (b:340963863) This field will be handled within the Sparky
        // Delegate Impl as it requires a profile to obtain the storage data.
        break;
      }
    }
  }
  diagnostics_requested_ = diagnostics;
}

void SystemInfoDelegateImpl::OnDiagnosticsUpdated() {
  if (diagnostics_error_) {  // TODO (b:343072278) add in a test case for
                             // diagnostics error.
    return;
  }
  // Check if all of the requested diagnostics fields have been calculated yet.
  // Only return the results once all of the values are found.
  for (auto diagnostics_type : diagnostics_requested_) {
    if (diagnostics_type == manta::Diagnostics::kMemory && !memory_data_) {
      return;
    }
    if (diagnostics_type == manta::Diagnostics::kCpu && !cpu_data_) {
      return;
    }
    if (diagnostics_type == manta::Diagnostics::kBattery && !battery_data_) {
      return;
    }
  }
  if (diagnostics_callback_) {
    std::move(diagnostics_callback_)
        .Run(std::make_unique<manta::DiagnosticsData>(
            battery_data_ ? std::optional<manta::BatteryData>(*battery_data_)
                          : std::nullopt,
            cpu_data_ ? std::optional<manta::CpuData>(*cpu_data_)
                      : std::nullopt,
            memory_data_ ? std::optional<manta::MemoryData>(*memory_data_)
                         : std::nullopt,
            std::nullopt));
  }
}

void SystemInfoDelegateImpl::ReturnWithNullptr() {
  diagnostics_error_ = true;
  if (diagnostics_callback_) {
    std::move(diagnostics_callback_).Run(nullptr);
  }
  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
}

void SystemInfoDelegateImpl::UpdateMemoryUsage() {
  auto* probe_service =
      ash::cros_healthd::ServiceConnection::GetInstance()->GetProbeService();

  probe_service->ProbeTelemetryInfo(
      {ProbeCategories::kMemory},
      base::BindOnce(&SystemInfoDelegateImpl::OnMemoryUsageUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemInfoDelegateImpl::UpdateCpuUsage() {
  auto* probe_service =
      ash::cros_healthd::ServiceConnection::GetInstance()->GetProbeService();

  probe_service->ProbeTelemetryInfo(
      {ProbeCategories::kCpu},
      base::BindOnce(&SystemInfoDelegateImpl::OnCpuUsageUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemInfoDelegateImpl::UpdateBatteryInfo() {
  auto* probe_service =
      ash::cros_healthd::ServiceConnection::GetInstance()->GetProbeService();

  probe_service->ProbeTelemetryInfo(
      {ProbeCategories::kBattery},
      base::BindOnce(&SystemInfoDelegateImpl::OnBatteryInfoUpdated,
                     weak_factory_.GetWeakPtr()));
}

void SystemInfoDelegateImpl::OnMemoryUsageUpdated(TelemetryInfoPtr info_ptr) {
  auto* memory_info = system_info::GetMemoryInfo(*info_ptr, "");
  if (!memory_info) {
    LOG(ERROR) << "Memory information not provided by croshealthd";
    ReturnWithNullptr();
    return;
  }

  double available_memory_gb = ConvertKBtoGB(memory_info->available_memory_kib);
  double total_memory_gb = ConvertKBtoGB(memory_info->total_memory_kib);

  memory_data_ =
      std::make_unique<manta::MemoryData>(available_memory_gb, total_memory_gb);
  OnDiagnosticsUpdated();
}

void SystemInfoDelegateImpl::OnCpuUsageUpdated(
    ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr) {
  const CpuInfo* cpu_info = system_info::GetCpuInfo(*info_ptr, "");
  if (cpu_info == nullptr) {
    LOG(ERROR) << "No CpuInfo in response from cros_healthd.";
    ReturnWithNullptr();
    return;
  }

  if (cpu_info->physical_cpus.empty()) {
    LOG(ERROR) << "Device reported having zero physical CPUs.";
    ReturnWithNullptr();
    return;
  }

  if (cpu_info->physical_cpus[0]->logical_cpus.empty()) {
    LOG(ERROR) << "Device reported having zero logical CPUs.";
    ReturnWithNullptr();
    return;
  }

  // For simplicity, assumes that all devices have just one physical CPU, made
  // up of one or more virtual CPUs.
  if (cpu_info->physical_cpus.size() > 1) {
    VLOG(1) << "Device has more than one physical CPU.";
  }

  const PhysicalCpuInfoPtr& physical_cpu_ptr = cpu_info->physical_cpus[0];

  system_info::CpuUsageData new_cpu_usage_data =
      system_info::CalculateCpuUsage(physical_cpu_ptr->logical_cpus);
  std::unique_ptr<system_info::CpuData> new_cpu_usage =
      std::make_unique<system_info::CpuData>();

  system_info::PopulateCpuUsage(new_cpu_usage_data, previous_cpu_usage_data_,
                                *new_cpu_usage.get());
  system_info::PopulateAverageCpuTemperature(*cpu_info, *new_cpu_usage.get());
  system_info::PopulateAverageScaledClockSpeed(*cpu_info, *new_cpu_usage.get());

  previous_cpu_usage_data_ = new_cpu_usage_data;
  cpu_refreshes_left_--;
  if (cpu_refreshes_left_ == 0) {
    int percentage;
    base::StringToInt(new_cpu_usage->GetPercentUsageTotalString(), &percentage);
    cpu_data_ = std::make_unique<manta::CpuData>(
        percentage, new_cpu_usage->GetAverageCpuTempCelsius(),
        static_cast<double>(
            new_cpu_usage->GetScalingAverageCurrentFrequencyKhz() / 10000) /
            100);
    cpu_usage_timer_->Stop();
    OnDiagnosticsUpdated();
  }
}

void SystemInfoDelegateImpl::OnBatteryInfoUpdated(
    ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr) {
  const BatteryInfo* battery_info_ptr =
      system_info::GetBatteryInfo(*info_ptr, "", "");
  if (!battery_info_ptr) {
    LOG(ERROR) << "BatteryInfo requested by device does not have a battery.";
    ReturnWithNullptr();
    return;
  }

  std::unique_ptr<system_info::BatteryHealth> new_battery_health =
      std::make_unique<system_info::BatteryHealth>();

  system_info::PopulateBatteryHealth(*battery_info_ptr,
                                     *new_battery_health.get());

  const std::optional<power_manager::PowerSupplyProperties>& proto =
      chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!proto) {
    LOG(ERROR) << "No data from Power Manager.";
    ReturnWithNullptr();
    return;
  }
  PopulatePowerStatus(proto.value(), *new_battery_health.get());
  battery_data_ = std::make_unique<manta::BatteryData>(
      new_battery_health->GetCycleCount(),
      new_battery_health->GetBatteryWearPercentage(),
      base::UTF16ToUTF8(new_battery_health->GetPowerTime()),
      new_battery_health->GetBatteryPercentage());
  OnDiagnosticsUpdated();
}

}  // namespace sparky

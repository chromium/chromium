// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/healthd_internals/healthd_internals_message_handler.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

namespace mojom = ::ash::cros_healthd::mojom;

std::string Convert(mojom::ThermalSensorInfo::ThermalSensorSource source) {
  switch (source) {
    case mojom::ThermalSensorInfo::ThermalSensorSource::kEc:
      return "EC";
    case mojom::ThermalSensorInfo::ThermalSensorSource::kSysFs:
      return "SysFs";
    case mojom::ThermalSensorInfo::ThermalSensorSource::kUnmappedEnumField:
      return "Unknown";
  }
}

std::string Convert(mojom::CpuArchitectureEnum source) {
  switch (source) {
    case mojom::CpuArchitectureEnum::kX86_64:
      return "x86_64";
    case mojom::CpuArchitectureEnum::kAArch64:
      return "aarch64";
    case mojom::CpuArchitectureEnum::kArmv7l:
      return "armv7l";
    case mojom::CpuArchitectureEnum::kUnknown:
      return "Unknown";
  }
}

std::string Convert(mojom::ProcessState state) {
  switch (state) {
    case mojom::ProcessState::kUnknown:
      return "Unknown";
    case mojom::ProcessState::kRunning:
      return "Running";
    case mojom::ProcessState::kSleeping:
      return "Sleeping";
    case mojom::ProcessState::kWaiting:
      return "Waiting";
    case mojom::ProcessState::kZombie:
      return "Zombie";
    case mojom::ProcessState::kStopped:
      return "Stopped";
    case mojom::ProcessState::kTracingStop:
      return "TracingStop";
    case mojom::ProcessState::kDead:
      return "Dead";
    case mojom::ProcessState::kIdle:
      return "Idle";
  }
}

base::Value::Dict ConvertBatteryValue(const mojom::BatteryInfoPtr& info) {
  base::Value::Dict out_battery;
  if (info) {
    out_battery.Set("currentNow", info->current_now);
    out_battery.Set("voltageNow", info->voltage_now);
    out_battery.Set("chargeNow", info->charge_now);
  }
  return out_battery;
}

base::Value::List ConvertLogicalCpus(
    const std::vector<mojom::LogicalCpuInfoPtr>& logical_cpus) {
  base::Value::List out_logical_cpus;
  for (const auto& logical_cpu : logical_cpus) {
    if (!logical_cpu) {
      continue;
    }
    base::Value::Dict out_logical_cpu;
    out_logical_cpu.Set("coreId", base::NumberToString(logical_cpu->core_id));
    out_logical_cpu.SetByDottedPath(
        "frequency.current",
        base::NumberToString(logical_cpu->scaling_current_frequency_khz));
    out_logical_cpu.SetByDottedPath(
        "frequency.max",
        base::NumberToString(logical_cpu->scaling_max_frequency_khz));
    out_logical_cpu.SetByDottedPath(
        "executionTime.user",
        base::NumberToString(logical_cpu->user_time_user_hz));
    out_logical_cpu.SetByDottedPath(
        "executionTime.system",
        base::NumberToString(logical_cpu->system_time_user_hz));
    out_logical_cpu.SetByDottedPath(
        "executionTime.idle",
        base::NumberToString(logical_cpu->idle_time_user_hz));
    out_logical_cpus.Append(std::move(out_logical_cpu));
  }
  return out_logical_cpus;
}

base::Value::List ConvertPhysicalCpus(
    const std::vector<mojom::PhysicalCpuInfoPtr>& physical_cpus) {
  base::Value::List out_physical_cpus;
  for (const auto& physical_cpu : physical_cpus) {
    if (!physical_cpu) {
      continue;
    }
    base::Value::Dict out_physical_cpu;
    if (physical_cpu->model_name.has_value()) {
      out_physical_cpu.Set("modelName", physical_cpu->model_name.value());
    }
    out_physical_cpu.Set("logicalCpus",
                         ConvertLogicalCpus(physical_cpu->logical_cpus));
    out_physical_cpus.Append(std::move(out_physical_cpu));
  }
  return out_physical_cpus;
}

base::Value::List ConvertTemperatureChannels(
    const std::vector<mojom::CpuTemperatureChannelPtr>& temperature_channels) {
  base::Value::List out_temperature_channels;
  for (const auto& temperature_channel : temperature_channels) {
    if (!temperature_channel) {
      continue;
    }
    base::Value::Dict out_temperature_channel;
    if (temperature_channel->label.has_value()) {
      out_temperature_channel.Set("label", temperature_channel->label.value());
    }
    out_temperature_channel.Set("temperatureCelsius",
                                temperature_channel->temperature_celsius);
    out_temperature_channels.Append(std::move(out_temperature_channel));
  }
  return out_temperature_channels;
}

base::Value::Dict ConvertCpuValue(const mojom::CpuInfoPtr& info) {
  base::Value::Dict out_cpu;
  if (info) {
    out_cpu.Set("architecture", Convert(info->architecture));
    out_cpu.Set("numTotalThreads",
                base::NumberToString(info->num_total_threads));
    out_cpu.Set("physicalCpus", ConvertPhysicalCpus(info->physical_cpus));
    out_cpu.Set("temperatureChannels",
                ConvertTemperatureChannels(info->temperature_channels));
  }
  return out_cpu;
}

base::Value::List ConvertFanValue(const std::vector<mojom::FanInfoPtr>& info) {
  base::Value::List out_fans;
  for (const auto& fan : info) {
    if (fan) {
      base::Value::Dict fan_result;
      fan_result.Set("speedRpm", base::NumberToString(fan->speed_rpm));
      out_fans.Append(std::move(fan_result));
    }
  }
  return out_fans;
}

// Set the value to `output` for optional uint64 value.
void SetValue(std::string_view field_name,
              std::optional<uint64_t> value,
              base::Value::Dict& output) {
  if (value.has_value()) {
    output.Set(field_name, base::NumberToString(value.value()));
  }
}

base::Value::Dict ConvertMemoryValue(const mojom::MemoryInfoPtr& info) {
  base::Value::Dict out_memory;
  if (info) {
    out_memory.Set("availableMemoryKib",
                   base::NumberToString(info->available_memory_kib));
    out_memory.Set("freeMemoryKib",
                   base::NumberToString(info->free_memory_kib));
    out_memory.Set("totalMemoryKib",
                   base::NumberToString(info->total_memory_kib));
    SetValue("buffersKib", info->buffers_kib, out_memory);
    SetValue("pageCacheKib", info->page_cache_kib, out_memory);
    SetValue("sharedMemoryKib", info->shared_memory_kib, out_memory);

    SetValue("activeMemoryKib", info->active_memory_kib, out_memory);
    SetValue("inactiveMemoryKib", info->inactive_memory_kib, out_memory);

    SetValue("totalSwapMemoryKib", info->total_swap_memory_kib, out_memory);
    SetValue("freeSwapMemoryKib", info->free_swap_memory_kib, out_memory);
    SetValue("cachedSwapMemoryKib", info->cached_swap_memory_kib, out_memory);

    SetValue("totalSlabMemoryKib", info->total_slab_memory_kib, out_memory);
    SetValue("reclaimableSlabMemoryKib", info->reclaimable_slab_memory_kib,
             out_memory);
    SetValue("unreclaimableSlabMemoryKib", info->unreclaimable_slab_memory_kib,
             out_memory);
  }
  return out_memory;
}

base::Value::Dict ConvertProcessValue(const mojom::ProcessInfoPtr& info) {
  base::Value::Dict out_process;
  if (info) {
    out_process.Set("command", info->command);
    out_process.Set("userId", base::NumberToString(info->user_id));
    out_process.Set("priority", info->priority);
    out_process.Set("nice", info->nice);
    out_process.Set("uptimeTicks", base::NumberToString(info->uptime_ticks));
    out_process.Set("state", Convert(info->state));
    out_process.Set("residentMemoryKib",
                    base::NumberToString(info->resident_memory_kib));
    out_process.Set("readSystemCallsCount",
                    base::NumberToString(info->read_system_calls));
    out_process.Set("writeSystemCallsCount",
                    base::NumberToString(info->write_system_calls));
    if (info->name.has_value()) {
      out_process.Set("name", info->name.value());
    }
    out_process.Set("parentProcessId",
                    base::NumberToString(info->parent_process_id));
    out_process.Set("processGroupId",
                    base::NumberToString(info->process_group_id));
    out_process.Set("threadsNumber", base::NumberToString(info->threads));
    out_process.Set("processId", base::NumberToString(info->process_id));
  }
  return out_process;
}

base::Value::List ConvertThermalValue(const mojom::ThermalInfoPtr& info) {
  base::Value::List out_thermals;
  if (info) {
    for (const auto& thermal : info->thermal_sensors) {
      base::Value::Dict thermal_result;
      thermal_result.Set("name", thermal->name);
      thermal_result.Set("source", Convert(thermal->source));
      thermal_result.Set("temperatureCelsius", thermal->temperature_celsius);
      out_thermals.Append(std::move(thermal_result));
    }
  }
  return out_thermals;
}

}  // namespace

HealthdInternalsMessageHandler::HealthdInternalsMessageHandler() = default;

HealthdInternalsMessageHandler::~HealthdInternalsMessageHandler() = default;

void HealthdInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getHealthdInternalsFeatureFlag",
      base::BindRepeating(
          &HealthdInternalsMessageHandler::HandleGetHealthdInternalsFeatureFlag,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getHealthdTelemetryInfo",
      base::BindRepeating(
          &HealthdInternalsMessageHandler::HandleGetHealthdTelemetryInfo,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getHealthdProcessInfo",
      base::BindRepeating(
          &HealthdInternalsMessageHandler::HandleGetHealthdProcessInfo,
          weak_ptr_factory_.GetWeakPtr()));
}

void HealthdInternalsMessageHandler::HandleGetHealthdInternalsFeatureFlag(
    const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AllowJavascript();
  if (list.size() != 1 || !list[0].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  base::Value callback_id = list[0].Clone();
  base::Value::Dict result;
  result.Set("tabsDisplayed", features::AreHealthdInternalsTabsEnabled());
  ResolveJavascriptCallback(callback_id, result);
}

void HealthdInternalsMessageHandler::HandleGetHealthdTelemetryInfo(
    const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AllowJavascript();
  if (list.size() != 1 || !list[0].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  base::Value callback_id = list[0].Clone();
  auto* service = GetProbeService();
  if (!service) {
    HandleTelemetryResult(std::move(callback_id), nullptr);
    return;
  }

  service->ProbeTelemetryInfo(
      {mojom::ProbeCategoryEnum::kBattery, mojom::ProbeCategoryEnum::kCpu,
       mojom::ProbeCategoryEnum::kFan, mojom::ProbeCategoryEnum::kMemory,
       mojom::ProbeCategoryEnum::kThermal},
      base::BindOnce(&HealthdInternalsMessageHandler::HandleTelemetryResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void HealthdInternalsMessageHandler::HandleTelemetryResult(
    base::Value callback_id,
    mojom::TelemetryInfoPtr info) {
  if (!info) {
    LOG(WARNING) << "Unable to access telemetry info from Healthd";
    ReplyHealthdInternalInfo(std::move(callback_id), base::Value::Dict());
    return;
  }

  base::Value::Dict result;
  if (info->battery_result && info->battery_result->is_battery_info()) {
    const auto& battery_info = info->battery_result->get_battery_info();
    if (battery_info) {
      result.Set("battery", ConvertBatteryValue(battery_info));
    }
  }
  if (info->cpu_result && info->cpu_result->is_cpu_info()) {
    result.Set("cpu", ConvertCpuValue(info->cpu_result->get_cpu_info()));
  }
  if (info->fan_result && info->fan_result->is_fan_info()) {
    result.Set("fans", ConvertFanValue(info->fan_result->get_fan_info()));
  }
  if (info->memory_result && info->memory_result->is_memory_info()) {
    result.Set("memory",
               ConvertMemoryValue(info->memory_result->get_memory_info()));
  }
  if (info->thermal_result && info->thermal_result->is_thermal_info()) {
    result.Set("thermals",
               ConvertThermalValue(info->thermal_result->get_thermal_info()));
  }

  ReplyHealthdInternalInfo(std::move(callback_id), std::move(result));
}

void HealthdInternalsMessageHandler::HandleGetHealthdProcessInfo(
    const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AllowJavascript();
  if (list.size() != 1 || !list[0].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  base::Value callback_id = list[0].Clone();
  auto* service = GetProbeService();
  if (!service) {
    HandleMultipleProcessResult(std::move(callback_id), nullptr);
    return;
  }

  service->ProbeMultipleProcessInfo(
      /*process_ids=*/std::nullopt, /*ignore_single_process_error=*/true,
      base::BindOnce(
          &HealthdInternalsMessageHandler::HandleMultipleProcessResult,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void HealthdInternalsMessageHandler::HandleMultipleProcessResult(
    base::Value callback_id,
    mojom::MultipleProcessResultPtr process_result) {
  base::Value::Dict result;
  if (!process_result) {
    LOG(WARNING) << "Unable to access process info from Healthd";
    result.Set("processes", base::Value::List());
    ReplyHealthdInternalInfo(std::move(callback_id), std::move(result));
    return;
  }

  base::Value::List out_processes;
  for (const auto& [_, process_info] : process_result->process_infos) {
    out_processes.Append(ConvertProcessValue(process_info));
  }
  result.Set("processes", std::move(out_processes));

  ReplyHealthdInternalInfo(std::move(callback_id), std::move(result));
}

void HealthdInternalsMessageHandler::ReplyHealthdInternalInfo(
    base::Value callback_id,
    base::Value::Dict result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ResolveJavascriptCallback(callback_id, result);
}

mojom::CrosHealthdProbeService*
HealthdInternalsMessageHandler::GetProbeService() {
  if (!probe_service_ || !probe_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(base::BindOnce(
        &HealthdInternalsMessageHandler::OnProbeServiceDisconnect,
        weak_ptr_factory_.GetWeakPtr()));
  }
  return probe_service_.get();
}

void HealthdInternalsMessageHandler::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

}  // namespace ash

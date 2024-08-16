// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_converters.h"

#include <unistd.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"

namespace ash::converters::telemetry {

namespace {

cros_healthd::mojom::ProbeCategoryEnum Convert(
    crosapi::mojom::ProbeCategoryEnum input) {
  switch (input) {
    case crosapi::mojom::ProbeCategoryEnum::kUnknown:
      return cros_healthd::mojom::ProbeCategoryEnum::kUnknown;
    case crosapi::mojom::ProbeCategoryEnum::kBattery:
      return cros_healthd::mojom::ProbeCategoryEnum::kBattery;
    case crosapi::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices:
      return cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices;
    case crosapi::mojom::ProbeCategoryEnum::kCachedVpdData:
      return cros_healthd::mojom::ProbeCategoryEnum::kSystem;
    case crosapi::mojom::ProbeCategoryEnum::kCpu:
      return cros_healthd::mojom::ProbeCategoryEnum::kCpu;
    case crosapi::mojom::ProbeCategoryEnum::kTimezone:
      return cros_healthd::mojom::ProbeCategoryEnum::kTimezone;
    case crosapi::mojom::ProbeCategoryEnum::kMemory:
      return cros_healthd::mojom::ProbeCategoryEnum::kMemory;
    case crosapi::mojom::ProbeCategoryEnum::kNetwork:
      return cros_healthd::mojom::ProbeCategoryEnum::kNetwork;
    case crosapi::mojom::ProbeCategoryEnum::kBacklight:
      return cros_healthd::mojom::ProbeCategoryEnum::kBacklight;
    case crosapi::mojom::ProbeCategoryEnum::kFan:
      return cros_healthd::mojom::ProbeCategoryEnum::kFan;
    case crosapi::mojom::ProbeCategoryEnum::kStatefulPartition:
      return cros_healthd::mojom::ProbeCategoryEnum::kStatefulPartition;
    case crosapi::mojom::ProbeCategoryEnum::kBluetooth:
      return cros_healthd::mojom::ProbeCategoryEnum::kBluetooth;
    case crosapi::mojom::ProbeCategoryEnum::kSystem:
      return cros_healthd::mojom::ProbeCategoryEnum::kSystem;
    case crosapi::mojom::ProbeCategoryEnum::kTpm:
      return cros_healthd::mojom::ProbeCategoryEnum::kTpm;
    case crosapi::mojom::ProbeCategoryEnum::kAudio:
      return cros_healthd::mojom::ProbeCategoryEnum::kAudio;
    case crosapi::mojom::ProbeCategoryEnum::kBus:
      return cros_healthd::mojom::ProbeCategoryEnum::kBus;
    case crosapi::mojom::ProbeCategoryEnum::kDisplay:
      return cros_healthd::mojom::ProbeCategoryEnum::kDisplay;
    case crosapi::mojom::ProbeCategoryEnum::kThermal:
      return cros_healthd::mojom::ProbeCategoryEnum::kThermal;
  }
  NOTREACHED();
}

}  // namespace

namespace unchecked {

crosapi::mojom::ProbeErrorPtr UncheckedConvertPtr(
    cros_healthd::mojom::ProbeErrorPtr input) {
  return crosapi::mojom::ProbeError::New(Convert(input->type),
                                         std::move(input->msg));
}

std::optional<double> UncheckedConvertPtr(
    cros_healthd::mojom::NullableDoublePtr input) {
  return input->value;
}

std::optional<uint8_t> UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint8Ptr input) {
  return input->value;
}

std::optional<uint16_t> UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint16Ptr input) {
  return input->value;
}

std::optional<uint32_t> UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint32Ptr input) {
  return input->value;
}

crosapi::mojom::UInt64ValuePtr LegacyUncheckedConvertPtr(
    cros_healthd::mojom::NullableUint64Ptr input) {
  return crosapi::mojom::UInt64Value::New(input->value);
}

crosapi::mojom::ProbeAudioInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioInfoPtr input) {
  std::optional<std::vector<crosapi::mojom::ProbeAudioOutputNodeInfoPtr>>
      output_nodes;
  std::optional<std::vector<crosapi::mojom::ProbeAudioInputNodeInfoPtr>>
      input_nodes;

  if (input->output_nodes) {
    output_nodes = std::vector<crosapi::mojom::ProbeAudioOutputNodeInfoPtr>();
    for (auto& elem : input->output_nodes.value()) {
      auto converted_value = ConvertAudioOutputNodePtr(std::move(elem));
      output_nodes->push_back(std::move(converted_value));
    }
  }
  if (input->input_nodes) {
    input_nodes = std::vector<crosapi::mojom::ProbeAudioInputNodeInfoPtr>();
    for (auto& elem : input->input_nodes.value()) {
      auto converted_value = ConvertAudioInputNodePtr(std::move(elem));
      input_nodes->push_back(std::move(converted_value));
    }
  }

  return crosapi::mojom::ProbeAudioInfo::New(
      crosapi::mojom::BoolValue::New(input->output_mute),
      crosapi::mojom::BoolValue::New(input->input_mute),
      crosapi::mojom::UInt32Value::New(input->underruns),
      crosapi::mojom::UInt32Value::New(input->severe_underruns),
      std::move(output_nodes), std::move(input_nodes));
}

crosapi::mojom::ProbeAudioResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::AudioResult::Tag::kAudioInfo:
      return crosapi::mojom::ProbeAudioResult::NewAudioInfo(
          ConvertProbePtr(std::move(input->get_audio_info())));
    case cros_healthd::mojom::AudioResult::Tag::kError:
      return crosapi::mojom::ProbeAudioResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeUsbBusInterfaceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::UsbBusInterfaceInfoPtr input) {
  return crosapi::mojom::ProbeUsbBusInterfaceInfo::New(
      crosapi::mojom::UInt8Value::New(input->interface_number),
      crosapi::mojom::UInt8Value::New(input->class_id),
      crosapi::mojom::UInt8Value::New(input->subclass_id),
      crosapi::mojom::UInt8Value::New(input->protocol_id), input->driver);
}

crosapi::mojom::ProbeFwupdFirmwareVersionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FwupdFirmwareVersionInfoPtr input) {
  return crosapi::mojom::ProbeFwupdFirmwareVersionInfo::New(
      input->version, Convert(input->version_format));
}

crosapi::mojom::ProbeUsbBusInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::UsbBusInfoPtr input) {
  return crosapi::mojom::ProbeUsbBusInfo::New(
      crosapi::mojom::UInt8Value::New(input->class_id),
      crosapi::mojom::UInt8Value::New(input->subclass_id),
      crosapi::mojom::UInt8Value::New(input->protocol_id),
      crosapi::mojom::UInt16Value::New(input->vendor_id),
      crosapi::mojom::UInt16Value::New(input->product_id),
      ConvertPtrVector<crosapi::mojom::ProbeUsbBusInterfaceInfoPtr>(
          std::move(input->interfaces)),
      ConvertProbePtr(std::move(input->fwupd_firmware_version_info)),
      Convert(input->version), Convert(input->spec_speed));
}

crosapi::mojom::ProbeBusInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BusInfoPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BusInfo::Tag::kUsbBusInfo:
      return crosapi::mojom::ProbeBusInfo::NewUsbBusInfo(
          ConvertProbePtr(std::move(input->get_usb_bus_info())));
    case cros_healthd::mojom::BusInfo::Tag::kPciBusInfo:
    case cros_healthd::mojom::BusInfo::Tag::kThunderboltBusInfo:
    case cros_healthd::mojom::BusInfo::Tag::kUnmappedField:
      return nullptr;
  }
}

crosapi::mojom::ProbeBusInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BusDevicePtr input) {
  return ConvertProbePtr(std::move(input->bus_info));
}

crosapi::mojom::ProbeBusResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BusResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BusResult::Tag::kBusDevices:
      return crosapi::mojom::ProbeBusResult::NewBusDevicesInfo(
          ConvertPtrVector<crosapi::mojom::ProbeBusInfoPtr>(
              std::move(input->get_bus_devices())));
    case cros_healthd::mojom::BusResult::Tag::kError:
      return crosapi::mojom::ProbeBusResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeBatteryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryInfoPtr input) {
  return crosapi::mojom::ProbeBatteryInfo::New(
      Convert(input->cycle_count), Convert(input->voltage_now),
      std::move(input->vendor), std::move(input->serial_number),
      Convert(input->charge_full_design), Convert(input->charge_full),
      Convert(input->voltage_min_design), std::move(input->model_name),
      Convert(input->charge_now), Convert(input->current_now),
      std::move(input->technology), std::move(input->status),
      std::move(input->manufacture_date),
      LegacyConvertProbePtr(std::move(input->temperature)));
}

crosapi::mojom::ProbeBatteryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BatteryResult::Tag::kBatteryInfo:
      return crosapi::mojom::ProbeBatteryResult::NewBatteryInfo(
          ConvertProbePtr(std::move(input->get_battery_info())));
    case cros_healthd::mojom::BatteryResult::Tag::kError:
      return crosapi::mojom::ProbeBatteryResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeNonRemovableBlockDeviceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr input) {
  return crosapi::mojom::ProbeNonRemovableBlockDeviceInfo::New(
      std::move(input->path), Convert(input->size), std::move(input->type),
      Convert(static_cast<uint32_t>(input->manufacturer_id)),
      std::move(input->name), base::NumberToString(input->serial),
      Convert(input->bytes_read_since_last_boot),
      Convert(input->bytes_written_since_last_boot),
      Convert(input->read_time_seconds_since_last_boot),
      Convert(input->write_time_seconds_since_last_boot),
      Convert(input->io_time_seconds_since_last_boot),
      LegacyConvertProbePtr(
          std::move(input->discard_time_seconds_since_last_boot)));
}

crosapi::mojom::ProbeNonRemovableBlockDeviceResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::
        kBlockDeviceInfo:
      return crosapi::mojom::ProbeNonRemovableBlockDeviceResult::
          NewBlockDeviceInfo(
              ConvertPtrVector<
                  crosapi::mojom::ProbeNonRemovableBlockDeviceInfoPtr>(
                  std::move(input->get_block_device_info())));
    case cros_healthd::mojom::NonRemovableBlockDeviceResult::Tag::kError:
      return crosapi::mojom::ProbeNonRemovableBlockDeviceResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeCachedVpdInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::VpdInfoPtr input) {
  return crosapi::mojom::ProbeCachedVpdInfo::New(
      std::move(input->activate_date), std::move(input->sku_number),
      std::move(input->serial_number), std::move(input->model_name));
}

crosapi::mojom::ProbeCpuCStateInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuCStateInfoPtr input) {
  return crosapi::mojom::ProbeCpuCStateInfo::New(
      std::move(input->name), Convert(input->time_in_state_since_last_boot_us));
}

namespace {

uint64_t UserHz() {
  const long user_hz = sysconf(_SC_CLK_TCK);
  DCHECK(user_hz >= 0);
  return user_hz;
}

}  // namespace

crosapi::mojom::ProbeLogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input) {
  return UncheckedConvertPtr(std::move(input), UserHz());
}

crosapi::mojom::ProbeLogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input,
    uint64_t user_hz) {
  constexpr uint64_t kMillisecondsInSecond = 1000;
  uint64_t idle_time_user_hz = static_cast<uint64_t>(input->idle_time_user_hz);

  DCHECK(user_hz != 0);

  return crosapi::mojom::ProbeLogicalCpuInfo::New(
      Convert(input->max_clock_speed_khz),
      Convert(input->scaling_max_frequency_khz),
      Convert(input->scaling_current_frequency_khz),
      Convert(idle_time_user_hz * kMillisecondsInSecond / user_hz),
      ConvertPtrVector<crosapi::mojom::ProbeCpuCStateInfoPtr>(
          std::move(input->c_states)),
      Convert(input->core_id));
}

crosapi::mojom::ProbePhysicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PhysicalCpuInfoPtr input) {
  return crosapi::mojom::ProbePhysicalCpuInfo::New(
      std::move(input->model_name),
      ConvertPtrVector<crosapi::mojom::ProbeLogicalCpuInfoPtr>(
          std::move(input->logical_cpus)));
}

crosapi::mojom::ProbeCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuInfoPtr input) {
  return crosapi::mojom::ProbeCpuInfo::New(
      Convert(input->num_total_threads), Convert(input->architecture),
      ConvertPtrVector<crosapi::mojom::ProbePhysicalCpuInfoPtr>(
          std::move(input->physical_cpus)));
}

crosapi::mojom::ProbeCpuResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::CpuResult::Tag::kCpuInfo:
      return crosapi::mojom::ProbeCpuResult::NewCpuInfo(
          ConvertProbePtr(std::move(input->get_cpu_info())));
    case cros_healthd::mojom::CpuResult::Tag::kError:
      return crosapi::mojom::ProbeCpuResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeDisplayResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::DisplayResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::DisplayResult::Tag::kDisplayInfo:
      return crosapi::mojom::ProbeDisplayResult::NewDisplayInfo(
          ConvertProbePtr(std::move(input->get_display_info())));
    case cros_healthd::mojom::DisplayResult::Tag::kError:
      return crosapi::mojom::ProbeDisplayResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeDisplayInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::DisplayInfoPtr input) {
  return crosapi::mojom::ProbeDisplayInfo::New(
      ConvertProbePtr(std::move(input->embedded_display)),
      ConvertOptionalPtrVector<crosapi::mojom::ProbeExternalDisplayInfoPtr>(
          std::move(input->external_displays)));
}

crosapi::mojom::ProbeEmbeddedDisplayInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::EmbeddedDisplayInfoPtr input) {
  return crosapi::mojom::ProbeEmbeddedDisplayInfo::New(
      input->privacy_screen_supported, input->privacy_screen_enabled,
      ConvertProbePtr(std::move(input->display_width)),
      ConvertProbePtr(std::move(input->display_height)),
      ConvertProbePtr(std::move(input->resolution_horizontal)),
      ConvertProbePtr(std::move(input->resolution_vertical)),
      ConvertProbePtr(std::move(input->refresh_rate)), input->manufacturer,
      ConvertProbePtr(std::move(input->model_id)),
      ConvertProbePtr(std::move(input->serial_number)),
      ConvertProbePtr(std::move(input->manufacture_week)),
      ConvertProbePtr(std::move(input->manufacture_year)), input->edid_version,
      Convert(input->input_type), input->display_name);
}

crosapi::mojom::ProbeExternalDisplayInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::ExternalDisplayInfoPtr input) {
  return crosapi::mojom::ProbeExternalDisplayInfo::New(
      ConvertProbePtr(std::move(input->display_width)),
      ConvertProbePtr(std::move(input->display_height)),
      ConvertProbePtr(std::move(input->resolution_horizontal)),
      ConvertProbePtr(std::move(input->resolution_vertical)),
      ConvertProbePtr(std::move(input->refresh_rate)), input->manufacturer,
      ConvertProbePtr(std::move(input->model_id)),
      ConvertProbePtr(std::move(input->serial_number)),
      ConvertProbePtr(std::move(input->manufacture_week)),
      ConvertProbePtr(std::move(input->manufacture_year)), input->edid_version,
      Convert(input->input_type), input->display_name);
}

crosapi::mojom::ProbeTimezoneInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneInfoPtr input) {
  return crosapi::mojom::ProbeTimezoneInfo::New(input->posix, input->region);
}

crosapi::mojom::ProbeTimezoneResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::TimezoneResult::Tag::kTimezoneInfo:
      return crosapi::mojom::ProbeTimezoneResult::NewTimezoneInfo(
          ConvertProbePtr(std::move(input->get_timezone_info())));
    case cros_healthd::mojom::TimezoneResult::Tag::kError:
      return crosapi::mojom::ProbeTimezoneResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeMemoryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryInfoPtr input) {
  return crosapi::mojom::ProbeMemoryInfo::New(
      Convert(input->total_memory_kib), Convert(input->free_memory_kib),
      Convert(input->available_memory_kib),
      Convert(static_cast<uint64_t>(input->page_faults_since_last_boot)));
}

crosapi::mojom::ProbeMemoryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::MemoryResult::Tag::kMemoryInfo:
      return crosapi::mojom::ProbeMemoryResult::NewMemoryInfo(
          ConvertProbePtr(std::move(input->get_memory_info())));
    case cros_healthd::mojom::MemoryResult::Tag::kError:
      return crosapi::mojom::ProbeMemoryResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeBacklightInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightInfoPtr input) {
  return crosapi::mojom::ProbeBacklightInfo::New(std::move(input->path),
                                                 Convert(input->max_brightness),
                                                 Convert(input->brightness));
}

crosapi::mojom::ProbeBacklightResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BacklightResult::Tag::kBacklightInfo:
      return crosapi::mojom::ProbeBacklightResult::NewBacklightInfo(
          ConvertPtrVector<crosapi::mojom::ProbeBacklightInfoPtr>(
              std::move(input->get_backlight_info())));
    case cros_healthd::mojom::BacklightResult::Tag::kError:
      return crosapi::mojom::ProbeBacklightResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeFanInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanInfoPtr input) {
  return crosapi::mojom::ProbeFanInfo::New(Convert(input->speed_rpm));
}

crosapi::mojom::ProbeFanResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::FanResult::Tag::kFanInfo:
      return crosapi::mojom::ProbeFanResult::NewFanInfo(
          ConvertPtrVector<crosapi::mojom::ProbeFanInfoPtr>(
              std::move(input->get_fan_info())));
    case cros_healthd::mojom::FanResult::Tag::kError:
      return crosapi::mojom::ProbeFanResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeStatefulPartitionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionInfoPtr input) {
  constexpr uint64_t k100MiB = 100 * 1024 * 1024;
  return crosapi::mojom::ProbeStatefulPartitionInfo::New(
      Convert(input->available_space / k100MiB * k100MiB),
      Convert(input->total_space));
}

crosapi::mojom::ProbeStatefulPartitionResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::StatefulPartitionResult::Tag::kPartitionInfo:
      return crosapi::mojom::ProbeStatefulPartitionResult::NewPartitionInfo(
          ConvertProbePtr(std::move(input->get_partition_info())));
    case cros_healthd::mojom::StatefulPartitionResult::Tag::kError:
      return crosapi::mojom::ProbeStatefulPartitionResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeBluetoothAdapterInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothAdapterInfoPtr input) {
  return crosapi::mojom::ProbeBluetoothAdapterInfo::New(
      std::move(input->name), std::move(input->address),
      Convert(input->powered), Convert(input->num_connected_devices));
}

crosapi::mojom::ProbeBluetoothResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::BluetoothResult::Tag::kBluetoothAdapterInfo:
      return crosapi::mojom::ProbeBluetoothResult::NewBluetoothAdapterInfo(
          ConvertPtrVector<crosapi::mojom::ProbeBluetoothAdapterInfoPtr>(
              std::move(input->get_bluetooth_adapter_info())));
    case cros_healthd::mojom::BluetoothResult::Tag::kError:
      return crosapi::mojom::ProbeBluetoothResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeSystemInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsInfoPtr input) {
  return crosapi::mojom::ProbeSystemInfo::New(crosapi::mojom::ProbeOsInfo::New(
      std::move(input->oem_name), ConvertProbePtr(std::move(input->os_version)),
      std::move(input->marketing_name)));
}

crosapi::mojom::ProbeOsVersionPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsVersionPtr input) {
  return crosapi::mojom::ProbeOsVersion::New(
      std::move(input->release_milestone), std::move(input->build_number),
      std::move(input->patch_number), std::move(input->release_channel));
}

crosapi::mojom::ProbeNetworkResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NetworkResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::NetworkResult::Tag::kNetworkHealth:
      return crosapi::mojom::ProbeNetworkResult::NewNetworkHealth(
          std::move(input->get_network_health()));
    case cros_healthd::mojom::NetworkResult::Tag::kError:
      return crosapi::mojom::ProbeNetworkResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

std::pair<crosapi::mojom::ProbeCachedVpdInfoPtr,
          crosapi::mojom::ProbeSystemInfoPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemInfoPtr input) {
  return std::make_pair(ConvertProbePtr(std::move(input->vpd_info)),
                        ConvertProbePtr(std::move(input->os_info)));
}

std::pair<crosapi::mojom::ProbeCachedVpdResultPtr,
          crosapi::mojom::ProbeSystemResultPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::SystemResult::Tag::kSystemInfo: {
      auto output = ConvertProbePairPtr(std::move(input->get_system_info()));
      return std::make_pair(
          crosapi::mojom::ProbeCachedVpdResult::NewVpdInfo(
              output.first ? std::move(output.first)
                           : crosapi::mojom::ProbeCachedVpdInfo::New()),
          crosapi::mojom::ProbeSystemResult::NewSystemInfo(
              output.second ? std::move(output.second)
                            : crosapi::mojom::ProbeSystemInfo::New()));
    }
    case cros_healthd::mojom::SystemResult::Tag::kError: {
      auto system_error = ConvertProbePtr(std::move(input->get_error()));
      return std::make_pair(
          crosapi::mojom::ProbeCachedVpdResult::NewError(system_error.Clone()),
          crosapi::mojom::ProbeSystemResult::NewError(system_error.Clone()));
    }
  }
}

crosapi::mojom::ProbeTpmVersionPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmVersionPtr input) {
  return crosapi::mojom::ProbeTpmVersion::New(
      Convert(input->gsc_version), Convert(input->family),
      Convert(input->spec_level), Convert(input->manufacturer),
      Convert(input->tpm_model), Convert(input->firmware_version),
      input->vendor_specific);
}

crosapi::mojom::ProbeTpmStatusPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmStatusPtr input) {
  return crosapi::mojom::ProbeTpmStatus::New(
      Convert(input->enabled), Convert(input->owned),
      Convert(input->owner_password_is_present));
}

crosapi::mojom::ProbeTpmDictionaryAttackPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmDictionaryAttackPtr input) {
  return crosapi::mojom::ProbeTpmDictionaryAttack::New(
      Convert(input->counter), Convert(input->threshold),
      Convert(input->lockout_in_effect),
      Convert(input->lockout_seconds_remaining));
}

crosapi::mojom::ProbeTpmInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmInfoPtr input) {
  return crosapi::mojom::ProbeTpmInfo::New(
      ConvertProbePtr(std::move(input->version)),
      ConvertProbePtr(std::move(input->status)),
      ConvertProbePtr(std::move(input->dictionary_attack)));
}

crosapi::mojom::ProbeTpmResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::TpmResult::Tag::kTpmInfo:
      return crosapi::mojom::ProbeTpmResult::NewTpmInfo(
          ConvertProbePtr(std::move(input->get_tpm_info())));
    case cros_healthd::mojom::TpmResult::Tag::kError:
      return crosapi::mojom::ProbeTpmResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeThermalSensorInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::ThermalSensorInfoPtr input) {
  return crosapi::mojom::ProbeThermalSensorInfo::New(
      input->name, input->temperature_celsius, Convert(input->source));
}

crosapi::mojom::ProbeThermalInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::ThermalInfoPtr input) {
  return crosapi::mojom::ProbeThermalInfo::New(
      ConvertPtrVector<crosapi::mojom::ProbeThermalSensorInfoPtr>(
          std::move(input->thermal_sensors)));
}

crosapi::mojom::ProbeThermalResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::ThermalResultPtr input) {
  switch (input->which()) {
    case cros_healthd::mojom::ThermalResult::Tag::kThermalInfo:
      return crosapi::mojom::ProbeThermalResult::NewThermalInfo(
          ConvertProbePtr(std::move(input->get_thermal_info())));
    case cros_healthd::mojom::ThermalResult::Tag::kError:
      return crosapi::mojom::ProbeThermalResult::NewError(
          ConvertProbePtr(std::move(input->get_error())));
  }
}

crosapi::mojom::ProbeTelemetryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TelemetryInfoPtr input) {
  auto system_result_output =
      ConvertProbePairPtr(std::move(input->system_result));

  return crosapi::mojom::ProbeTelemetryInfo::New(
      ConvertProbePtr(std::move(input->battery_result)),
      ConvertProbePtr(std::move(input->block_device_result)),
      std::move(system_result_output.first),
      ConvertProbePtr(std::move(input->cpu_result)),
      ConvertProbePtr(std::move(input->timezone_result)),
      ConvertProbePtr(std::move(input->memory_result)),
      ConvertProbePtr(std::move(input->backlight_result)),
      ConvertProbePtr(std::move(input->fan_result)),
      ConvertProbePtr(std::move(input->stateful_partition_result)),
      ConvertProbePtr(std::move(input->bluetooth_result)),
      std::move(system_result_output.second),
      ConvertProbePtr(std::move(input->network_result)),
      ConvertProbePtr(std::move(input->tpm_result)),
      ConvertProbePtr(std::move(input->audio_result)),
      ConvertProbePtr(std::move(input->bus_result)),
      ConvertProbePtr(std::move(input->display_result)));
}

}  // namespace unchecked

crosapi::mojom::ProbeErrorType Convert(cros_healthd::mojom::ErrorType input) {
  switch (input) {
    case cros_healthd::mojom::ErrorType::kUnknown:
      return crosapi::mojom::ProbeErrorType::kUnknown;
    case cros_healthd::mojom::ErrorType::kFileReadError:
      return crosapi::mojom::ProbeErrorType::kFileReadError;
    case cros_healthd::mojom::ErrorType::kParseError:
      return crosapi::mojom::ProbeErrorType::kParseError;
    case cros_healthd::mojom::ErrorType::kSystemUtilityError:
      return crosapi::mojom::ProbeErrorType::kSystemUtilityError;
    case cros_healthd::mojom::ErrorType::kServiceUnavailable:
      return crosapi::mojom::ProbeErrorType::kServiceUnavailable;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeUsbVersion Convert(cros_healthd::mojom::UsbVersion input) {
  switch (input) {
    case cros_healthd::mojom::UsbVersion::kUnmappedEnumField:
      return crosapi::mojom::ProbeUsbVersion::kUnknown;
    case cros_healthd::mojom::UsbVersion::kUnknown:
      return crosapi::mojom::ProbeUsbVersion::kUnknown;
    case cros_healthd::mojom::UsbVersion::kUsb1:
      return crosapi::mojom::ProbeUsbVersion::kUsb1;
    case cros_healthd::mojom::UsbVersion::kUsb2:
      return crosapi::mojom::ProbeUsbVersion::kUsb2;
    case cros_healthd::mojom::UsbVersion::kUsb3:
      return crosapi::mojom::ProbeUsbVersion::kUsb3;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeUsbSpecSpeed Convert(
    cros_healthd::mojom::UsbSpecSpeed input) {
  switch (input) {
    case cros_healthd::mojom::UsbSpecSpeed::kUnmappedEnumField:
    case cros_healthd::mojom::UsbSpecSpeed::kDeprecatedSpeed:
      return crosapi::mojom::ProbeUsbSpecSpeed::kUnknown;
    case cros_healthd::mojom::UsbSpecSpeed::kUnknown:
      return crosapi::mojom::ProbeUsbSpecSpeed::kUnknown;
    case cros_healthd::mojom::UsbSpecSpeed::k1_5Mbps:
      return crosapi::mojom::ProbeUsbSpecSpeed::k1_5Mbps;
    case cros_healthd::mojom::UsbSpecSpeed::k12Mbps:
      return crosapi::mojom::ProbeUsbSpecSpeed::k12Mbps;
    case cros_healthd::mojom::UsbSpecSpeed::k480Mbps:
      return crosapi::mojom::ProbeUsbSpecSpeed::k480Mbps;
    case cros_healthd::mojom::UsbSpecSpeed::k5Gbps:
      return crosapi::mojom::ProbeUsbSpecSpeed::k5Gbps;
    case cros_healthd::mojom::UsbSpecSpeed::k10Gbps:
      return crosapi::mojom::ProbeUsbSpecSpeed::k10Gbps;
    case cros_healthd::mojom::UsbSpecSpeed::k20Gbps:
      return crosapi::mojom::ProbeUsbSpecSpeed::k20Gbps;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeFwupdVersionFormat Convert(
    cros_healthd::mojom::FwupdVersionFormat input) {
  switch (input) {
    case cros_healthd::mojom::FwupdVersionFormat::kUnmappedEnumField:
      return crosapi::mojom::ProbeFwupdVersionFormat::kUnknown;
    case cros_healthd::mojom::FwupdVersionFormat::kUnknown:
      return crosapi::mojom::ProbeFwupdVersionFormat::kUnknown;
    case cros_healthd::mojom::FwupdVersionFormat::kPlain:
      return crosapi::mojom::ProbeFwupdVersionFormat::kPlain;
    case cros_healthd::mojom::FwupdVersionFormat::kNumber:
      return crosapi::mojom::ProbeFwupdVersionFormat::kNumber;
    case cros_healthd::mojom::FwupdVersionFormat::kPair:
      return crosapi::mojom::ProbeFwupdVersionFormat::kPair;
    case cros_healthd::mojom::FwupdVersionFormat::kTriplet:
      return crosapi::mojom::ProbeFwupdVersionFormat::kTriplet;
    case cros_healthd::mojom::FwupdVersionFormat::kQuad:
      return crosapi::mojom::ProbeFwupdVersionFormat::kQuad;
    case cros_healthd::mojom::FwupdVersionFormat::kBcd:
      return crosapi::mojom::ProbeFwupdVersionFormat::kBcd;
    case cros_healthd::mojom::FwupdVersionFormat::kIntelMe:
      return crosapi::mojom::ProbeFwupdVersionFormat::kIntelMe;
    case cros_healthd::mojom::FwupdVersionFormat::kIntelMe2:
      return crosapi::mojom::ProbeFwupdVersionFormat::kIntelMe2;
    case cros_healthd::mojom::FwupdVersionFormat::kSurfaceLegacy:
      return crosapi::mojom::ProbeFwupdVersionFormat::kSurfaceLegacy;
    case cros_healthd::mojom::FwupdVersionFormat::kSurface:
      return crosapi::mojom::ProbeFwupdVersionFormat::kSurface;
    case cros_healthd::mojom::FwupdVersionFormat::kDellBios:
      return crosapi::mojom::ProbeFwupdVersionFormat::kDellBios;
    case cros_healthd::mojom::FwupdVersionFormat::kHex:
      return crosapi::mojom::ProbeFwupdVersionFormat::kHex;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeCpuArchitectureEnum Convert(
    cros_healthd::mojom::CpuArchitectureEnum input) {
  switch (input) {
    case cros_healthd::mojom::CpuArchitectureEnum::kUnknown:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kUnknown;
    case cros_healthd::mojom::CpuArchitectureEnum::kX86_64:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kX86_64;
    case cros_healthd::mojom::CpuArchitectureEnum::kAArch64:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kAArch64;
    case cros_healthd::mojom::CpuArchitectureEnum::kArmv7l:
      return crosapi::mojom::ProbeCpuArchitectureEnum::kArmv7l;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeTpmGSCVersion Convert(
    cros_healthd::mojom::TpmGSCVersion input) {
  switch (input) {
    case cros_healthd::mojom::TpmGSCVersion::kNotGSC:
      return crosapi::mojom::ProbeTpmGSCVersion::kNotGSC;
    case cros_healthd::mojom::TpmGSCVersion::kCr50:
      return crosapi::mojom::ProbeTpmGSCVersion::kCr50;
    case cros_healthd::mojom::TpmGSCVersion::kTi50:
      return crosapi::mojom::ProbeTpmGSCVersion::kTi50;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeDisplayInputType Convert(
    cros_healthd::mojom::DisplayInputType input) {
  switch (input) {
    case cros_healthd::mojom::DisplayInputType::kUnmappedEnumField:
      return crosapi::mojom::ProbeDisplayInputType::kUnmappedEnumField;
    case cros_healthd::mojom::DisplayInputType::kDigital:
      return crosapi::mojom::ProbeDisplayInputType::kDigital;
    case cros_healthd::mojom::DisplayInputType::kAnalog:
      return crosapi::mojom::ProbeDisplayInputType::kAnalog;
  }
  NOTREACHED();
}

crosapi::mojom::ProbeThermalSensorSource Convert(
    cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource input) {
  switch (input) {
    case cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource::
        kUnmappedEnumField:
      return crosapi::mojom::ProbeThermalSensorSource::kUnmappedEnumField;
    case cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource::kEc:
      return crosapi::mojom::ProbeThermalSensorSource::kEc;
    case cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource::kSysFs:
      return crosapi::mojom::ProbeThermalSensorSource::kSysFs;
  }
  NOTREACHED();
}

crosapi::mojom::BoolValuePtr Convert(bool input) {
  return crosapi::mojom::BoolValue::New(input);
}

crosapi::mojom::DoubleValuePtr Convert(double input) {
  return crosapi::mojom::DoubleValue::New(input);
}

crosapi::mojom::Int64ValuePtr Convert(int64_t input) {
  return crosapi::mojom::Int64Value::New(input);
}

crosapi::mojom::UInt32ValuePtr Convert(uint32_t input) {
  return crosapi::mojom::UInt32Value::New(input);
}

crosapi::mojom::UInt64ValuePtr Convert(uint64_t input) {
  return crosapi::mojom::UInt64Value::New(input);
}

crosapi::mojom::ProbeAudioInputNodeInfoPtr ConvertAudioInputNodePtr(
    cros_healthd::mojom::AudioNodeInfoPtr input) {
  if (!input) {
    return crosapi::mojom::ProbeAudioInputNodeInfoPtr();
  }

  auto result = crosapi::mojom::ProbeAudioInputNodeInfo::New();

  result->id = crosapi::mojom::UInt64Value::New(input->id);
  result->name = input->name;
  result->device_name = input->device_name;
  result->active = crosapi::mojom::BoolValue::New(input->active);
  result->node_gain = crosapi::mojom::UInt8Value::New(input->input_node_gain);

  return result;
}

crosapi::mojom::ProbeAudioOutputNodeInfoPtr ConvertAudioOutputNodePtr(
    cros_healthd::mojom::AudioNodeInfoPtr input) {
  if (!input) {
    return crosapi::mojom::ProbeAudioOutputNodeInfoPtr();
  }

  auto result = crosapi::mojom::ProbeAudioOutputNodeInfo::New();

  result->id = crosapi::mojom::UInt64Value::New(input->id);
  result->name = input->name;
  result->device_name = input->device_name;
  result->active = crosapi::mojom::BoolValue::New(input->active);
  result->node_volume = crosapi::mojom::UInt8Value::New(input->node_volume);

  return result;
}

std::vector<cros_healthd::mojom::ProbeCategoryEnum> ConvertCategoryVector(
    const std::vector<crosapi::mojom::ProbeCategoryEnum>& input) {
  std::vector<cros_healthd::mojom::ProbeCategoryEnum> output;
  for (const auto element : input) {
    output.push_back(Convert(element));
  }
  return output;
}

}  // namespace ash::converters::telemetry

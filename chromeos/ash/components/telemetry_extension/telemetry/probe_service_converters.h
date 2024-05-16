// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_TELEMETRY_PROBE_SERVICE_CONVERTERS_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_TELEMETRY_PROBE_SERVICE_CONVERTERS_H_

#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include "base/check.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom-forward.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom-forward.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom-forward.h"
#include "chromeos/crosapi/mojom/probe_service.mojom-forward.h"

namespace ash::converters::telemetry {

// This file contains helper functions used by ProbeService to convert its
// types to/from cros_healthd ProbeService types.

namespace unchecked {

// Functions in unchecked namespace do not verify whether input pointer is
// nullptr, they should be called only via ConvertPtr wrapper that checks
// whether input pointer is nullptr.

crosapi::mojom::UInt64ValuePtr LegacyUncheckedConvertPtr(
    cros_healthd::mojom::NullableUint64Ptr input);

crosapi::mojom::ProbeErrorPtr UncheckedConvertPtr(
    cros_healthd::mojom::ProbeErrorPtr input);

std::optional<double> UncheckedConvertPtr(
    cros_healthd::mojom::NullableDoublePtr input);

std::optional<uint8_t> UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint8Ptr input);

std::optional<uint16_t> UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint16Ptr input);

std::optional<uint32_t> UncheckedConvertPtr(
    cros_healthd::mojom::NullableUint32Ptr input);

crosapi::mojom::ProbeAudioInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioInfoPtr input);

crosapi::mojom::ProbeAudioResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::AudioResultPtr input);

crosapi::mojom::ProbeUsbBusInterfaceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::UsbBusInterfaceInfoPtr input);

crosapi::mojom::ProbeFwupdFirmwareVersionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FwupdFirmwareVersionInfoPtr input);

crosapi::mojom::ProbeUsbBusInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::UsbBusInfoPtr input);

crosapi::mojom::ProbeBusInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BusDevicePtr input);

crosapi::mojom::ProbeBusInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BusInfoPtr input);

crosapi::mojom::ProbeBusResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BusResultPtr input);

crosapi::mojom::ProbeBatteryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryInfoPtr input);

crosapi::mojom::ProbeBatteryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BatteryResultPtr input);

crosapi::mojom::ProbeNonRemovableBlockDeviceInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr input);

crosapi::mojom::ProbeNonRemovableBlockDeviceResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NonRemovableBlockDeviceResultPtr input);

crosapi::mojom::ProbeCachedVpdInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::VpdInfoPtr input);

crosapi::mojom::ProbeCpuCStateInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuCStateInfoPtr input);

crosapi::mojom::ProbeLogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input);

crosapi::mojom::ProbeLogicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::LogicalCpuInfoPtr input,
    uint64_t user_hz);

crosapi::mojom::ProbePhysicalCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::PhysicalCpuInfoPtr input);

crosapi::mojom::ProbeCpuInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuInfoPtr input);

crosapi::mojom::ProbeCpuResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::CpuResultPtr input);

crosapi::mojom::ProbeDisplayResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::DisplayResultPtr input);

crosapi::mojom::ProbeDisplayInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::DisplayInfoPtr input);

crosapi::mojom::ProbeEmbeddedDisplayInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::EmbeddedDisplayInfoPtr input);

crosapi::mojom::ProbeExternalDisplayInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::ExternalDisplayInfoPtr input);

crosapi::mojom::ProbeTimezoneInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneInfoPtr input);

crosapi::mojom::ProbeTimezoneResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TimezoneResultPtr input);

crosapi::mojom::ProbeMemoryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryInfoPtr input);

crosapi::mojom::ProbeMemoryResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryResultPtr input);

crosapi::mojom::ProbeBacklightInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightInfoPtr input);

crosapi::mojom::ProbeBacklightResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BacklightResultPtr input);

crosapi::mojom::ProbeFanInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanInfoPtr input);

crosapi::mojom::ProbeFanResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanResultPtr input);

crosapi::mojom::ProbeStatefulPartitionInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionInfoPtr input);

crosapi::mojom::ProbeStatefulPartitionResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::StatefulPartitionResultPtr input);

crosapi::mojom::ProbeBluetoothAdapterInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothAdapterInfoPtr input);

crosapi::mojom::ProbeBluetoothResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::BluetoothResultPtr input);

crosapi::mojom::ProbeSystemInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsInfoPtr input);

crosapi::mojom::ProbeOsVersionPtr UncheckedConvertPtr(
    cros_healthd::mojom::OsVersionPtr);

crosapi::mojom::ProbeNetworkResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::NetworkResultPtr input);

std::pair<crosapi::mojom::ProbeCachedVpdInfoPtr,
          crosapi::mojom::ProbeSystemInfoPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemInfoPtr input);

std::pair<crosapi::mojom::ProbeCachedVpdResultPtr,
          crosapi::mojom::ProbeSystemResultPtr>
UncheckedConvertPairPtr(cros_healthd::mojom::SystemResultPtr input);

crosapi::mojom::ProbeTpmVersionPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmVersionPtr input);

crosapi::mojom::ProbeTpmStatusPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmStatusPtr input);

crosapi::mojom::ProbeTpmDictionaryAttackPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmDictionaryAttackPtr input);

crosapi::mojom::ProbeTpmInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmInfoPtr input);

crosapi::mojom::ProbeTpmResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::TpmResultPtr input);

crosapi::mojom::ProbeThermalSensorInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::ThermalSensorInfoPtr input);

crosapi::mojom::ProbeThermalInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::ThermalInfoPtr input);

crosapi::mojom::ProbeThermalResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::ThermalResultPtr input);

crosapi::mojom::ProbeTelemetryInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::TelemetryInfoPtr input);

}  // namespace unchecked

crosapi::mojom::ProbeErrorType Convert(cros_healthd::mojom::ErrorType type);

crosapi::mojom::ProbeCpuArchitectureEnum Convert(
    cros_healthd::mojom::CpuArchitectureEnum input);

crosapi::mojom::ProbeTpmGSCVersion Convert(
    cros_healthd::mojom::TpmGSCVersion input);

crosapi::mojom::ProbeUsbVersion Convert(cros_healthd::mojom::UsbVersion input);

crosapi::mojom::ProbeUsbSpecSpeed Convert(
    cros_healthd::mojom::UsbSpecSpeed input);

crosapi::mojom::ProbeFwupdVersionFormat Convert(
    cros_healthd::mojom::FwupdVersionFormat input);

crosapi::mojom::ProbeDisplayInputType Convert(
    cros_healthd::mojom::DisplayInputType input);

crosapi::mojom::ProbeThermalSensorSource Convert(
    cros_healthd::mojom::ThermalSensorInfo::ThermalSensorSource input);

crosapi::mojom::BoolValuePtr Convert(bool input);

crosapi::mojom::DoubleValuePtr Convert(double input);

crosapi::mojom::Int64ValuePtr Convert(int64_t input);

crosapi::mojom::UInt32ValuePtr Convert(uint32_t input);

crosapi::mojom::UInt64ValuePtr Convert(uint64_t input);

crosapi::mojom::ProbeAudioInputNodeInfoPtr ConvertAudioInputNodePtr(
    cros_healthd::mojom::AudioNodeInfoPtr input);

crosapi::mojom::ProbeAudioOutputNodeInfoPtr ConvertAudioOutputNodePtr(
    cros_healthd::mojom::AudioNodeInfoPtr input);

template <class OutputT, class InputT>
std::vector<OutputT> ConvertPtrVector(std::vector<InputT> input) {
  std::vector<OutputT> output;
  for (auto&& element : input) {
    DCHECK(!element.is_null());
    auto converted = unchecked::UncheckedConvertPtr(std::move(element));
    if (!converted.is_null()) {
      output.push_back(std::move(converted));
    }
  }
  return output;
}

template <class OutputT, class InputT>
std::optional<std::vector<OutputT>> ConvertOptionalPtrVector(
    std::optional<std::vector<InputT>> input) {
  if (!input.has_value()) {
    return std::nullopt;
  }
  return ConvertPtrVector<OutputT, InputT>(std::move(input.value()));
}

template <class InputT>
auto LegacyConvertProbePtr(InputT input) {
  return (!input.is_null())
             ? unchecked::LegacyUncheckedConvertPtr(std::move(input))
             : nullptr;
}

template <class InputT,
          class... Types,
          class OutputT = decltype(unchecked::UncheckedConvertPtr(
              std::declval<InputT>(),
              std::declval<Types>()...)),
          class = std::enable_if_t<std::is_default_constructible_v<OutputT>>>
OutputT ConvertProbePtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : OutputT();
}

template <class InputT>
auto ConvertProbePairPtr(InputT input) {
  return (!input.is_null())
             ? unchecked::UncheckedConvertPairPtr(std::move(input))
             : std::make_pair(nullptr, nullptr);
}

std::vector<cros_healthd::mojom::ProbeCategoryEnum> ConvertCategoryVector(
    const std::vector<crosapi::mojom::ProbeCategoryEnum>& input);

}  // namespace ash::converters::telemetry

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_TELEMETRY_PROBE_SERVICE_CONVERTERS_H_

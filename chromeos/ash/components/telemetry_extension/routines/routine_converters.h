// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_

#include <type_traits>
#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"

namespace ash::converters {

// This file contains helper functions used by
// TelemetryDiagnosticsRoutineServiceAsh to convert its types to/from
// cros_healthd routine types.

// Contains conversion functions that skip checking the `mojo::InlinedStructPtr`
// for null.
namespace unchecked {

crosapi::mojom::TelemetryDiagnosticMemtesterResultPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemtesterResultPtr input);

crosapi::mojom::TelemetryDiagnosticMemoryRoutineDetailPtr UncheckedConvertPtr(
    cros_healthd::mojom::MemoryRoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticFanRoutineDetailPtr UncheckedConvertPtr(
    cros_healthd::mojom::FanRoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticNetworkBandwidthRoutineDetailPtr
UncheckedConvertPtr(
    cros_healthd::mojom::NetworkBandwidthRoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticCameraFrameAnalysisRoutineDetailPtr
UncheckedConvertPtr(
    cros_healthd::mojom::CameraFrameAnalysisRoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateInitializedPtr
UncheckedConvertPtr(cros_healthd::mojom::RoutineStateInitializedPtr input);

crosapi::mojom::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfoPtr
UncheckedConvertPtr(
    cros_healthd::mojom::NetworkBandwidthRoutineRunningInfoPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineRunningInfoPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineRunningInfoPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateRunningPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateRunningPtr input);

crosapi::mojom::TelemetryDiagnosticCheckLedLitUpStateInquiryPtr
UncheckedConvertPtr(cros_healthd::mojom::CheckLedLitUpStateInquiryPtr input);

crosapi::mojom::TelemetryDiagnosticCheckKeyboardBacklightStateInquiryPtr
UncheckedConvertPtr(
    cros_healthd::mojom::CheckKeyboardBacklightStateInquiryPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineInquiryPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineInquiryPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineInteractionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineInteractionPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateWaitingPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateWaitingPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineDetailPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineDetailPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateFinishedPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateFinishedPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStateUnionPtr input);

crosapi::mojom::TelemetryDiagnosticRoutineStatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineStatePtr input);

cros_healthd::mojom::RoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr input);

cros_healthd::mojom::MemoryRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticMemoryRoutineArgumentPtr input);

cros_healthd::mojom::VolumeButtonRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticVolumeButtonRoutineArgumentPtr input);

cros_healthd::mojom::FanRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticFanRoutineArgumentPtr input);

cros_healthd::mojom::LedLitUpRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticLedLitUpRoutineArgumentPtr input);

cros_healthd::mojom::CheckLedLitUpStateReplyPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticCheckLedLitUpStateReplyPtr input);

cros_healthd::mojom::CheckKeyboardBacklightStateReplyPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticCheckKeyboardBacklightStateReplyPtr
        input);

cros_healthd::mojom::RoutineInquiryReplyPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticRoutineInquiryReplyPtr input);

cros_healthd::mojom::NetworkBandwidthRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticNetworkBandwidthRoutineArgumentPtr
        input);

cros_healthd::mojom::CameraFrameAnalysisRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticCameraFrameAnalysisRoutineArgumentPtr
        input);

cros_healthd::mojom::KeyboardBacklightRoutineArgumentPtr UncheckedConvertPtr(
    crosapi::mojom::TelemetryDiagnosticKeyboardBacklightRoutineArgumentPtr
        input);

}  // namespace unchecked

cros_healthd::mojom::LedName Convert(
    crosapi::mojom::TelemetryDiagnosticLedName input);

cros_healthd::mojom::LedColor Convert(
    crosapi::mojom::TelemetryDiagnosticLedColor input);

cros_healthd::mojom::CheckLedLitUpStateReply::State Convert(
    crosapi::mojom::TelemetryDiagnosticCheckLedLitUpStateReply::State input);

cros_healthd::mojom::CheckKeyboardBacklightStateReply::State Convert(
    crosapi::mojom::TelemetryDiagnosticCheckKeyboardBacklightStateReply::State
        input);

crosapi::mojom::TelemetryDiagnosticMemtesterTestItemEnum Convert(
    cros_healthd::mojom::MemtesterTestItemEnum input);

crosapi::mojom::TelemetryDiagnosticHardwarePresenceStatus Convert(
    cros_healthd::mojom::HardwarePresenceStatus input);

crosapi::mojom::TelemetryDiagnosticRoutineStateWaiting::Reason Convert(
    cros_healthd::mojom::RoutineStateWaiting::Reason input);

crosapi::mojom::TelemetryDiagnosticNetworkBandwidthRoutineRunningInfo::Type
Convert(cros_healthd::mojom::NetworkBandwidthRoutineRunningInfo::Type input);

crosapi::mojom::TelemetryDiagnosticCameraFrameAnalysisRoutineDetail::Issue
Convert(cros_healthd::mojom::CameraFrameAnalysisRoutineDetail::Issue input);

crosapi::mojom::TelemetryDiagnosticCameraSubtestResult Convert(
    cros_healthd::mojom::CameraSubtestResult input);

template <class InputT,
          class OutputT = decltype(Convert(std::declval<InputT>())),
          class = std::enable_if_t<std::is_enum_v<InputT>, bool>>
std::vector<OutputT> ConvertVector(std::vector<InputT> input) {
  std::vector<OutputT> result;
  for (auto elem : input) {
    result.push_back(Convert(elem));
  }
  return result;
}

template <class InputT>
auto ConvertRoutinePtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace ash::converters

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONVERTERS_H_

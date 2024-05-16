// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_CONVERTERS_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_CONVERTERS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom-forward.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"

namespace ash::converters::diagnostics {

// This file contains helper functions used by DiagnosticsService to convert its
// types to/from cros_healthd DiagnosticsService types.

namespace unchecked {

crosapi::mojom::DiagnosticsRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdatePtr input);

crosapi::mojom::DiagnosticsRoutineUpdateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdateUnionPtr input);

crosapi::mojom::DiagnosticsInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::InteractiveRoutineUpdatePtr input);

crosapi::mojom::DiagnosticsNonInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::NonInteractiveRoutineUpdatePtr input);

crosapi::mojom::DiagnosticsRunRoutineResponsePtr UncheckedConvertPtr(
    cros_healthd::mojom::RunRoutineResponsePtr input);

cros_healthd::mojom::NullableUint32Ptr UncheckedConvertPtr(
    crosapi::mojom::UInt32ValuePtr value);

}  // namespace unchecked

std::optional<crosapi::mojom::DiagnosticsRoutineEnum> Convert(
    cros_healthd::mojom::DiagnosticRoutineEnum input);

std::vector<crosapi::mojom::DiagnosticsRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input);

crosapi::mojom::DiagnosticsRoutineUserMessageEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineUserMessageEnum input);

crosapi::mojom::DiagnosticsRoutineStatusEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineStatusEnum input);

cros_healthd::mojom::DiagnosticRoutineCommandEnum Convert(
    crosapi::mojom::DiagnosticsRoutineCommandEnum input);

cros_healthd::mojom::AcPowerStatusEnum Convert(
    crosapi::mojom::DiagnosticsAcPowerStatusEnum input);

cros_healthd::mojom::NvmeSelfTestTypeEnum Convert(
    crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum input);

cros_healthd::mojom::DiskReadRoutineTypeEnum Convert(
    crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum input);

template <class InputT>
auto ConvertDiagnosticsPtr(InputT input) {
  return (!input.is_null()) ? unchecked::UncheckedConvertPtr(std::move(input))
                            : nullptr;
}

}  // namespace ash::converters::diagnostics

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_CONVERTERS_H_

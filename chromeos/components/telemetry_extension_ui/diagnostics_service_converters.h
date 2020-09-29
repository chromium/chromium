// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_CONVERTERS_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_CONVERTERS_H_

#if defined(OFFICIAL_BUILD)
#error Diagnostics service should only be included in unofficial builds.
#endif

#include <string>
#include <utility>
#include <vector>

#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom-forward.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "mojo/public/cpp/system/handle.h"

namespace chromeos {
namespace converters {

// This file contains helper functions used by DiagnosticsService to convert its
// types to/from cros_healthd DiagnosticsService types.

namespace unchecked {

health::mojom::RoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdatePtr input);

health::mojom::RoutineUpdateUnionPtr UncheckedConvertPtr(
    cros_healthd::mojom::RoutineUpdateUnionPtr input);

health::mojom::InteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::InteractiveRoutineUpdatePtr input);

health::mojom::NonInteractiveRoutineUpdatePtr UncheckedConvertPtr(
    cros_healthd::mojom::NonInteractiveRoutineUpdatePtr input);

health::mojom::RunRoutineResponsePtr UncheckedConvertPtr(
    cros_healthd::mojom::RunRoutineResponsePtr input);

}  // namespace unchecked

base::Optional<health::mojom::DiagnosticRoutineEnum> Convert(
    cros_healthd::mojom::DiagnosticRoutineEnum input);

std::vector<health::mojom::DiagnosticRoutineEnum> Convert(
    const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>& input);

health::mojom::DiagnosticRoutineUserMessageEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineUserMessageEnum input);

health::mojom::DiagnosticRoutineStatusEnum Convert(
    cros_healthd::mojom::DiagnosticRoutineStatusEnum input);

cros_healthd::mojom::DiagnosticRoutineCommandEnum Convert(
    health::mojom::DiagnosticRoutineCommandEnum input);

cros_healthd::mojom::AcPowerStatusEnum Convert(
    health::mojom::AcPowerStatusEnum input);

cros_healthd::mojom::NvmeSelfTestTypeEnum Convert(
    health::mojom::NvmeSelfTestTypeEnum input);

cros_healthd::mojom::DiskReadRoutineTypeEnum Convert(
    health::mojom::DiskReadRoutineTypeEnum input);

}  // namespace converters
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_CONVERTERS_H_

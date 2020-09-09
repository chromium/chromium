// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/diagnostics_service.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "chromeos/components/telemetry_extension_ui/convert_ptr.h"
#include "chromeos/components/telemetry_extension_ui/diagnostics_service_converters.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"

namespace chromeos {

DiagnosticsService::DiagnosticsService(
    mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver)
    : receiver_(this, std::move(receiver)) {}

DiagnosticsService::~DiagnosticsService() = default;

cros_healthd::mojom::CrosHealthdDiagnosticsService*
DiagnosticsService::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetDiagnosticsService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(base::BindOnce(
        &DiagnosticsService::OnDisconnect, base::Unretained(this)));
  }
  return service_.get();
}

void DiagnosticsService::OnDisconnect() {
  service_.reset();
}

void DiagnosticsService::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  GetService()->GetAvailableRoutines(base::BindOnce(
      [](health::mojom::DiagnosticsService::GetAvailableRoutinesCallback
             callback,
         const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>&
             routines) {
        std::move(callback).Run(converters::Convert(routines));
      },
      std::move(callback)));
}

void DiagnosticsService::GetRoutineUpdate(
    int32_t id,
    health::mojom::DiagnosticRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  GetService()->GetRoutineUpdate(
      id, converters::Convert(command), include_output,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::GetRoutineUpdateCallback
                 callback,
             cros_healthd::mojom::RoutineUpdatePtr ptr) {
            std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunBatteryCapacityRoutine(
    uint32_t low_mah,
    uint32_t high_mah,
    RunBatteryCapacityRoutineCallback callback) {
  GetService()->RunBatteryCapacityRoutine(
      low_mah, high_mah,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::
                 RunBatteryCapacityRoutineCallback callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunBatteryHealthRoutine(
    uint32_t maximum_cycle_count,
    uint32_t percent_battery_wear_allowed,
    RunBatteryHealthRoutineCallback callback) {
  GetService()->RunBatteryHealthRoutine(
      maximum_cycle_count, percent_battery_wear_allowed,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunBatteryHealthRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunSmartctlCheckRoutine(
    RunSmartctlCheckRoutineCallback callback) {
  GetService()->RunSmartctlCheckRoutine(base::BindOnce(
      [](health::mojom::DiagnosticsService::RunSmartctlCheckRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsService::RunAcPowerRoutine(
    health::mojom::AcPowerStatusEnum expected_status,
    const base::Optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  GetService()->RunAcPowerRoutine(
      converters::Convert(expected_status), expected_power_type,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunAcPowerRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunCpuCacheRoutine(
    uint32_t length_seconds,
    RunCpuCacheRoutineCallback callback) {
  GetService()->RunCpuCacheRoutine(
      length_seconds,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunCpuCacheRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsService::RunCpuStressRoutine(
    uint32_t length_seconds,
    RunCpuStressRoutineCallback callback) {
  GetService()->RunCpuStressRoutine(
      length_seconds,
      base::BindOnce(
          [](health::mojom::DiagnosticsService::RunCpuStressRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(converters::ConvertPtr(std::move(ptr)));
          },
          std::move(callback)));
}

}  // namespace chromeos

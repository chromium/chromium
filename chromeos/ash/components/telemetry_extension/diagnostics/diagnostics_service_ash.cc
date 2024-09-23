// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// static
DiagnosticsServiceAsh::Factory* DiagnosticsServiceAsh::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<crosapi::mojom::DiagnosticsService>
DiagnosticsServiceAsh::Factory::Create(
    mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  auto diagnostics_service = std::make_unique<DiagnosticsServiceAsh>();
  diagnostics_service->BindReceiver(std::move(receiver));
  return diagnostics_service;
}

// static
void DiagnosticsServiceAsh::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

DiagnosticsServiceAsh::Factory::~Factory() = default;

DiagnosticsServiceAsh::DiagnosticsServiceAsh() = default;

DiagnosticsServiceAsh::~DiagnosticsServiceAsh() = default;

void DiagnosticsServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

cros_healthd::mojom::CrosHealthdDiagnosticsService*
DiagnosticsServiceAsh::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindDiagnosticsService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(base::BindOnce(
        &DiagnosticsServiceAsh::OnDisconnect, base::Unretained(this)));
  }
  return service_.get();
}

void DiagnosticsServiceAsh::OnDisconnect() {
  service_.reset();
}

void DiagnosticsServiceAsh::GetAvailableRoutines(
    GetAvailableRoutinesCallback callback) {
  GetService()->GetAvailableRoutines(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::GetAvailableRoutinesCallback
             callback,
         const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>&
             routines) {
        std::move(callback).Run(converters::diagnostics::Convert(routines));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::GetRoutineUpdate(
    int32_t id,
    crosapi::mojom::DiagnosticsRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  GetService()->GetRoutineUpdate(
      id, converters::diagnostics::Convert(command), include_output,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::GetRoutineUpdateCallback
                 callback,
             cros_healthd::mojom::RoutineUpdatePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunAcPowerRoutine(
    crosapi::mojom::DiagnosticsAcPowerStatusEnum expected_status,
    const std::optional<std::string>& expected_power_type,
    RunAcPowerRoutineCallback callback) {
  GetService()->RunAcPowerRoutine(
      converters::diagnostics::Convert(expected_status), expected_power_type,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunAcPowerRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunAudioDriverRoutine(
    RunAudioDriverRoutineCallback callback) {
  GetService()->RunAudioDriverRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunAudioDriverRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunBatteryCapacityRoutine(
    RunBatteryCapacityRoutineCallback callback) {
  GetService()->RunBatteryCapacityRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunBatteryCapacityRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunBatteryChargeRoutine(
    uint32_t length_seconds,
    uint32_t minimum_charge_percent_required,
    RunBatteryChargeRoutineCallback callback) {
  GetService()->RunBatteryChargeRoutine(
      length_seconds, minimum_charge_percent_required,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunBatteryChargeRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunBatteryDischargeRoutine(
    uint32_t length_seconds,
    uint32_t maximum_discharge_percent_allowed,
    RunBatteryDischargeRoutineCallback callback) {
  GetService()->RunBatteryDischargeRoutine(
      length_seconds, maximum_discharge_percent_allowed,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::
                 RunBatteryDischargeRoutineCallback callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunBatteryHealthRoutine(
    RunBatteryHealthRoutineCallback callback) {
  GetService()->RunBatteryHealthRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunBatteryHealthRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunBluetoothDiscoveryRoutine(
    RunBluetoothDiscoveryRoutineCallback callback) {
  GetService()->RunBluetoothDiscoveryRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::
             RunBluetoothDiscoveryRoutineCallback callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunBluetoothPairingRoutine(
    const std::string& peripheral_id,
    RunBluetoothPairingRoutineCallback callback) {
  GetService()->RunBluetoothPairingRoutine(
      peripheral_id,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::
                 RunBluetoothPairingRoutineCallback callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunBluetoothPowerRoutine(
    RunBluetoothPowerRoutineCallback callback) {
  GetService()->RunBluetoothPowerRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunBluetoothPowerRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunBluetoothScanningRoutine(
    uint32_t length_seconds,
    RunBluetoothScanningRoutineCallback callback) {
  GetService()->RunBluetoothScanningRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::
                 RunBluetoothScanningRoutineCallback callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunCpuCacheRoutine(
    uint32_t length_seconds,
    RunCpuCacheRoutineCallback callback) {
  GetService()->RunCpuCacheRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunCpuCacheRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunCpuStressRoutine(
    uint32_t length_seconds,
    RunCpuStressRoutineCallback callback) {
  GetService()->RunCpuStressRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunCpuStressRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunDiskReadRoutine(
    crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum type,
    uint32_t length_seconds,
    uint32_t file_size_mb,
    RunDiskReadRoutineCallback callback) {
  GetService()->RunDiskReadRoutine(
      converters::diagnostics::Convert(type), length_seconds, file_size_mb,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunDiskReadRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunDnsResolutionRoutine(
    RunDnsResolutionRoutineCallback callback) {
  GetService()->RunDnsResolutionRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunDnsResolutionRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunDnsResolverPresentRoutine(
    RunDnsResolverPresentRoutineCallback callback) {
  GetService()->RunDnsResolverPresentRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::
             RunDnsResolverPresentRoutineCallback callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunEmmcLifetimeRoutine(
    RunEmmcLifetimeRoutineCallback callback) {
  GetService()->RunEmmcLifetimeRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunEmmcLifetimeRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunFanRoutine(RunFanRoutineCallback callback) {
  GetService()->RunFanRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunFanRoutineCallback callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunFingerprintAliveRoutine(
    RunFingerprintAliveRoutineCallback callback) {
  GetService()->RunFingerprintAliveRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunFingerprintAliveRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunFloatingPointAccuracyRoutine(
    uint32_t length_seconds,
    RunFloatingPointAccuracyRoutineCallback callback) {
  GetService()->RunFloatingPointAccuracyRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::
                 RunFloatingPointAccuracyRoutineCallback callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunGatewayCanBePingedRoutine(
    RunGatewayCanBePingedRoutineCallback callback) {
  GetService()->RunGatewayCanBePingedRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::
             RunGatewayCanBePingedRoutineCallback callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunLanConnectivityRoutine(
    RunLanConnectivityRoutineCallback callback) {
  GetService()->RunLanConnectivityRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunLanConnectivityRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunMemoryRoutine(
    RunMemoryRoutineCallback callback) {
  GetService()->RunMemoryRoutine(
      std::nullopt,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunMemoryRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunNvmeSelfTestRoutine(
    crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum nvme_self_test_type,
    RunNvmeSelfTestRoutineCallback callback) {
  GetService()->RunNvmeSelfTestRoutine(
      converters::diagnostics::Convert(nvme_self_test_type),
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunNvmeSelfTestRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::DEPRECATED_RunNvmeWearLevelRoutine(
    uint32_t wear_level_threshold,
    DEPRECATED_RunNvmeWearLevelRoutineCallback callback) {
  // This routine is deprecated.
  std::move(callback).Run(nullptr);
}

void DiagnosticsServiceAsh::RunPrimeSearchRoutine(
    uint32_t length_seconds,
    RunPrimeSearchRoutineCallback callback) {
  GetService()->RunPrimeSearchRoutine(
      cros_healthd::mojom::NullableUint32::New(length_seconds),
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunPrimeSearchRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunSensitiveSensorRoutine(
    RunSensitiveSensorRoutineCallback callback) {
  GetService()->RunSensitiveSensorRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunSensitiveSensorRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunSignalStrengthRoutine(
    RunSignalStrengthRoutineCallback callback) {
  GetService()->RunSignalStrengthRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunSignalStrengthRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunSmartctlCheckRoutine(
    crosapi::mojom::UInt32ValuePtr percentage_used_threshold,
    RunSmartctlCheckRoutineCallback callback) {
  GetService()->RunSmartctlCheckRoutine(
      converters::diagnostics::ConvertDiagnosticsPtr(
          std::move(percentage_used_threshold)),
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunSmartctlCheckRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

void DiagnosticsServiceAsh::RunUfsLifetimeRoutine(
    RunUfsLifetimeRoutineCallback callback) {
  GetService()->RunUfsLifetimeRoutine(base::BindOnce(
      [](crosapi::mojom::DiagnosticsService::RunUfsLifetimeRoutineCallback
             callback,
         cros_healthd::mojom::RunRoutineResponsePtr ptr) {
        std::move(callback).Run(
            converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
      },
      std::move(callback)));
}

void DiagnosticsServiceAsh::RunPowerButtonRoutine(
    uint32_t timeout_seconds,
    RunPowerButtonRoutineCallback callback) {
  GetService()->RunPowerButtonRoutine(
      timeout_seconds,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::RunPowerButtonRoutineCallback
                 callback,
             cros_healthd::mojom::RunRoutineResponsePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

}  // namespace ash

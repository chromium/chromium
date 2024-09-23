// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_ASH_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_ASH_H_

#include <memory>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class DiagnosticsServiceAsh : public crosapi::mojom::DiagnosticsService {
 public:
  class Factory {
   public:
    static std::unique_ptr<crosapi::mojom::DiagnosticsService> Create(
        mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver);

    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<crosapi::mojom::DiagnosticsService> CreateInstance(
        mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) = 0;

   private:
    static Factory* test_factory_;
  };

  DiagnosticsServiceAsh();
  DiagnosticsServiceAsh(const DiagnosticsServiceAsh&) = delete;
  DiagnosticsServiceAsh& operator=(const DiagnosticsServiceAsh&) = delete;
  ~DiagnosticsServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver);

 private:
  // Ensures that |service_| created and connected to the
  // CrosHealthdDiagnosticsService.
  cros_healthd::mojom::CrosHealthdDiagnosticsService* GetService();

  void OnDisconnect();

  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        crosapi::mojom::DiagnosticsRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;
  void RunAcPowerRoutine(
      crosapi::mojom::DiagnosticsAcPowerStatusEnum expected_status,
      const std::optional<std::string>& expected_power_type,
      RunAcPowerRoutineCallback callback) override;
  void RunAudioDriverRoutine(RunAudioDriverRoutineCallback callback) override;
  void RunBatteryCapacityRoutine(
      RunBatteryCapacityRoutineCallback callback) override;
  void RunBatteryChargeRoutine(
      uint32_t length_seconds,
      uint32_t minimum_charge_percent_required,
      RunBatteryChargeRoutineCallback callback) override;
  void RunBatteryDischargeRoutine(
      uint32_t length_seconds,
      uint32_t maximum_discharge_percent_allowed,
      RunBatteryDischargeRoutineCallback callback) override;
  void RunBatteryHealthRoutine(
      RunBatteryHealthRoutineCallback callback) override;
  void RunBluetoothDiscoveryRoutine(
      RunBluetoothDiscoveryRoutineCallback) override;
  void RunBluetoothPairingRoutine(
      const std::string& peripheral_id,
      RunBluetoothPairingRoutineCallback callback) override;
  void RunBluetoothPowerRoutine(RunBluetoothPowerRoutineCallback) override;
  void RunBluetoothScanningRoutine(
      uint32_t length_seconds,
      RunBluetoothScanningRoutineCallback callback) override;
  void RunCpuCacheRoutine(uint32_t length_seconds,
                          RunCpuCacheRoutineCallback callback) override;
  void RunCpuStressRoutine(uint32_t length_seconds,
                           RunCpuStressRoutineCallback callback) override;
  void RunDiskReadRoutine(
      crosapi::mojom::DiagnosticsDiskReadRoutineTypeEnum type,
      uint32_t length_seconds,
      uint32_t file_size_mb,
      RunDiskReadRoutineCallback callback) override;
  void RunDnsResolutionRoutine(
      RunDnsResolutionRoutineCallback callback) override;
  void RunDnsResolverPresentRoutine(
      RunDnsResolverPresentRoutineCallback callback) override;
  void RunEmmcLifetimeRoutine(RunEmmcLifetimeRoutineCallback callback) override;
  void RunFanRoutine(RunFanRoutineCallback callback) override;
  void RunFingerprintAliveRoutine(
      RunFingerprintAliveRoutineCallback callback) override;
  void RunFloatingPointAccuracyRoutine(
      uint32_t length_seconds,
      RunFloatingPointAccuracyRoutineCallback callback) override;
  void RunGatewayCanBePingedRoutine(
      RunGatewayCanBePingedRoutineCallback callback) override;
  void RunLanConnectivityRoutine(
      RunLanConnectivityRoutineCallback callback) override;
  void RunMemoryRoutine(RunMemoryRoutineCallback callback) override;
  void RunNvmeSelfTestRoutine(
      crosapi::mojom::DiagnosticsNvmeSelfTestTypeEnum nvme_self_test_type,
      RunNvmeSelfTestRoutineCallback callback) override;
  void DEPRECATED_RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      DEPRECATED_RunNvmeWearLevelRoutineCallback callback) override;
  void RunPrimeSearchRoutine(uint32_t length_seconds,
                             RunPrimeSearchRoutineCallback callback) override;
  void RunSensitiveSensorRoutine(
      RunSensitiveSensorRoutineCallback callback) override;
  void RunSignalStrengthRoutine(
      RunSignalStrengthRoutineCallback callback) override;
  void RunSmartctlCheckRoutine(
      crosapi::mojom::UInt32ValuePtr percentage_used_threshold,
      RunSmartctlCheckRoutineCallback callback) override;
  void RunUfsLifetimeRoutine(RunUfsLifetimeRoutineCallback callback) override;
  void RunPowerButtonRoutine(uint32_t timeout_seconds,
                             RunPowerButtonRoutineCallback callback) override;

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  //
  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::DiagnosticsService> receivers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_DIAGNOSTICS_DIAGNOSTICS_SERVICE_ASH_H_

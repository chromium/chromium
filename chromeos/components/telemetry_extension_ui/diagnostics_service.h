// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_H_

#if defined(OFFICIAL_BUILD)
#error Diagnostics service should only be included in unofficial builds.
#endif

#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class DiagnosticsService : public health::mojom::DiagnosticsService {
 public:
  explicit DiagnosticsService(
      mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver);
  DiagnosticsService(const DiagnosticsService&) = delete;
  DiagnosticsService& operator=(const DiagnosticsService&) = delete;
  ~DiagnosticsService() override;

 private:
  // Ensures that |service_| created and connected to the
  // CrosHealthdDiagnosticsService.
  cros_healthd::mojom::CrosHealthdDiagnosticsService* GetService();

  void OnDisconnect();

  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        health::mojom::DiagnosticRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;
  void RunBatteryCapacityRoutine(
      uint32_t low_mah,
      uint32_t high_mah,
      RunBatteryCapacityRoutineCallback callback) override;
  void RunBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed,
      RunBatteryHealthRoutineCallback callback) override;
  void RunSmartctlCheckRoutine(
      RunSmartctlCheckRoutineCallback callback) override;
  void RunAcPowerRoutine(health::mojom::AcPowerStatusEnum expected_status,
                         const base::Optional<std::string>& expected_power_type,
                         RunAcPowerRoutineCallback callback) override;
  void RunCpuCacheRoutine(uint32_t length_seconds,
                          RunCpuCacheRoutineCallback callback) override;
  void RunCpuStressRoutine(uint32_t length_seconds,
                           RunCpuStressRoutineCallback callback) override;
  void RunFloatingPointAccuracyRoutine(
      uint32_t length_seconds,
      RunFloatingPointAccuracyRoutineCallback callback) override;
  void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      RunNvmeWearLevelRoutineCallback callback) override;
  void RunNvmeSelfTestRoutine(
      health::mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
      RunNvmeSelfTestRoutineCallback callback) override;
  void RunDiskReadRoutine(health::mojom::DiskReadRoutineTypeEnum type,
                          uint32_t length_seconds,
                          uint32_t file_size_mb,
                          RunDiskReadRoutineCallback callback) override;
  void RunPrimeSearchRoutine(uint32_t length_seconds,
                             uint64_t max_num,
                             RunPrimeSearchRoutineCallback callback) override;
  void RunBatteryDischargeRoutine(
      uint32_t length_seconds,
      uint32_t maximum_discharge_percent_allowed,
      RunBatteryDischargeRoutineCallback callback) override;
  void RunBatteryChargeRoutine(
      uint32_t length_seconds,
      uint32_t minimum_charge_percent_required,
      RunBatteryChargeRoutineCallback callback) override;

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  mojo::Receiver<health::mojom::DiagnosticsService> receiver_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_DIAGNOSTICS_SERVICE_H_

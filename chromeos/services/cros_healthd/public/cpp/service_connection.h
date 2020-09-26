// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include <sys/types.h>

#include <cstdint>
#include <string>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {
namespace cros_healthd {

// Encapsulates a connection to the Chrome OS cros_healthd daemon via its Mojo
// interface.
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();

  using BindNetworkHealthServiceCallback =
      base::RepeatingCallback<mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkHealthService>()>;
  using BindNetworkDiagnosticsRoutinesCallback =
      base::RepeatingCallback<mojo::PendingRemote<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>()>;

  // Retrieve a list of available diagnostic routines. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetAvailableRoutines(
      mojom::CrosHealthdDiagnosticsService::GetAvailableRoutinesCallback
          callback) = 0;

  // Send a command to an existing routine. Also returns status information
  // for the routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetRoutineUpdate(
      int32_t id,
      mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      mojom::CrosHealthdDiagnosticsService::GetRoutineUpdateCallback
          callback) = 0;

  // Requests that cros_healthd runs the urandom routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunUrandomRoutine(
      uint32_t length_seconds,
      mojom::CrosHealthdDiagnosticsService::RunUrandomRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery capacity routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryCapacityRoutine(
      uint32_t low_mah,
      uint32_t high_mah,
      mojom::CrosHealthdDiagnosticsService::RunBatteryCapacityRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery health routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryHealthRoutine(
      uint32_t maximum_cycle_count,
      uint32_t percent_battery_wear_allowed,
      mojom::CrosHealthdDiagnosticsService::RunBatteryHealthRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the smartcl check routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunSmartctlCheckRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSmartctlCheckRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the AC power routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunAcPowerRoutine(
      mojom::AcPowerStatusEnum expected_status,
      const base::Optional<std::string>& expected_power_type,
      mojom::CrosHealthdDiagnosticsService::RunAcPowerRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the CPU cache routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunCpuCacheRoutine(
      const base::TimeDelta& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuCacheRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the CPU stress routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunCpuStressRoutine(
      const base::TimeDelta& exec_duration,
      mojom::CrosHealthdDiagnosticsService::RunCpuStressRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the floating point accuracy routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunFloatingPointAccuracyRoutine(
      const base::TimeDelta& exec_duration,
      mojom::CrosHealthdDiagnosticsService::
          RunFloatingPointAccuracyRoutineCallback callback) = 0;

  // Requests that cros_healthd runs the NVMe wear-level routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      mojom::CrosHealthdDiagnosticsService::RunNvmeWearLevelRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the NVMe self-test routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunNvmeSelfTestRoutine(
      mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
      mojom::CrosHealthdDiagnosticsService::RunNvmeSelfTestRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the Disk Read routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunDiskReadRoutine(
      mojom::DiskReadRoutineTypeEnum type,
      base::TimeDelta& exec_duration,
      uint32_t file_size_mb,
      mojom::CrosHealthdDiagnosticsService::RunDiskReadRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the prime search routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunPrimeSearchRoutine(
      base::TimeDelta& exec_duration,
      uint64_t max_num,
      mojom::CrosHealthdDiagnosticsService::RunPrimeSearchRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery discharge routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryDischargeRoutine(
      base::TimeDelta exec_duration,
      uint32_t maximum_discharge_percent_allowed,
      mojom::CrosHealthdDiagnosticsService::RunBatteryDischargeRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the battery charge routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunBatteryChargeRoutine(
      base::TimeDelta exec_duration,
      uint32_t minimum_charge_percent_required,
      mojom::CrosHealthdDiagnosticsService::RunBatteryChargeRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the memory routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunMemoryRoutine(
      mojom::CrosHealthdDiagnosticsService::RunMemoryRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the lan connectivity routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunLanConnectivityRoutine(
      mojom::CrosHealthdDiagnosticsService::RunLanConnectivityRoutineCallback
          callback) = 0;

  // Requests that cros_healthd runs the lan connectivity routine. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void RunSignalStrengthRoutine(
      mojom::CrosHealthdDiagnosticsService::RunSignalStrengthRoutineCallback
          callback) = 0;

  // Subscribes to cros_healthd's Bluetooth-related events. See
  // src/chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddBluetoothObserver(
      mojo::PendingRemote<mojom::CrosHealthdBluetoothObserver>
          pending_observer) = 0;

  // Subscribes to cros_healthd's lid-related events. See
  // src/chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddLidObserver(
      mojo::PendingRemote<mojom::CrosHealthdLidObserver> pending_observer) = 0;

  // Subscribes to cros_healthd's power-related events. See
  // src/chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void AddPowerObserver(
      mojo::PendingRemote<mojom::CrosHealthdPowerObserver>
          pending_observer) = 0;

  // Gathers pieces of information about the platform. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories_to_test,
      mojom::CrosHealthdProbeService::ProbeTelemetryInfoCallback callback) = 0;

  // Gathers information about a particular process on the device. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void ProbeProcessInfo(
      pid_t process_id,
      mojom::CrosHealthdProbeService::ProbeProcessInfoCallback callback) = 0;

  // Binds |service| to an implementation of CrosHealthdDiagnosticsService. In
  // production, this implementation is provided by cros_healthd. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetDiagnosticsService(
      mojom::CrosHealthdDiagnosticsServiceRequest service) = 0;

  // Binds |service| to an implementation of CrosHealthdProbeService. In
  // production, this implementation is provided by cros_healthd. See
  // src/chromeos/service/cros_healthd/public/mojom/cros_healthd.mojom for
  // details.
  virtual void GetProbeService(
      mojom::CrosHealthdProbeServiceRequest service) = 0;

  // Sets a callback to request binding a PendingRemote to the
  // NetworkHealthService. This callback is invoked once when it is set, and
  // anytime the mojo connection to CrosHealthd is disconnected.
  virtual void SetBindNetworkHealthServiceCallback(
      BindNetworkHealthServiceCallback callback) = 0;

  // Sets a callback to request binding a PendingRemote to the
  // NetworkDiagnosticsRoutines interface. This callback is invoked once when it
  // is set, and anytime the mojo connection to CrosHealthd is disconnected.
  virtual void SetBindNetworkDiagnosticsRoutinesCallback(
      BindNetworkDiagnosticsRoutinesCallback callback) = 0;

  // Calls FlushForTesting method on all mojo::Remote objects owned by
  // ServiceConnection. This method can be used for example to gracefully
  // observe destruction of the cros_healthd client.
  virtual void FlushForTesting() = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;
};

}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_SERVICE_CONNECTION_H_

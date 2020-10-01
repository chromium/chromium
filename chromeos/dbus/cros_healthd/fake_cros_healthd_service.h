// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_SERVICE_H_
#define CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace cros_healthd {

// This class serves as a fake for all four of cros_healthd's mojo interfaces.
// The factory methods bind to receivers held within FakeCrosHealtdService, and
// all requests on each of the interfaces are fulfilled by
// FakeCrosHealthdService.
class FakeCrosHealthdService final
    : public mojom::CrosHealthdServiceFactory,
      public mojom::CrosHealthdDiagnosticsService,
      public mojom::CrosHealthdEventService,
      public mojom::CrosHealthdProbeService {
 public:
  FakeCrosHealthdService();
  ~FakeCrosHealthdService() override;

  // CrosHealthdServiceFactory overrides:
  void GetProbeService(mojom::CrosHealthdProbeServiceRequest service) override;
  void GetDiagnosticsService(
      mojom::CrosHealthdDiagnosticsServiceRequest service) override;
  void GetEventService(mojom::CrosHealthdEventServiceRequest service) override;
  void SendNetworkHealthService(
      mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
          remote) override;
  void SendNetworkDiagnosticsRoutines(
      mojo::PendingRemote<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
          network_diagnostics_routines) override;

  // CrosHealthdDiagnosticsService overrides:
  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(int32_t id,
                        mojom::DiagnosticRoutineCommandEnum command,
                        bool include_output,
                        GetRoutineUpdateCallback callback) override;
  void RunUrandomRoutine(uint32_t length_seconds,
                         RunUrandomRoutineCallback callback) override;
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
  void RunAcPowerRoutine(mojom::AcPowerStatusEnum expected_status,
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
  void RunNvmeSelfTestRoutine(mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
                              RunNvmeSelfTestRoutineCallback callback) override;
  void RunDiskReadRoutine(mojom::DiskReadRoutineTypeEnum type,
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
  void RunMemoryRoutine(RunMemoryRoutineCallback callback) override;
  void RunLanConnectivityRoutine(
      RunLanConnectivityRoutineCallback callback) override;
  void RunSignalStrengthRoutine(
      RunSignalStrengthRoutineCallback callback) override;

  // CrosHealthdEventService overrides:
  void AddBluetoothObserver(
      mojom::CrosHealthdBluetoothObserverPtr observer) override;
  void AddLidObserver(mojom::CrosHealthdLidObserverPtr observer) override;
  void AddPowerObserver(mojom::CrosHealthdPowerObserverPtr observer) override;

  // CrosHealthdProbeService overrides:
  void ProbeTelemetryInfo(
      const std::vector<mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;

  void ProbeProcessInfo(const uint32_t process_id,
                        ProbeProcessInfoCallback callback) override;

  // Set the list of routines that will be used in the response to any
  // GetAvailableRoutines IPCs received.
  void SetAvailableRoutinesForTesting(
      const std::vector<mojom::DiagnosticRoutineEnum>& available_routines);

  // Set the RunRoutine response that will be used in the response to any
  // RunSomeRoutine IPCs received.
  void SetRunRoutineResponseForTesting(mojom::RunRoutineResponsePtr& response);

  // Set the GetRoutineUpdate response that will be used in the response to any
  // GetRoutineUpdate IPCs received.
  void SetGetRoutineUpdateResponseForTesting(mojom::RoutineUpdatePtr& response);

  // Set the TelemetryInfoPtr that will be used in the response to any
  // ProbeTelemetryInfo IPCs received.
  void SetProbeTelemetryInfoResponseForTesting(
      mojom::TelemetryInfoPtr& response_info);

  // Set the ProcessResultPtr that will be used in the response to any
  // ProbeProcessInfo IPCs received.
  void SetProbeProcessInfoResponseForTesting(mojom::ProcessResultPtr& result);

  // Adds a delay before the passed callback is called.
  void SetCallbackDelay(base::TimeDelta delay);

  // Calls the power event OnAcInserted for all registered power observers.
  void EmitAcInsertedEventForTesting();

  // Calls the Bluetooth event OnAdapterAdded for all registered Bluetooth
  // observers.
  void EmitAdapterAddedEventForTesting();

  // Calls the lid event OnLidClosed for all registered lid observers.
  void EmitLidClosedEventForTesting();

  // Calls the lid event OnLidOpened for all registered lid observers.
  void EmitLidOpenedEventForTesting();

  // Requests the network health state using the network_health_remote_.
  void RequestNetworkHealthForTesting(
      chromeos::network_health::mojom::NetworkHealthService::
          GetHealthSnapshotCallback callback);

  // Calls the LanConnectivity routine on |network_diagnostics_routines_|.
  void RunLanConnectivityRoutineForTesting(
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
          LanConnectivityCallback callback);

 private:
  // Used as the response to any GetAvailableRoutines IPCs received.
  std::vector<mojom::DiagnosticRoutineEnum> available_routines_;
  // Used as the response to any RunSomeRoutine IPCs received.
  mojom::RunRoutineResponsePtr run_routine_response_{
      mojom::RunRoutineResponse::New()};
  // Used as the response to any GetRoutineUpdate IPCs received.
  mojom::RoutineUpdatePtr routine_update_response_{mojom::RoutineUpdate::New()};
  // Used as the response to any ProbeTelemetryInfo IPCs received.
  mojom::TelemetryInfoPtr telemetry_response_info_{mojom::TelemetryInfo::New()};
  // Used as the response to any ProbeProcessInfo IPCs received.
  mojom::ProcessResultPtr process_response_{
      mojom::ProcessResult::NewProcessInfo(mojom::ProcessInfo::New())};

  // Allows the remote end to call the probe, diagnostics and event service
  // methods.
  mojo::ReceiverSet<mojom::CrosHealthdProbeService> probe_receiver_set_;
  mojo::ReceiverSet<mojom::CrosHealthdDiagnosticsService>
      diagnostics_receiver_set_;
  mojo::ReceiverSet<mojom::CrosHealthdEventService> event_receiver_set_;

  // NetworkHealthService remote.
  mojo::Remote<chromeos::network_health::mojom::NetworkHealthService>
      network_health_remote_;

  // Collection of registered Bluetooth observers.
  mojo::RemoteSet<mojom::CrosHealthdBluetoothObserver> bluetooth_observers_;
  // Collection of registered lid observers.
  mojo::RemoteSet<mojom::CrosHealthdLidObserver> lid_observers_;
  // Collection of registered power observers.
  mojo::RemoteSet<mojom::CrosHealthdPowerObserver> power_observers_;

  // Allow |this| to call the methods on the NetworkDiagnosticsRoutines
  // interface.
  mojo::Remote<chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      network_diagnostics_routines_;

  base::TimeDelta callback_delay_;

  DISALLOW_COPY_AND_ASSIGN(FakeCrosHealthdService);
};

}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_SERVICE_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_CROS_HEALTHD_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_CROS_HEALTHD_H_

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::cros_healthd {

// This class serves as a fake for all four of cros_healthd's mojo interfaces.
// The factory methods bind to receivers held within FakeCrosHealtdService, and
// all requests on each of the interfaces are fulfilled by
// FakeCrosHealthd.
class FakeCrosHealthd final
    : public chromeos::cros_healthd::mojom::CrosHealthdServiceFactory,
      public chromeos::cros_healthd::mojom::CrosHealthdDiagnosticsService,
      public chromeos::cros_healthd::mojom::CrosHealthdEventService,
      public chromeos::cros_healthd::mojom::CrosHealthdProbeService,
      public chromeos::cros_healthd::mojom::CrosHealthdSystemService {
 public:
  // Stores the params passed to `GetRoutineUpdate`.
  struct RoutineUpdateParams {
    RoutineUpdateParams(
        int32_t id,
        chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum command,
        bool include_output);

    int32_t id;
    chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum command;
    bool include_output;
  };

  FakeCrosHealthd(const FakeCrosHealthd&) = delete;
  FakeCrosHealthd& operator=(const FakeCrosHealthd&) = delete;

  // Initializes a global instance. This register a fake mojo service for
  // testing. Don't need to call this in browser test because ServiceConnection
  // will initialize this in browser test.
  static void Initialize();

  // Shutdowns the global instance. This also shutdown the CrosHealthdClient
  // (the dbus client). In browser test this will not be called.
  static void Shutdown();

  // Gets the global instance. A `nullptr` could be returned if it is not
  // initialized.
  static FakeCrosHealthd* Get();

  // Set the list of routines that will be used in the response to any
  // GetAvailableRoutines IPCs received.
  void SetAvailableRoutinesForTesting(
      const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>&
          available_routines);

  // Set the RunRoutine response that will be used in the response to any
  // RunSomeRoutine IPCs received.
  void SetRunRoutineResponseForTesting(
      chromeos::cros_healthd::mojom::RunRoutineResponsePtr& response);

  // Set the GetRoutineUpdate response that will be used in the response to any
  // GetRoutineUpdate IPCs received.
  void SetGetRoutineUpdateResponseForTesting(
      chromeos::cros_healthd::mojom::RoutineUpdatePtr& response);

  // Set the TelemetryInfoPtr that will be used in the response to any
  // ProbeTelemetryInfo IPCs received.
  void SetProbeTelemetryInfoResponseForTesting(
      chromeos::cros_healthd::mojom::TelemetryInfoPtr& response_info);

  // Set the ProcessResultPtr that will be used in the response to any
  // ProbeProcessInfo IPCs received.
  void SetProbeProcessInfoResponseForTesting(
      chromeos::cros_healthd::mojom::ProcessResultPtr& result);

  // Adds a delay before the passed callback is called.
  void SetCallbackDelay(base::TimeDelta delay);

  // Calls the power event OnAcInserted for all registered power observers.
  void EmitAcInsertedEventForTesting();

  // Calls the power event OnAcRemoved on all registered power observers.
  void EmitAcRemovedEventForTesting();

  // Calls the power event OnOsSuspend on all registered power observers.
  void EmitOsSuspendEventForTesting();

  // Calls the power event OnOsResume on all registered power observers.
  void EmitOsResumeEventForTesting();

  // Calls the Bluetooth event OnAdapterAdded for all registered Bluetooth
  // observers.
  void EmitAdapterAddedEventForTesting();

  // Calls the Bluetooth event OnAdapterRemoved on all registered Bluetooth
  // observers.
  void EmitAdapterRemovedEventForTesting();

  // Calls the Bluetooth event OnAdapterPropertyChanged on all registered
  // Bluetooth observers.
  void EmitAdapterPropertyChangedEventForTesting();

  // Calls the Bluetooth event OnDeviceAdded on all registered Bluetooth
  // observers.
  void EmitDeviceAddedEventForTesting();

  // Calls the Bluetooth event OnDeviceRemoved on all registered Bluetooth
  // observers.
  void EmitDeviceRemovedEventForTesting();

  // Calls the Bluetooth event OnDevicePropertyChanged on all registered
  // Bluetooth observers.
  void EmitDevicePropertyChangedEventForTesting();

  // Calls the lid event OnLidClosed for all registered lid observers.
  void EmitLidClosedEventForTesting();

  // Calls the lid event OnLidOpened for all registered lid observers.
  void EmitLidOpenedEventForTesting();

  // Calls the audio event OnUnderrun for all registered audio observers.
  void EmitAudioUnderrunEventForTesting();

  // Calls the audio event OnSevereUnderrun for all registered audio observers.
  void EmitAudioSevereUnderrunEventForTesting();

  // Calls the Thunderbolt event OnAdd on all registered Thunderbolt observers.
  void EmitThunderboltAddEventForTesting();

  // Calls the USB event OnAdd on all registered USB observers.
  void EmitUsbAddEventForTesting();

  // Calls the network event OnConnectionStateChangedEvent on all registered
  // network observers.
  void EmitConnectionStateChangedEventForTesting(
      const std::string& network_guid,
      chromeos::network_health::mojom::NetworkState state);

  // Calls the network event OnSignalStrengthChangedEvent on all registered
  // network observers.
  void EmitSignalStrengthChangedEventForTesting(
      const std::string& network_guid,
      chromeos::network_health::mojom::UInt32ValuePtr signal_strength);

  // Requests the network health state using the network_health_remote_.
  void RequestNetworkHealthForTesting(
      chromeos::network_health::mojom::NetworkHealthService::
          GetHealthSnapshotCallback callback);

  // Calls the LanConnectivity routine on |network_diagnostics_routines_|.
  void RunLanConnectivityRoutineForTesting(
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
          RunLanConnectivityCallback callback);

  // Returns the last created routine by any Run*Routine method.
  absl::optional<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>
  GetLastRunRoutine() const;

  // Returns the parameters passed for the most recent call to
  // `GetRoutineUpdate`.
  absl::optional<RoutineUpdateParams> GetRoutineUpdateParams() const;

 private:
  FakeCrosHealthd();
  ~FakeCrosHealthd() override;

  // Binds a new mojo remote and disconnected the old one if exists.
  mojo::Remote<chromeos::cros_healthd::mojom::CrosHealthdServiceFactory>
  BindNewRemote();

  // CrosHealthdServiceFactory overrides:
  void GetProbeService(mojo::PendingReceiver<
                       chromeos::cros_healthd::mojom::CrosHealthdProbeService>
                           service) override;
  void GetDiagnosticsService(
      mojo::PendingReceiver<
          chromeos::cros_healthd::mojom::CrosHealthdDiagnosticsService> service)
      override;
  void GetEventService(mojo::PendingReceiver<
                       chromeos::cros_healthd::mojom::CrosHealthdEventService>
                           service) override;
  void SendNetworkHealthService(
      mojo::PendingRemote<chromeos::network_health::mojom::NetworkHealthService>
          remote) override;
  void SendNetworkDiagnosticsRoutines(
      mojo::PendingRemote<
          chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
          network_diagnostics_routines) override;
  void GetSystemService(mojo::PendingReceiver<
                        chromeos::cros_healthd::mojom::CrosHealthdSystemService>
                            service) override;

  // CrosHealthdDiagnosticsService overrides:
  void GetAvailableRoutines(GetAvailableRoutinesCallback callback) override;
  void GetRoutineUpdate(
      int32_t id,
      chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum command,
      bool include_output,
      GetRoutineUpdateCallback callback) override;
  void RunUrandomRoutine(
      chromeos::cros_healthd::mojom::NullableUint32Ptr length_seconds,
      RunUrandomRoutineCallback callback) override;
  void RunBatteryCapacityRoutine(
      RunBatteryCapacityRoutineCallback callback) override;
  void RunBatteryHealthRoutine(
      RunBatteryHealthRoutineCallback callback) override;
  void RunSmartctlCheckRoutine(
      RunSmartctlCheckRoutineCallback callback) override;
  void RunAcPowerRoutine(
      chromeos::cros_healthd::mojom::AcPowerStatusEnum expected_status,
      const absl::optional<std::string>& expected_power_type,
      RunAcPowerRoutineCallback callback) override;
  void RunCpuCacheRoutine(
      chromeos::cros_healthd::mojom::NullableUint32Ptr length_seconds,
      RunCpuCacheRoutineCallback callback) override;
  void RunCpuStressRoutine(
      chromeos::cros_healthd::mojom::NullableUint32Ptr length_seconds,
      RunCpuStressRoutineCallback callback) override;
  void RunFloatingPointAccuracyRoutine(
      chromeos::cros_healthd::mojom::NullableUint32Ptr length_seconds,
      RunFloatingPointAccuracyRoutineCallback callback) override;
  void RunNvmeWearLevelRoutine(
      uint32_t wear_level_threshold,
      RunNvmeWearLevelRoutineCallback callback) override;
  void RunNvmeSelfTestRoutine(
      chromeos::cros_healthd::mojom::NvmeSelfTestTypeEnum nvme_self_test_type,
      RunNvmeSelfTestRoutineCallback callback) override;
  void RunDiskReadRoutine(
      chromeos::cros_healthd::mojom::DiskReadRoutineTypeEnum type,
      uint32_t length_seconds,
      uint32_t file_size_mb,
      RunDiskReadRoutineCallback callback) override;
  void RunPrimeSearchRoutine(
      chromeos::cros_healthd::mojom::NullableUint32Ptr length_seconds,
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
  void RunGatewayCanBePingedRoutine(
      RunGatewayCanBePingedRoutineCallback callback) override;
  void RunHasSecureWiFiConnectionRoutine(
      RunHasSecureWiFiConnectionRoutineCallback callback) override;
  void RunDnsResolverPresentRoutine(
      RunDnsResolverPresentRoutineCallback callback) override;
  void RunDnsLatencyRoutine(RunDnsLatencyRoutineCallback callback) override;
  void RunDnsResolutionRoutine(
      RunDnsResolutionRoutineCallback callback) override;
  void RunCaptivePortalRoutine(
      RunCaptivePortalRoutineCallback callback) override;
  void RunHttpFirewallRoutine(RunHttpFirewallRoutineCallback callback) override;
  void RunHttpsFirewallRoutine(
      RunHttpsFirewallRoutineCallback callback) override;
  void RunHttpsLatencyRoutine(RunHttpsLatencyRoutineCallback callback) override;
  void RunVideoConferencingRoutine(
      const absl::optional<std::string>& stun_server_hostname,
      RunVideoConferencingRoutineCallback callback) override;
  void RunArcHttpRoutine(RunArcHttpRoutineCallback callback) override;
  void RunArcPingRoutine(RunArcPingRoutineCallback callback) override;
  void RunArcDnsResolutionRoutine(
      RunArcDnsResolutionRoutineCallback callback) override;

  // CrosHealthdEventService overrides:
  void AddBluetoothObserver(
      mojo::PendingRemote<
          chromeos::cros_healthd::mojom::CrosHealthdBluetoothObserver> observer)
      override;
  void AddLidObserver(
      mojo::PendingRemote<chromeos::cros_healthd::mojom::CrosHealthdLidObserver>
          observer) override;
  void AddPowerObserver(mojo::PendingRemote<
                        chromeos::cros_healthd::mojom::CrosHealthdPowerObserver>
                            observer) override;
  void AddNetworkObserver(
      mojo::PendingRemote<
          chromeos::network_health::mojom::NetworkEventsObserver> observer)
      override;
  void AddAudioObserver(mojo::PendingRemote<
                        chromeos::cros_healthd::mojom::CrosHealthdAudioObserver>
                            observer) override;
  void AddThunderboltObserver(
      mojo::PendingRemote<
          chromeos::cros_healthd::mojom::CrosHealthdThunderboltObserver>
          observer) override;
  void AddUsbObserver(
      mojo::PendingRemote<chromeos::cros_healthd::mojom::CrosHealthdUsbObserver>
          observer) override;

  // CrosHealthdProbeService overrides:
  void ProbeTelemetryInfo(
      const std::vector<chromeos::cros_healthd::mojom::ProbeCategoryEnum>&
          categories,
      ProbeTelemetryInfoCallback callback) override;
  void ProbeProcessInfo(const uint32_t process_id,
                        ProbeProcessInfoCallback callback) override;

  // CrosHealthdSystemService overrides:
  void GetServiceStatus(GetServiceStatusCallback callback) override;

  // Used to simulate the bootstrap of healthd mojo interface.
  mojo::Receiver<chromeos::cros_healthd::mojom::CrosHealthdServiceFactory>
      healthd_receiver_{this};

  // Used as the response to any GetAvailableRoutines IPCs received.
  std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>
      available_routines_;
  // Used to store last created routine by any Run*Routine method.
  absl::optional<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>
      last_run_routine_;
  // Used as the response to any RunSomeRoutine IPCs received.
  chromeos::cros_healthd::mojom::RunRoutineResponsePtr run_routine_response_{
      chromeos::cros_healthd::mojom::RunRoutineResponse::New()};
  // Used as the response to any GetRoutineUpdate IPCs received.
  chromeos::cros_healthd::mojom::RoutineUpdatePtr routine_update_response_{
      chromeos::cros_healthd::mojom::RoutineUpdate::New()};
  // Used as the response to any ProbeTelemetryInfo IPCs received.
  chromeos::cros_healthd::mojom::TelemetryInfoPtr telemetry_response_info_{
      chromeos::cros_healthd::mojom::TelemetryInfo::New()};
  // Used as the response to any ProbeProcessInfo IPCs received.
  chromeos::cros_healthd::mojom::ProcessResultPtr process_response_{
      chromeos::cros_healthd::mojom::ProcessResult::NewProcessInfo(
          chromeos::cros_healthd::mojom::ProcessInfo::New())};

  // Allows the remote end to call the probe, diagnostics and event service
  // methods.
  mojo::ReceiverSet<chromeos::cros_healthd::mojom::CrosHealthdProbeService>
      probe_receiver_set_;
  mojo::ReceiverSet<
      chromeos::cros_healthd::mojom::CrosHealthdDiagnosticsService>
      diagnostics_receiver_set_;
  mojo::ReceiverSet<chromeos::cros_healthd::mojom::CrosHealthdEventService>
      event_receiver_set_;
  mojo::ReceiverSet<chromeos::cros_healthd::mojom::CrosHealthdSystemService>
      system_receiver_set_;

  // NetworkHealthService remote.
  mojo::Remote<chromeos::network_health::mojom::NetworkHealthService>
      network_health_remote_;

  // Collection of registered Bluetooth observers.
  mojo::RemoteSet<chromeos::cros_healthd::mojom::CrosHealthdBluetoothObserver>
      bluetooth_observers_;
  // Collection of registered lid observers.
  mojo::RemoteSet<chromeos::cros_healthd::mojom::CrosHealthdLidObserver>
      lid_observers_;
  // Collection of registered power observers.
  mojo::RemoteSet<chromeos::cros_healthd::mojom::CrosHealthdPowerObserver>
      power_observers_;
  // Collection of registered network observers.
  mojo::RemoteSet<chromeos::network_health::mojom::NetworkEventsObserver>
      network_observers_;
  // Collection of registered audio observers.
  mojo::RemoteSet<chromeos::cros_healthd::mojom::CrosHealthdAudioObserver>
      audio_observers_;
  // Collection of registered Thunderbolt observers.
  mojo::RemoteSet<chromeos::cros_healthd::mojom::CrosHealthdThunderboltObserver>
      thunderbolt_observers_;
  // Collection of registered USB observers.
  mojo::RemoteSet<chromeos::cros_healthd::mojom::CrosHealthdUsbObserver>
      usb_observers_;

  // Contains the most recent params passed to `GetRoutineUpdate`, if it has
  // been called.
  absl::optional<RoutineUpdateParams> routine_update_params_;

  // Allow |this| to call the methods on the NetworkDiagnosticsRoutines
  // interface.
  mojo::Remote<chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines>
      network_diagnostics_routines_;

  base::TimeDelta callback_delay_;
};

}  // namespace ash::cros_healthd

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos::cros_healthd {
using ::ash::cros_healthd::FakeCrosHealthd;
}

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_PUBLIC_CPP_FAKE_CROS_HEALTHD_H_

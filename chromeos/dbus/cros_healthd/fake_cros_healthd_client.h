// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
#define CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/time/time.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace cros_healthd {

// Fake implementation of CrosHealthdClient.
class COMPONENT_EXPORT(CROS_HEALTHD) FakeCrosHealthdClient
    : public CrosHealthdClient {
 public:
  // FakeCrosHealthdClient can be embedded in unit tests, but the
  // InitializeFake/Shutdown pattern should be preferred. Constructing the
  // instance will set the global instance for the fake and for the base class,
  // so the static Get() accessor can be used with that pattern.
  FakeCrosHealthdClient();

  FakeCrosHealthdClient(const FakeCrosHealthdClient&) = delete;
  FakeCrosHealthdClient& operator=(const FakeCrosHealthdClient&) = delete;

  ~FakeCrosHealthdClient() override;

  // Checks that a FakeCrosHealthdClient instance was initialized and returns
  // it.
  static FakeCrosHealthdClient* Get();

  // CrosHealthdClient overrides:
  mojo::Remote<mojom::CrosHealthdServiceFactory> BootstrapMojoConnection(
      BootstrapMojoConnectionCallback result_callback) override;

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
  void SetProbeTelemetryInfoResponseForTesting(mojom::TelemetryInfoPtr& info);

  // Set the ProcessResultPtr that will be used in the response to any
  // ProbeProcessInfo IPCs received.
  void SetProbeProcessInfoResponseForTesting(mojom::ProcessResultPtr& result);

  // Adds a delay before the passed callback is called.
  void SetCallbackDelay(base::TimeDelta delay);

  // Calls the power event OnAcInserted on all registered power observers.
  void EmitAcInsertedEventForTesting();

  // Calls the power event OnAcRemoved on all registered power observers.
  void EmitAcRemovedEventForTesting();

  // Calls the power event OnOsSuspend on all registered power observers.
  void EmitOsSuspendEventForTesting();

  // Calls the power event OnOsResume on all registered power observers.
  void EmitOsResumeEventForTesting();

  // Calls the Bluetooth event OnAdapterAdded on all registered Bluetooth
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

  // Calls the lid event OnLidClosed on all registered lid observers.
  void EmitLidClosedEventForTesting();

  // Calls the lid event OnLidOpened on all registered lid observers.
  void EmitLidOpenedEventForTesting();

  // Calls the audio event OnUnderrun on all registered audio observers.
  void EmitAudioUnderrunEventForTesting();

  // Calls the audio event OnSevereUnderrun on all registered audio observers.
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

  // Requests the network health state using the NetworkHealthService remote.
  void RequestNetworkHealthForTesting(
      chromeos::network_health::mojom::NetworkHealthService::
          GetHealthSnapshotCallback callback);

  // Calls the LanConnectivity routine using the NetworkDiagnosticsRoutines
  // remote.
  void RunLanConnectivityRoutineForTesting(
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
          RunLanConnectivityCallback);

  // Returns the last created routine by any Run*Routine method.
  absl::optional<mojom::DiagnosticRoutineEnum> GetLastRunRoutine() const;

  // Returns the parameters passed for the most recent call to
  // `GetRoutineUpdate`.
  absl::optional<FakeCrosHealthdService::RoutineUpdateParams>
  GetRoutineUpdateParams();

 private:
  FakeCrosHealthdService fake_service_;
  mojo::Receiver<mojom::CrosHealthdServiceFactory> receiver_{&fake_service_};
};

}  // namespace cros_healthd
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace cros_healthd {
using ::chromeos::cros_healthd::FakeCrosHealthdClient;
}  // namespace cros_healthd
}  // namespace ash

#endif  // CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_

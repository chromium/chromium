// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_
#define CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_service.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

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

  // Calls the Bluetooth event OnAdapterAdded on all registered Bluetooth
  // observers.
  void EmitAdapterAddedEventForTesting();

  // Calls the lid event OnLidClosed on all registered lid observers.
  void EmitLidClosedEventForTesting();

  // Calls the lid event OnLidOpened on all registered lid observers.
  void EmitLidOpenedEventForTesting();

  // Requests the network health state using the NetworkHealthService remote.
  void RequestNetworkHealthForTesting(
      chromeos::network_health::mojom::NetworkHealthService::
          GetHealthSnapshotCallback callback);

  // Calls the LanConnectivity routine using the NetworkDiagnosticsRoutines
  // remote.
  void RunLanConnectivityRoutineForTesting(
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
          LanConnectivityCallback);

 private:
  FakeCrosHealthdService fake_service_;
  mojo::Receiver<mojom::CrosHealthdServiceFactory> receiver_{&fake_service_};

  DISALLOW_COPY_AND_ASSIGN(FakeCrosHealthdClient);
};

}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CROS_HEALTHD_FAKE_CROS_HEALTHD_CLIENT_H_

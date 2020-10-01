// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"

#include "base/callback.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"

namespace chromeos {
namespace cros_healthd {

namespace {

// Used to track the fake instance, mirrors the instance in the base class.
FakeCrosHealthdClient* g_instance = nullptr;

}  // namespace

FakeCrosHealthdClient::FakeCrosHealthdClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeCrosHealthdClient::~FakeCrosHealthdClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeCrosHealthdClient* FakeCrosHealthdClient::Get() {
  return g_instance;
}

mojo::Remote<mojom::CrosHealthdServiceFactory>
FakeCrosHealthdClient::BootstrapMojoConnection(
    BootstrapMojoConnectionCallback result_callback) {
  mojo::Remote<mojom::CrosHealthdServiceFactory> remote(
      receiver_.BindNewPipeAndPassRemote());

  std::move(result_callback).Run(/*success=*/true);
  return remote;
}

void FakeCrosHealthdClient::SetAvailableRoutinesForTesting(
    const std::vector<mojom::DiagnosticRoutineEnum>& available_routines) {
  fake_service_.SetAvailableRoutinesForTesting(available_routines);
}

void FakeCrosHealthdClient::SetRunRoutineResponseForTesting(
    mojom::RunRoutineResponsePtr& response) {
  fake_service_.SetRunRoutineResponseForTesting(response);
}

void FakeCrosHealthdClient::SetGetRoutineUpdateResponseForTesting(
    mojom::RoutineUpdatePtr& response) {
  fake_service_.SetGetRoutineUpdateResponseForTesting(response);
}

void FakeCrosHealthdClient::SetProbeTelemetryInfoResponseForTesting(
    mojom::TelemetryInfoPtr& info) {
  fake_service_.SetProbeTelemetryInfoResponseForTesting(info);
}

void FakeCrosHealthdClient::SetProbeProcessInfoResponseForTesting(
    mojom::ProcessResultPtr& result) {
  fake_service_.SetProbeProcessInfoResponseForTesting(result);
}

void FakeCrosHealthdClient::SetCallbackDelay(base::TimeDelta delay) {
  fake_service_.SetCallbackDelay(delay);
}

void FakeCrosHealthdClient::EmitAcInsertedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  receiver_.FlushForTesting();
  fake_service_.EmitAcInsertedEventForTesting();
}

void FakeCrosHealthdClient::EmitAdapterAddedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  receiver_.FlushForTesting();
  fake_service_.EmitAdapterAddedEventForTesting();
}

void FakeCrosHealthdClient::EmitLidClosedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  receiver_.FlushForTesting();
  fake_service_.EmitLidClosedEventForTesting();
}

void FakeCrosHealthdClient::EmitLidOpenedEventForTesting() {
  // Flush the receiver, so any pending observers are registered before the
  // event is emitted.
  receiver_.FlushForTesting();
  fake_service_.EmitLidOpenedEventForTesting();
}

void FakeCrosHealthdClient::RequestNetworkHealthForTesting(
    chromeos::network_health::mojom::NetworkHealthService::
        GetHealthSnapshotCallback callback) {
  // Flush the receiver, so any requests to send the NetworkHealthService remote
  // are processed before the request is emitted.
  receiver_.FlushForTesting();
  fake_service_.RequestNetworkHealthForTesting(std::move(callback));
}

void FakeCrosHealthdClient::RunLanConnectivityRoutineForTesting(
    chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines::
        LanConnectivityCallback callback) {
  // Flush the receiver, so the NetworkDiagnosticsRoutines interface is
  // registered before routines are called on it.
  receiver_.FlushForTesting();
  fake_service_.RunLanConnectivityRoutineForTesting(std::move(callback));
}

}  // namespace cros_healthd
}  // namespace chromeos

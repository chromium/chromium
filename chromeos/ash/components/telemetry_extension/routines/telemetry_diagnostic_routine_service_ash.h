// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/telemetry_extension/common/self_owned_mojo_proxy.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

// Implementation of the `TelemetryDiagnosticRoutinesService`, allows for
// creating new routines on the platform as well as interaction with existing
// routines and requesting information about the `SupportStatus` of routines.
class TelemetryDiagnosticsRoutineServiceAsh
    : public crosapi::mojom::TelemetryDiagnosticRoutinesService {
 public:
  // Factory for creating instances of `TelemetryDiagnosticsRoutineServiceAsh`.
  // Provides a method for setting a test instance.
  class Factory {
   public:
    static std::unique_ptr<crosapi::mojom::TelemetryDiagnosticRoutinesService>
    Create(mojo::PendingReceiver<
           crosapi::mojom::TelemetryDiagnosticRoutinesService> receiver);

    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<crosapi::mojom::TelemetryDiagnosticRoutinesService>
    CreateInstance(
        mojo::PendingReceiver<
            crosapi::mojom::TelemetryDiagnosticRoutinesService> receiver) = 0;

   private:
    static Factory* test_factory_;
  };

  TelemetryDiagnosticsRoutineServiceAsh();
  TelemetryDiagnosticsRoutineServiceAsh(
      const TelemetryDiagnosticsRoutineServiceAsh&) = delete;
  TelemetryDiagnosticsRoutineServiceAsh& operator=(
      const TelemetryDiagnosticsRoutineServiceAsh&) = delete;
  ~TelemetryDiagnosticsRoutineServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutinesService>
          receiver);

  // `TelemetryDiagnosticRoutinesService`:
  void CreateRoutine(
      crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutineControl>
          routine_receiver,
      mojo::PendingRemote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
          observer) override;
  void IsRoutineArgumentSupported(
      crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr arg,
      IsRoutineArgumentSupportedCallback callback) override;

 private:
  // Called when a routine controller or observer connection is closed. This
  // removes the controller / observer from our list.
  void OnConnectionClosed(
      base::WeakPtr<SelfOwnedMojoProxyInterface> closed_connection);

  // The routine controls and observers created for each running routine.
  std::set<base::WeakPtr<SelfOwnedMojoProxyInterface>,
           SelfOwnedMojoProxyInterfaceWeakPtrComparator>
      routine_controls_and_observers_;

  // Support any number of connections.
  mojo::ReceiverSet<crosapi::mojom::TelemetryDiagnosticRoutinesService>
      receivers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_TELEMETRY_DIAGNOSTIC_ROUTINE_SERVICE_ASH_H_

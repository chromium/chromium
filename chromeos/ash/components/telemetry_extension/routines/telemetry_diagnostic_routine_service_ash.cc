// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/telemetry_extension/common/self_owned_mojo_proxy.h"
#include "chromeos/ash/components/telemetry_extension/common/telemetry_extension_converters.h"
#include "chromeos/ash/components/telemetry_extension/routines/routine_control.h"
#include "chromeos/ash/components/telemetry_extension/routines/routine_converters.h"
#include "chromeos/ash/components/telemetry_extension/routines/routine_events_forwarder.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

namespace {

namespace crosapi = crosapi::mojom;
namespace healthd = cros_healthd::mojom;

using RoutineControlProxy =
    SelfOwnedMojoProxy<healthd::RoutineControl,
                       crosapi::TelemetryDiagnosticRoutineControl,
                       CrosHealthdRoutineControl>;
using RoutineObserverProxy =
    SelfOwnedMojoProxy<crosapi::TelemetryDiagnosticRoutineObserver,
                       healthd::RoutineObserver,
                       CrosHealthdRoutineEventsForwarder>;

}  // namespace

// static
TelemetryDiagnosticsRoutineServiceAsh::Factory*
    TelemetryDiagnosticsRoutineServiceAsh::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<crosapi::TelemetryDiagnosticRoutinesService>
TelemetryDiagnosticsRoutineServiceAsh::Factory::Create(
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutinesService>
        receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  auto routine_service =
      std::make_unique<TelemetryDiagnosticsRoutineServiceAsh>();
  routine_service->BindReceiver(std::move(receiver));
  return routine_service;
}

// static
void TelemetryDiagnosticsRoutineServiceAsh::Factory::SetForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

TelemetryDiagnosticsRoutineServiceAsh::Factory::~Factory() = default;

TelemetryDiagnosticsRoutineServiceAsh::TelemetryDiagnosticsRoutineServiceAsh() =
    default;

TelemetryDiagnosticsRoutineServiceAsh::
    ~TelemetryDiagnosticsRoutineServiceAsh() {
  for (auto&& proxy : routine_controls_and_observers_) {
    if (proxy) {
      proxy->OnServiceDestroyed();
    }
  }
  routine_controls_and_observers_.clear();
}

void TelemetryDiagnosticsRoutineServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutinesService>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TelemetryDiagnosticsRoutineServiceAsh::CreateRoutine(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr routine_argument,
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineControl>
        routine_receiver,
    mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineObserver> observer) {
  // Setup the RoutineControl.
  mojo::PendingRemote<healthd::RoutineControl> cros_healthd_remote;
  auto cros_healthd_receiver =
      cros_healthd_remote.InitWithNewPipeAndPassReceiver();

  // SAFETY: We can use `base::Unretained` here since we signal the
  // `SelfOwnedMojoProxy` in the destructor.
  auto control_delete_cb =
      base::BindOnce(&TelemetryDiagnosticsRoutineServiceAsh::OnConnectionClosed,
                     base::Unretained(this));
  auto routine_control = RoutineControlProxy::Create(
      std::move(routine_receiver), std::move(cros_healthd_remote),
      std::move(control_delete_cb));
  routine_controls_and_observers_.insert(std::move(routine_control));

  // Setup the RoutineObserver.
  mojo::PendingRemote<healthd::RoutineObserver> cros_healthd_observer;
  if (observer.is_valid()) {
    // SAFETY: We can use `base::Unretained` here since we signal the
    // `SelfOwnedMojoProxy` in the destructor.
    auto observer_delete_cb = base::BindOnce(
        &TelemetryDiagnosticsRoutineServiceAsh::OnConnectionClosed,
        base::Unretained(this));
    auto routine_observer = RoutineObserverProxy::Create(
        cros_healthd_observer.InitWithNewPipeAndPassReceiver(),
        std::move(observer), std::move(observer_delete_cb));
    routine_controls_and_observers_.insert(std::move(routine_observer));
  }

  // Register the two objects with cros_healthd.
  cros_healthd::ServiceConnection::GetInstance()
      ->GetRoutinesService()
      ->CreateRoutine(
          converters::ConvertRoutinePtr(std::move(routine_argument)),
          std::move(cros_healthd_receiver), std::move(cros_healthd_observer));
}

void TelemetryDiagnosticsRoutineServiceAsh::IsRoutineArgumentSupported(
    crosapi::TelemetryDiagnosticRoutineArgumentPtr arg,
    IsRoutineArgumentSupportedCallback callback) {
  cros_healthd::ServiceConnection::GetInstance()
      ->GetRoutinesService()
      ->IsRoutineArgumentSupported(
          converters::ConvertRoutinePtr(std::move(arg)),
          base::BindOnce(
              [](IsRoutineArgumentSupportedCallback callback,
                 healthd::SupportStatusPtr status) {
                std::move(callback).Run(
                    converters::ConvertCommonPtr(std::move(status)));
              },
              std::move(callback)));
}

void TelemetryDiagnosticsRoutineServiceAsh::OnConnectionClosed(
    base::WeakPtr<SelfOwnedMojoProxyInterface> closed_connection) {
  routine_controls_and_observers_.erase(closed_connection);
}

}  // namespace ash

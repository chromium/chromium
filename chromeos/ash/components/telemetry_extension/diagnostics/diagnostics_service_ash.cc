// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/nullable_primitives.mojom.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "chromeos/crosapi/mojom/nullable_primitives.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// static
DiagnosticsServiceAsh::Factory* DiagnosticsServiceAsh::Factory::test_factory_ =
    nullptr;

// static
std::unique_ptr<crosapi::mojom::DiagnosticsService>
DiagnosticsServiceAsh::Factory::Create(
    mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  auto diagnostics_service = std::make_unique<DiagnosticsServiceAsh>();
  diagnostics_service->BindReceiver(std::move(receiver));
  return diagnostics_service;
}

// static
void DiagnosticsServiceAsh::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

DiagnosticsServiceAsh::Factory::~Factory() = default;

DiagnosticsServiceAsh::DiagnosticsServiceAsh() = default;

DiagnosticsServiceAsh::~DiagnosticsServiceAsh() = default;

void DiagnosticsServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

const mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService>&
DiagnosticsServiceAsh::GetService() {
  if (!service_ || !service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindDiagnosticsService(
        service_.BindNewPipeAndPassReceiver());
    service_.set_disconnect_handler(base::BindOnce(
        &DiagnosticsServiceAsh::OnDisconnect, base::Unretained(this)));
  }
  return service_;
}

void DiagnosticsServiceAsh::OnDisconnect() {
  service_.reset();
}

void DiagnosticsServiceAsh::GetRoutineUpdate(
    int32_t id,
    crosapi::mojom::DiagnosticsRoutineCommandEnum command,
    bool include_output,
    GetRoutineUpdateCallback callback) {
  GetService()->GetRoutineUpdate(
      id, converters::diagnostics::Convert(command), include_output,
      base::BindOnce(
          [](crosapi::mojom::DiagnosticsService::GetRoutineUpdateCallback
                 callback,
             cros_healthd::mojom::RoutineUpdatePtr ptr) {
            std::move(callback).Run(
                converters::diagnostics::ConvertDiagnosticsPtr(std::move(ptr)));
          },
          std::move(callback)));
}

}  // namespace ash

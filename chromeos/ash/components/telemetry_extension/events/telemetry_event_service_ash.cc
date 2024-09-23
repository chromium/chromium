// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/telemetry_extension/common/telemetry_extension_converters.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_forwarder.h"
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_converters.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

namespace {

using EventObserverProxy =
    SelfOwnedMojoProxy<crosapi::mojom::TelemetryEventObserver,
                       cros_healthd::mojom::EventObserver,
                       CrosHealthdEventForwarder>;

}  // namespace

// static
TelemetryEventServiceAsh::Factory*
    TelemetryEventServiceAsh::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<crosapi::mojom::TelemetryEventService>
TelemetryEventServiceAsh::Factory::Create(
    mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  auto event_service = std::make_unique<TelemetryEventServiceAsh>();
  event_service->BindReceiver(std::move(receiver));
  return event_service;
}

// static
void TelemetryEventServiceAsh::Factory::SetForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

TelemetryEventServiceAsh::Factory::~Factory() = default;

TelemetryEventServiceAsh::TelemetryEventServiceAsh() = default;

TelemetryEventServiceAsh::~TelemetryEventServiceAsh() = default;

void TelemetryEventServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::TelemetryEventService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TelemetryEventServiceAsh::AddEventObserver(
    crosapi::mojom::TelemetryEventCategoryEnum category,
    mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> observer) {
  auto cb = base::BindOnce(&TelemetryEventServiceAsh::OnConnectionClosed,
                           weak_factory_.GetWeakPtr());
  mojo::PendingReceiver<cros_healthd::mojom::EventObserver> pending_receiver;
  cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->AddEventObserver(converters::events::Convert(category),
                         pending_receiver.InitWithNewPipeAndPassRemote());

  observers_.insert(EventObserverProxy::Create(std::move(pending_receiver),
                                               std::move(observer),
                                               std::move(cb), category));
}

void TelemetryEventServiceAsh::IsEventSupported(
    crosapi::mojom::TelemetryEventCategoryEnum category,
    IsEventSupportedCallback callback) {
  cros_healthd::ServiceConnection::GetInstance()
      ->GetEventService()
      ->IsEventSupported(
          converters::events::Convert(category),
          base::BindOnce(
              [](IsEventSupportedCallback callback,
                 cros_healthd::mojom::SupportStatusPtr ptr) {
                std::move(callback).Run(
                    converters::ConvertCommonPtr(std::move(ptr)));
              },
              std::move(callback)));
}

void TelemetryEventServiceAsh::OnConnectionClosed(
    base::WeakPtr<SelfOwnedMojoProxyInterface> closed_connection) {
  observers_.erase(closed_connection);
}

}  // namespace ash

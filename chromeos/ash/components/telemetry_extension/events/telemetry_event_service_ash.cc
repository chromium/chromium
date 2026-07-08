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

TelemetryEventServiceAsh::TelemetryEventServiceAsh() = default;

TelemetryEventServiceAsh::~TelemetryEventServiceAsh() = default;

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

  observers_.push_back(EventObserverProxy::Create(std::move(pending_receiver),
                                                  std::move(observer),
                                                  std::move(cb), category));
}

void TelemetryEventServiceAsh::OnConnectionClosed(
    base::WeakPtr<SelfOwnedMojoProxyInterface> closed_connection) {
  std::erase_if(observers_, [&closed_connection](const auto& ptr) {
    return ptr.get() == closed_connection.get();
  });
}

}  // namespace ash

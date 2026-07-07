// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_ASH_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_ASH_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/telemetry_extension/common/self_owned_mojo_proxy.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

class TelemetryEventServiceAsh {
 public:
  using IsEventSupportedCallback = base::OnceCallback<void(
      crosapi::mojom::TelemetryExtensionSupportStatusPtr status)>;

  TelemetryEventServiceAsh();
  TelemetryEventServiceAsh(const TelemetryEventServiceAsh&) = delete;
  TelemetryEventServiceAsh& operator=(const TelemetryEventServiceAsh&) = delete;
  ~TelemetryEventServiceAsh();

  // Adds an observer to be notified on events. The caller can remove the
  // observer created by this call by closing their end of the message pipe.
  //
  // The request:
  // * |category| - Event category.
  // * |observer| - Event observer to be added to crosapi.
  void AddEventObserver(
      crosapi::mojom::TelemetryEventCategoryEnum category,
      mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver> observer);

  // Checks whether an event is supported. It does the same support status check
  // as in `AddEventObserver()` and returns immediately after the check.
  //
  // The request:
  // * |category| - Event category to check.
  //
  // The response:
  // * |status| - See the documentation of `TelemetryExtensionSupportStatus`.
  void IsEventSupported(crosapi::mojom::TelemetryEventCategoryEnum category,
                        IsEventSupportedCallback callback);

  // Called by a connection when it is reset from either side (crosapi or
  // cros_healthd). Unregisters the connection.
  void OnConnectionClosed(
      base::WeakPtr<SelfOwnedMojoProxyInterface> closed_connection);

 private:
  // Currently open connections.
  std::vector<base::WeakPtr<SelfOwnedMojoProxyInterface>> observers_;

  base::WeakPtrFactory<TelemetryEventServiceAsh> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_SERVICE_ASH_H_

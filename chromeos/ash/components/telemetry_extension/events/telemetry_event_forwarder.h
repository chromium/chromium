// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_FORWARDER_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_FORWARDER_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A class that handles an Event connection. For each subscription to an
// event category, and instance of this object is created. It handles the
// two necessary mojom connections for observing events:
// - The `mojo::Remote<crosapi::mojom::TelemetryEventObserver>` which holds
//   a connection with crosapi. As soon as an event is triggered from
//   cros_healthd, it should be forwarded to this remote.
// - The `mojo::Receiver<cros_healthd::mojom::EventObserver>`, which holds
//   a connection with cros_healthd. The `OnEvent` message is invoked whenever
//   cros_healthd monitors an event.
//
// The connection is "alive" while both connections with cros_healthd and
// crosapi are open. If one of them is closed, we also close the other open
// connection and this object can be deleted.
class CrosHealthdEventForwarder : public cros_healthd::mojom::EventObserver {
 public:
  explicit CrosHealthdEventForwarder(
      mojo::PendingRemote<crosapi::mojom::TelemetryEventObserver>
          crosapi_remote,
      crosapi::mojom::TelemetryEventCategoryEnum category);
  CrosHealthdEventForwarder(const CrosHealthdEventForwarder&) = delete;
  CrosHealthdEventForwarder& operator=(const CrosHealthdEventForwarder&) =
      delete;
  ~CrosHealthdEventForwarder() override;

  // cros_healthd::mojom::EventObserver:
  void OnEvent(cros_healthd::mojom::EventInfoPtr info) override;

  mojo::Remote<crosapi::mojom::TelemetryEventObserver>& GetRemote();

 private:
  const crosapi::mojom::TelemetryEventCategoryEnum category_;

  // The mojo remote that corresponds to a crosapi observer connection.
  mojo::Remote<crosapi::mojom::TelemetryEventObserver> remote_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_EVENTS_TELEMETRY_EVENT_FORWARDER_H_

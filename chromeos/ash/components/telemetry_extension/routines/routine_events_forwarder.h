// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_EVENTS_FORWARDER_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_EVENTS_FORWARDER_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A class that handles an Routine observer connection. For each subscription to
// a routine's lifecycle, and instance of this object is created. It handles the
// two necessary mojom connections for observing lifecycle events:
// - The `mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>`
// which holds
//   a connection with crosapi. As soon as an event is triggered from
//   cros_healthd, it should be forwarded to this remote.
// - The `mojo::Receiver<cros_healthd::mojom::RoutineObserver>`, which holds
//   a connection with cros_healthd. The `OnRoutineStateChange` method is
//   invoked whenever cros_healthd monitors an event.
//
// The connection is "alive" while both connections with cros_healthd and
// crosapi are open. If one of them is closed, we also close the other open
// connection and this object can be deleted.
class CrosHealthdRoutineEventsForwarder
    : public cros_healthd::mojom::RoutineObserver {
 public:
  explicit CrosHealthdRoutineEventsForwarder(
      mojo::PendingRemote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>
          observer);
  CrosHealthdRoutineEventsForwarder(const CrosHealthdRoutineEventsForwarder&) =
      delete;
  CrosHealthdRoutineEventsForwarder& operator=(
      const CrosHealthdRoutineEventsForwarder&) = delete;
  ~CrosHealthdRoutineEventsForwarder() override;

  // `RoutineObserver`:
  void OnRoutineStateChange(
      cros_healthd::mojom::RoutineStatePtr state) override;

  mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutineObserver>& GetRemote();

 private:
  mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutineObserver> remote_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_EVENTS_FORWARDER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/routines/routine_events_forwarder.h"

#include <utility>

#include "chromeos/ash/components/telemetry_extension/routines/routine_converters.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace {

namespace crosapi = crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

CrosHealthdRoutineEventsForwarder::CrosHealthdRoutineEventsForwarder(
    mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineObserver>
        pending_remote)
    : remote_(std::move(pending_remote)) {}

CrosHealthdRoutineEventsForwarder::~CrosHealthdRoutineEventsForwarder() =
    default;

mojo::Remote<crosapi::TelemetryDiagnosticRoutineObserver>&
CrosHealthdRoutineEventsForwarder::GetRemote() {
  return remote_;
}

void CrosHealthdRoutineEventsForwarder::OnRoutineStateChange(
    healthd::RoutineStatePtr state) {
  remote_->OnRoutineStateChange(
      converters::ConvertRoutinePtr(std::move(state)));
}

}  // namespace ash

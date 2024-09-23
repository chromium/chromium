// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONTROL_H_
#define CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONTROL_H_

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Implements the `TelemetryDiagnosticRoutineControl` interface and forwards all
// control calls to cros_healthd. For that reasons the class handles two mojom
// connections, one over crosapi and one over cros_healthd. If either of the
// connections get closed, the other connection is also invalidated and thus
// closed.
class CrosHealthdRoutineControl
    : public crosapi::mojom::TelemetryDiagnosticRoutineControl {
 public:
  explicit CrosHealthdRoutineControl(
      mojo::PendingRemote<cros_healthd::mojom::RoutineControl> pending_remote);
  CrosHealthdRoutineControl(const CrosHealthdRoutineControl&) = delete;
  CrosHealthdRoutineControl& operator=(const CrosHealthdRoutineControl&) =
      delete;
  ~CrosHealthdRoutineControl() override;

  // `TelemetryDiagnosticRoutineControl`:
  void GetState(GetStateCallback callback) override;
  void Start() override;
  void ReplyToInquiry(
      crosapi::mojom::TelemetryDiagnosticRoutineInquiryReplyPtr reply) override;

  mojo::Remote<cros_healthd::mojom::RoutineControl>& GetRemote();

 private:
  mojo::Remote<cros_healthd::mojom::RoutineControl> remote_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TELEMETRY_EXTENSION_ROUTINES_ROUTINE_CONTROL_H_

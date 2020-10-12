// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/system_routine_controller.h"

#include "base/bind.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace chromeos {
namespace diagnostics {

void SystemRoutineController::BindCrosHealthdDiagnosticsServiceIfNeccessary() {
  if (!diagnostics_service_ || !diagnostics_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->GetDiagnosticsService(
        diagnostics_service_.BindNewPipeAndPassReceiver());
    diagnostics_service_.set_disconnect_handler(base::BindOnce(
        &SystemRoutineController::OnDiagnosticsServiceDisconnected,
        base::Unretained(this)));
  }
}

void SystemRoutineController::OnDiagnosticsServiceDisconnected() {
  diagnostics_service_.reset();
}

}  // namespace diagnostics
}  // namespace chromeos

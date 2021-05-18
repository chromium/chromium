// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/diagnostics_manager.h"

#include "ash/constants/ash_features.h"
#include "chromeos/components/diagnostics_ui/backend/input_data_provider.h"
#include "chromeos/components/diagnostics_ui/backend/session_log_handler.h"
#include "chromeos/components/diagnostics_ui/backend/system_data_provider.h"
#include "chromeos/components/diagnostics_ui/backend/system_routine_controller.h"

namespace chromeos {
namespace diagnostics {

DiagnosticsManager::DiagnosticsManager(SessionLogHandler* session_log_handler)
    : system_data_provider_(std::make_unique<SystemDataProvider>(
          session_log_handler->GetTelemetryLog())),
      system_routine_controller_(std::make_unique<SystemRoutineController>(
          session_log_handler->GetRoutineLog())) {
  if (features::IsInputInDiagnosticsAppEnabled()) {
    input_data_provider_ = std::make_unique<InputDataProvider>();
  }
}

DiagnosticsManager::~DiagnosticsManager() = default;

SystemDataProvider* DiagnosticsManager::GetSystemDataProvider() const {
  return system_data_provider_.get();
}

SystemRoutineController* DiagnosticsManager::GetSystemRoutineController()
    const {
  return system_routine_controller_.get();
}

InputDataProvider* DiagnosticsManager::GetInputDataProvider() const {
  return input_data_provider_.get();
}

}  // namespace diagnostics
}  // namespace chromeos

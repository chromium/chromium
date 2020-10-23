// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/diagnostics_manager.h"

#include "chromeos/components/diagnostics_ui/backend/system_data_provider.h"
#include "chromeos/components/diagnostics_ui/backend/system_routine_controller.h"

namespace chromeos {
namespace diagnostics {

DiagnosticsManager::DiagnosticsManager()
    : system_data_provider_(std::make_unique<SystemDataProvider>()),
      system_routine_controller_(std::make_unique<SystemRoutineController>()) {}

DiagnosticsManager::~DiagnosticsManager() = default;

SystemDataProvider* DiagnosticsManager::GetSystemDataProvider() const {
  return system_data_provider_.get();
}

SystemRoutineController* DiagnosticsManager::GetSystemRoutineController()
    const {
  return system_routine_controller_.get();
}

}  // namespace diagnostics
}  // namespace chromeos

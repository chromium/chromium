// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_DIAGNOSTICS_MANAGER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_DIAGNOSTICS_MANAGER_H_

#include <memory>

namespace chromeos {
namespace diagnostics {

class SessionLogHandler;
class SystemDataProvider;
class SystemRoutineController;
class InputDataProvider;

// DiagnosticsManager is responsible for managing the lifetime of the services
// used by the Diagnostics SWA.
class DiagnosticsManager {
 public:
  explicit DiagnosticsManager(SessionLogHandler* session_log_handler);
  ~DiagnosticsManager();

  DiagnosticsManager(const DiagnosticsManager&) = delete;
  DiagnosticsManager& operator=(const DiagnosticsManager&) = delete;

  SystemDataProvider* GetSystemDataProvider() const;
  SystemRoutineController* GetSystemRoutineController() const;
  InputDataProvider* GetInputDataProvider() const;

 private:
  std::unique_ptr<SystemDataProvider> system_data_provider_;
  std::unique_ptr<SystemRoutineController> system_routine_controller_;
  std::unique_ptr<InputDataProvider> input_data_provider_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_DIAGNOSTICS_MANAGER_H_

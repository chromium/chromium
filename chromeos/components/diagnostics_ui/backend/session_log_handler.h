// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {
class WebContents;
}  // namespace content

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace diagnostics {

class TelemetryLog;
class RoutineLog;

class SessionLogHandler {
 public:
  using SelectFilePolicyCreator =
      base::RepeatingCallback<std::unique_ptr<ui::SelectFilePolicy>(
          content::WebContents*)>;
  explicit SessionLogHandler(
      const SelectFilePolicyCreator& select_file_policy_creator);
  ~SessionLogHandler();

  SessionLogHandler(const SessionLogHandler&) = delete;
  SessionLogHandler& operator=(const SessionLogHandler&) = delete;

  TelemetryLog* GetTelemetryLog() const;
  RoutineLog* GetRoutineLog() const;

 private:
  // Creates a session log at `file_path`. The session log includes the contents
  // of both `telemetry_log_` and `routine_log_`. Returns true if the file was
  // successfully written. Retrns false otherwise.
  bool CreateSessionLog(const base::FilePath& file_path);

  SelectFilePolicyCreator select_file_policy_creator_;
  std::unique_ptr<TelemetryLog> telemetry_log_;
  std::unique_ptr<RoutineLog> routine_log_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_

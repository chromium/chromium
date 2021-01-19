// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_

#include <memory>

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace diagnostics {

class TelemetryLog;
class RoutineLog;

class SessionLogHandler {
 public:
  SessionLogHandler();
  ~SessionLogHandler();

  // Constructor for testing, allowing an injected `routine_log_path`. Should
  // not be called outside of tests.
  SessionLogHandler(const base::FilePath& routine_log_path);

  SessionLogHandler(const SessionLogHandler&) = delete;
  SessionLogHandler& operator=(const SessionLogHandler&) = delete;

  TelemetryLog* GetTelemetryLog() const;
  RoutineLog* GetRoutineLog() const;

 private:
  // Creates a session log at `file_path`. The session log includes the contents
  // of both `telemetry_log_` and `routine_log_`. Returns true if the file was
  // successfully written. Retrns false otherwise.
  bool CreateSessionLog(const base::FilePath& file_path);

  std::unique_ptr<TelemetryLog> telemetry_log_;
  std::unique_ptr<RoutineLog> routine_log_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_

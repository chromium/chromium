// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/session_log_handler.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "chromeos/components/diagnostics_ui/backend/routine_log.h"
#include "chromeos/components/diagnostics_ui/backend/telemetry_log.h"

namespace chromeos {
namespace diagnostics {
namespace {

const char kRoutineLogSectionHeader[] = "=== Routine Log === \n";
const char kTelemetryLogSectionHeader[] = "=== Telemetry Log === \n";

const char kRoutineLogPath[] = "/var/log/diagnostics_routine_log";

}  // namespace

SessionLogHandler::SessionLogHandler()
    : SessionLogHandler(base::FilePath(kRoutineLogPath)) {}

SessionLogHandler::~SessionLogHandler() = default;

SessionLogHandler::SessionLogHandler(const base::FilePath& routine_log_path)
    : telemetry_log_(std::make_unique<TelemetryLog>()),
      routine_log_(std::make_unique<RoutineLog>(routine_log_path)) {}

TelemetryLog* SessionLogHandler::GetTelemetryLog() const {
  return telemetry_log_.get();
}

RoutineLog* SessionLogHandler::GetRoutineLog() const {
  return routine_log_.get();
}

bool SessionLogHandler::CreateSessionLog(const base::FilePath& file_path) {
  // Fetch RoutineLog
  const std::string routine_log_contents = routine_log_->GetContents();

  // Fetch TelemetryLog
  const std::string telemetry_log_contents = telemetry_log_->GetContents();

  const std::string combined_contents =
      base::StrCat({kTelemetryLogSectionHeader, telemetry_log_contents,
                    kRoutineLogSectionHeader, routine_log_contents});
  return base::WriteFile(file_path, combined_contents);
}

}  // namespace diagnostics
}  // namespace chromeos

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/session_log_handler.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "chromeos/components/diagnostics_ui/backend/routine_log.h"
#include "chromeos/components/diagnostics_ui/backend/telemetry_log.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace chromeos {
namespace diagnostics {
namespace {

const char kRoutineLogSectionHeader[] = "=== Routine Log === \n";
const char kTelemetryLogSectionHeader[] = "=== Telemetry Log === \n";

const char kRoutineLogPath[] = "/var/log/diagnostics_routine_log";

}  // namespace

SessionLogHandler::SessionLogHandler(
    const SelectFilePolicyCreator& select_file_policy_creator)
    : select_file_policy_creator_(select_file_policy_creator),
      telemetry_log_(std::make_unique<TelemetryLog>()),
      routine_log_(
          std::make_unique<RoutineLog>(base::FilePath(kRoutineLogPath))) {}

SessionLogHandler::~SessionLogHandler() = default;

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

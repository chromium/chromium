// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/logging.h"

#include "base/command_line.h"

namespace ash::quick_start {

namespace {

// Passing "--quick-start-verbose-logging" on the command line will force Quick
// Start's verbose logs to be emitted to the log file regardless of the current
// vlog level or vmodules.
constexpr char kQuickStartVerboseLoggingSwitch[] =
    "quick-start-verbose-logging";

}  // namespace

ScopedLogMessage::ScopedLogMessage(const char* file,
                                   int line,
                                   logging::LogSeverity severity)
    : file_(file), line_(line), severity_(severity) {}

ScopedLogMessage::~ScopedLogMessage() {
  if (ShouldEmitToStandardLog()) {
    // Create a log for the standard logging system.
    logging::LogMessage log_message(file_, line_, severity_);
    log_message.stream() << stream_.str();
  }
}

bool ScopedLogMessage::ShouldEmitToStandardLog() const {
  // Logs should be emitted if any of the following is true:
  // - The severity is INFO or greater
  // - The Vlog Level for |file_| is at least 1
  // - The --quick-start-verbose-logging switch is enabled
  return severity_ > logging::LOG_VERBOSE ||
         logging::GetVlogLevelHelper(file_, strlen(file_) + 1) > 0 ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             kQuickStartVerboseLoggingSwitch);
}

}  // namespace ash::quick_start

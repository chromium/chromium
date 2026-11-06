// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOGGING_CHROME_H_
#define CHROME_COMMON_LOGGING_CHROME_H_

#include "base/logging/logging_settings.h"
#include "build/build_config.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace logging {

// Call to initialize logging for Chrome. This sets up the chrome-specific
// logfile naming scheme and might do other things like log modules and
// setting levels in the future.
//
// The main process might want to delete any old log files on startup by
// setting `delete_old_log_file`, but child processes should not, or they
// will delete each others' logs.
void InitChromeLogging(const base::CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file);

LoggingDestination DetermineLoggingDestination(
    const base::CommandLine& command_line);

#if BUILDFLAG(IS_CHROMEOS)
// Get the log file location.
base::FilePath GetSessionLogFile(const base::CommandLine& command_line);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Call when done using logging for Chrome.
void CleanupChromeLogging();

// Returns the fully-qualified name of the log file.
base::FilePath GetLogFileName(const base::CommandLine& command_line);

// Returns true when error/assertion dialogs are not to be shown, false
// otherwise.
bool DialogsAreSuppressed();

}  // namespace logging

#endif  // CHROME_COMMON_LOGGING_CHROME_H_

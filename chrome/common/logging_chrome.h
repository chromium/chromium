// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOGGING_CHROME_H_
#define CHROME_COMMON_LOGGING_CHROME_H_

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

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
// Prepare the log file. If `new_log` is true, rotate the previous log file to
// write new logs to the latest log file. Otherwise, we reuse the existing file
// if exists.
base::FilePath SetUpLogFile(const base::FilePath& target_path, bool new_log);

// Allow external calls to the internal method for testing.
bool RotateLogFile(const base::FilePath& target_path);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(UNIT_TEST)
// Expose the following methods only for tests.

// Point the logging symlink to the system log or the user session log.
base::FilePath SetUpSymlinkIfNeeded(const base::FilePath& symlink_path,
                                    bool new_log);
#endif  // defined(UNIT_TEST)

// Remove the logging symlink.
void RemoveSymlinkAndLog(const base::FilePath& link_path,
                         const base::FilePath& target_path);

// Get the log file directory path.
base::FilePath GetSessionLogDir(const base::CommandLine& command_line);

// Get the log file location.
base::FilePath GetSessionLogFile(const base::CommandLine& command_line);
#endif

// Call when done using logging for Chrome.
void CleanupChromeLogging();

// Returns the fully-qualified name of the log file.
base::FilePath GetLogFileName(const base::CommandLine& command_line);

// Returns true when error/assertion dialogs are not to be shown, false
// otherwise.
bool DialogsAreSuppressed();

#if BUILDFLAG(IS_CHROMEOS)
// Inserts timestamp before file extension (if any) in the form
// "_yymmdd-hhmmss".
base::FilePath GenerateTimestampedName(const base::FilePath& base_path,
                                       base::Time timestamp);
#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace logging

#endif  // CHROME_COMMON_LOGGING_CHROME_H_

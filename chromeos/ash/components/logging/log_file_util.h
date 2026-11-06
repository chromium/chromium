// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGGING_LOG_FILE_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_LOGGING_LOG_FILE_UTIL_H_

#include "base/component_export.h"
#include "base/time/time.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// Prepare the log file. If `new_log` is true, rotate the previous log file to
// write new logs to the latest log file. Otherwise, we reuse the existing file
// if exists.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING)
base::FilePath SetUpLogFile(const base::FilePath& target_path, bool new_log);

// Allow external calls to the internal method for testing.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING)
bool RotateLogFile(const base::FilePath& target_path);

// Point the logging symlink to the system log or the user session log.
// Exposed only for testing.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING)
base::FilePath SetUpSymlinkIfNeeded(const base::FilePath& symlink_path,
                                    bool new_log);

// Remove the logging symlink.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING)
void RemoveSymlinkAndLog(const base::FilePath& link_path,
                         const base::FilePath& target_path);

// Inserts timestamp before file extension (if any) in the form
// "_yymmdd-hhmmss".
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING)
base::FilePath GenerateTimestampedName(const base::FilePath& base_path,
                                       base::Time timestamp);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGGING_LOG_FILE_UTIL_H_

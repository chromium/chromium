// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_
#define CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_

namespace base {
class FilePath;
}

namespace installer {

class InitialPreferences;

// Verbose installer runs clock in at around 50K, non-verbose much less than
// that. Some installer operations span multiple setup.exe runs, so we try
// to keep enough for at least 10 runs or so at any given time.
const int kMaxInstallerLogFileSize = 1024 * 1024;

// Truncate the file down to half of the max, such that we don't incur
// truncation on every update.
const int kTruncatedInstallerLogFileSize = kMaxInstallerLogFileSize / 2;

static_assert(kTruncatedInstallerLogFileSize < kMaxInstallerLogFileSize,
              "kTruncatedInstallerLogFileSize must be less than "
              "kMaxInstallerLogFileSize");

enum TruncateResult {
  LOGFILE_UNTOUCHED,
  LOGFILE_TRUNCATED,
  LOGFILE_DELETED,
};

// Cuts off the _beginning_ of the file at |log_file| down to
// kTruncatedInstallerLogFileSize if it exceeds kMaxInstallerLogFileSize bytes.
//
// If the file is not changed, returns LOGFILE_UNTOUCHED.
// If the file is successfully truncated, returns LOGFILE_TRUNCATED.
// If the file needed truncation, but the truncation failed, the file will be
// deleted and the function returns LOGFILE_DELETED. This is done to prevent
// run-away log files and guard against full disks.
TruncateResult TruncateLogFileIfNeeded(const base::FilePath& log_file);

// Call to initialize logging for Chrome installer.
void InitInstallerLogging(const installer::InitialPreferences& prefs);

// Call when done using logging for Chrome installer.
void EndInstallerLogging();

// Returns the full path of the log file.
base::FilePath GetLogFilePath(const installer::InitialPreferences& prefs);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_LOGGING_INSTALLER_H_

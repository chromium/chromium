// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/logging_installer.h"

#include <stdint.h>
#include <windows.h>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"

// {93BCE0BF-3FAF-43b1-9E28-BEB6FAB5ECE7}
static const GUID kSetupTraceProvider = { 0x93bce0bf, 0x3faf, 0x43b1,
    { 0x9e, 0x28, 0xbe, 0xb6, 0xfa, 0xb5, 0xec, 0xe7 } };

namespace installer {

// This should be true for the period between the end of
// InitInstallerLogging() and the beginning of EndInstallerLogging().
bool installer_logging_ = false;

TruncateResult TruncateLogFileIfNeeded(const base::FilePath& log_file) {
  TruncateResult result = LOGFILE_UNTOUCHED;

  int64_t log_size = 0;
  if (base::GetFileSize(log_file, &log_size) &&
      log_size > kMaxInstallerLogFileSize) {
    // Cause the old log file to be deleted when we are done with it.
    uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                          base::File::FLAG_SHARE_DELETE |
                          base::File::FLAG_DELETE_ON_CLOSE;
    base::File old_log_file(log_file, file_flags);

    if (old_log_file.IsValid()) {
      result = LOGFILE_DELETED;
      base::FilePath tmp_log(log_file.value() + FILE_PATH_LITERAL(".tmp"));
      // Note that base::Move will attempt to replace existing files.
      if (base::Move(log_file, tmp_log)) {
        int64_t offset = log_size - kTruncatedInstallerLogFileSize;
        std::string old_log_data(kTruncatedInstallerLogFileSize, 0);
        int bytes_read = old_log_file.Read(offset,
                                           &old_log_data[0],
                                           kTruncatedInstallerLogFileSize);
        if (bytes_read > 0 &&
            (bytes_read == base::WriteFile(log_file, &old_log_data[0],
                                           bytes_read) ||
             base::PathExists(log_file))) {
          result = LOGFILE_TRUNCATED;
        }
      }
    } else if (base::DeleteFile(log_file, false)) {
      // Couldn't get sufficient access to the log file, optimistically try to
      // delete it.
      result = LOGFILE_DELETED;
    }
  }

  return result;
}


void InitInstallerLogging(const installer::MasterPreferences& prefs) {
  if (installer_logging_)
    return;

  installer_logging_ = true;

  bool value = false;
  if (prefs.GetBool(installer::master_preferences::kDisableLogging,
                    &value) && value) {
    return;
  }

  base::FilePath log_file_path(GetLogFilePath(prefs));
  TruncateLogFileIfNeeded(log_file_path);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file_path = log_file_path.value().c_str();
  logging::InitLogging(settings);

  if (prefs.GetBool(installer::master_preferences::kVerboseLogging,
                    &value) && value) {
    logging::SetMinLogLevel(logging::LOG_VERBOSE);
  } else {
    logging::SetMinLogLevel(logging::LOG_ERROR);
  }

  // Enable ETW logging.
  logging::LogEventProvider::Initialize(kSetupTraceProvider);
}

void EndInstallerLogging() {
  logging::CloseLogFile();

  installer_logging_ = false;
}

base::FilePath GetLogFilePath(const installer::MasterPreferences& prefs) {
  std::string path;
  prefs.GetString(installer::master_preferences::kLogFile, &path);
  if (!path.empty())
    return base::FilePath(base::UTF8ToWide(path));

  static const base::FilePath::CharType kLogFilename[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      FILE_PATH_LITERAL("chrome_installer.log");
#else  // BUILDFLAG(CHROMIUM_BRANDING)
      FILE_PATH_LITERAL("chromium_installer.log");
#endif

  // Fallback to current directory if getting the temp directory fails.
  base::FilePath tmp_path;
  ignore_result(base::PathService::Get(base::DIR_TEMP, &tmp_path));
  return tmp_path.Append(kLogFilename);
}

}  // namespace installer

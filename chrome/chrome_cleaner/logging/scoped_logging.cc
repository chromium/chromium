// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/scoped_logging.h"

#include <memory>

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging_win.h"
#include "base/win/current_module.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

namespace {

// {985388DD-5F6A-40A9-A4D2-86D8547EFB52}
const GUID kChromeCleanerTraceProviderName = {
    0x985388DD,
    0x5F6A,
    0x40A9,
    {0xA4, 0xD2, 0x86, 0xD8, 0x54, 0x7E, 0xFB, 0x52}};

// The log file extension.
const wchar_t kLogFileExtension[] = L"log";

base::FilePath GetLoggingDirectory() {
  base::FilePath logging_directory =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kTestLoggingPathSwitch);
  if (logging_directory.empty()) {
    if (!GetAppDataProductDirectory(&logging_directory))
      return base::FilePath();
  }
  return logging_directory;
}

}  // namespace

ScopedLogging::ScopedLogging(base::FilePath::StringPieceType suffix) {
  // Log to an ETW facility for convenience.
  // This swallows all log lines it gets, so we need to initialize it before
  // LoggingServiceAPI so LoggingServiceAPI can see the logs first, which it
  // then passes on to LogEventProvider.
  logging::LogEventProvider::Initialize(kChromeCleanerTraceProviderName);

  // Initialize the logging global state.
  LoggingServiceAPI* logging_service = LoggingServiceAPI::GetInstance();
  logging_service->Initialize(nullptr);

  const base::FilePath log_file_path = GetLogFilePath(suffix);

  // Truncate log files to 40kB. This should be enough to cover logs of the
  // previous run (99th percentile of uploaded raw log line size is 36kB).
  TruncateLogFileToTail(log_file_path, 40 * 1000);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_FILE;
  logging_settings.log_file_path = log_file_path.value().c_str();

  bool success = logging::InitLogging(logging_settings);
  DCHECK(success);
  LOG(INFO) << "Starting logs for version: " << CHROME_CLEANER_VERSION_STRING;

  logging_service->EnableUploads(
      chrome_cleaner::Settings::GetInstance()->logs_upload_allowed(), nullptr);
}

ScopedLogging::~ScopedLogging() {
  // Terminate the service to avoid work being done in the destructor called by
  // the AtExitManager.
  LoggingServiceAPI::GetInstance()->Terminate();

  // Kill our ETW provider.
  logging::LogEventProvider::Uninitialize();
}

// static
base::FilePath ScopedLogging::GetLogFilePath(
    base::FilePath::StringPieceType suffix) {
  // Initialize the logging settings to set a specific log file name.
  std::unique_ptr<FileVersionInfo> version(
      FileVersionInfo::CreateFileVersionInfoForModule(CURRENT_MODULE()));

  // Test executables don't have version resources.
  base::FilePath original_filename;
  if (version.get()) {
    original_filename = base::FilePath(version->original_filename());
  } else {
    original_filename =
        PreFetchedPaths::GetInstance()->GetExecutablePath().BaseName();
  }

  if (!suffix.empty())
    original_filename = original_filename.InsertBeforeExtension(suffix);

  base::FilePath log_file_path =
      original_filename.ReplaceExtension(kLogFileExtension);
  base::FilePath logging_directory = GetLoggingDirectory();
  if (!logging_directory.empty())
    log_file_path = logging_directory.Append(log_file_path);
  return log_file_path;
}

}  // namespace chrome_cleaner

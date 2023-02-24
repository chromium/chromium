// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/logging_service_api.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/logging/logging_definitions.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

namespace {

const wchar_t kProtoExtension[] = L"pb";

}  // namespace

// static
LoggingServiceAPI* LoggingServiceAPI::logging_service_for_testing_ = nullptr;

// static
LoggingServiceAPI* LoggingServiceAPI::GetInstance() {
  if (logging_service_for_testing_)
    return logging_service_for_testing_;
  return GetLoggingServiceForCurrentBuild();
}

// static
void LoggingServiceAPI::SetInstanceForTesting(
    LoggingServiceAPI* logging_service) {
  logging_service_for_testing_ = logging_service;
}

void LoggingServiceAPI::MaybeSaveLogsToFile(const std::wstring& tag) {
#if !defined(NDEBUG)
  // Always dump the raw logs in debug builds.
  const bool dump_raw_logs = true;
#else
  const bool dump_raw_logs =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kDumpRawLogsSwitch);
#endif
  if (dump_raw_logs) {
    base::FilePath exe_file_path =
        PreFetchedPaths::GetInstance()->GetExecutablePath();

    base::FilePath log_file_path(exe_file_path.ReplaceExtension(kProtoExtension)
                                     .InsertBeforeExtension(tag));

    std::string logs_proto = RawReportContent();
    base::WriteFile(log_file_path, logs_proto);
  }
}

}  // namespace chrome_cleaner

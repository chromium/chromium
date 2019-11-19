// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/crash_client.h"

#include <algorithm>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/updater/util.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/prune_crash_reports.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/wrapped_window_proc.h"
#endif

namespace {

#if defined(OS_WIN)

int __cdecl HandleWinProcException(EXCEPTION_POINTERS* exception_pointers) {
  crashpad::CrashpadClient::DumpAndCrash(exception_pointers);
  return EXCEPTION_CONTINUE_SEARCH;
}

#endif

}  // namespace

namespace updater {

CrashClient::CrashClient() = default;
CrashClient::~CrashClient() = default;

// static
CrashClient* CrashClient::GetInstance() {
  static base::NoDestructor<CrashClient> crash_client;
  return crash_client.get();
}

bool CrashClient::InitializeDatabaseOnly() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath handler_path;
  base::PathService::Get(base::FILE_EXE, &handler_path);

  base::FilePath database_path;
  if (!GetProductDirectory(&database_path)) {
    LOG(ERROR) << "Failed to get the database path.";
    return false;
  }

  database_ = crashpad::CrashReportDatabase::Initialize(database_path);
  if (!database_) {
    LOG(ERROR) << "Failed to initialize Crashpad database.";
    return false;
  }

  return true;
}

bool CrashClient::InitializeCrashReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!InitializeDatabaseOnly())
    return false;

#if defined(OS_WIN)
  // Catch exceptions thrown from a window procedure.
  base::win::WinProcExceptionFilter exception_filter =
      base::win::SetWinProcExceptionFilter(&HandleWinProcException);
  LOG_IF(DFATAL, exception_filter) << "Exception filter already present";
#endif  // OS_WIN

  std::vector<crashpad::CrashReportDatabase::Report> reports_completed;
  const crashpad::CrashReportDatabase::OperationStatus status_completed =
      database_->GetCompletedReports(&reports_completed);
  if (status_completed == crashpad::CrashReportDatabase::kNoError) {
    VLOG(1) << "Found " << reports_completed.size()
            << " completed crash reports";
    for (const auto& report : reports_completed) {
      VLOG(1) << "Crash since last run: ID \"" << report.id << "\", created at "
              << report.creation_time << ", " << report.upload_attempts
              << " upload attempts, file path \"" << report.file_path
              << "\", unique ID \"" << report.uuid.ToString()
              << "\"; uploaded: " << (report.uploaded ? "yes" : "no");
    }
  } else {
    LOG(ERROR) << "Failed to fetch completed crash reports: "
               << status_completed;
  }

  std::vector<crashpad::CrashReportDatabase::Report> reports_pending;
  const crashpad::CrashReportDatabase::OperationStatus status_pending =
      database_->GetPendingReports(&reports_pending);
  if (status_pending == crashpad::CrashReportDatabase::kNoError) {
    VLOG(1) << "Found " << reports_pending.size() << " pending crash reports";
    for (const auto& report : reports_pending) {
      VLOG(1) << "Crash since last run: (pending), created at "
              << report.creation_time << ", " << report.upload_attempts
              << " upload attempts, file path \"" << report.file_path
              << "\", unique ID \"" << report.uuid.ToString() << "\"";
    }
  } else {
    LOG(ERROR) << "Failed to fetch pending crash reports: " << status_pending;
  }

  // TODO(sorin): fix before shipping to users, crbug.com/940098.
  crashpad::Settings* crashpad_settings = database_->GetSettings();
  DCHECK(crashpad_settings);
  crashpad_settings->SetUploadsEnabled(true);

  return true;
}

// static
std::string CrashClient::GetClientId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetInstance()->sequence_checker_);
  DCHECK(GetInstance()->database_) << "Crash reporting not initialized";
  crashpad::Settings* settings = GetInstance()->database_->GetSettings();
  DCHECK(settings);

  crashpad::UUID uuid;
  if (!settings->GetClientID(&uuid)) {
    LOG(ERROR) << "Unable to retrieve client ID from Crashpad database";
    return {};
  }

  std::string uuid_string = uuid.ToString();
  base::ReplaceSubstringsAfterOffset(&uuid_string, 0, "-", "");
  return uuid_string;
}

// static
bool CrashClient::IsUploadEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetInstance()->sequence_checker_);
  DCHECK(GetInstance()->database_) << "Crash reporting not initialized";
  crashpad::Settings* settings = GetInstance()->database_->GetSettings();
  DCHECK(settings);

  bool upload_enabled = false;
  if (!settings->GetUploadsEnabled(&upload_enabled)) {
    LOG(ERROR) << "Unable to verify if crash uploads are enabled or not";
    return false;
  }
  return upload_enabled;
}

}  // namespace updater

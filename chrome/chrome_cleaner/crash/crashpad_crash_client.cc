// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/crash/crashpad_crash_client.h"

#include <process.h>
#include <psapi.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/win/wrapped_window_proc.h"
#include "chrome/chrome_cleaner/chrome_utils/chrome_util.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/crash/crash_keys.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/settings/engine_settings.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"

namespace chrome_cleaner {

namespace {

base::LazyInstance<base::Lock>::Leaky record_memory_usage_and_crash_lock =
    LAZY_INSTANCE_INITIALIZER;

// NOTE: the following functions will be executed when the application is likely
// in the process of crashing. That could be because there is no memory left, so
// we must avoid allocating memory when they are run.

// Helper function to format a DWORD value to a string. Does not allocate
// memory. The returned value is valid until the next call to this function.
// This function is not thread-safe.
const char* ConvertDwordToString(DWORD value) {
  static_assert(sizeof(value) == 4, "DWORD is not 32 bits?");

  // The maximum value represented by a DWORD value is 4294967295, which is 10
  // characters long. Leave space for a NULL terminator character.
  static char buffer[11] = {};

  _snprintf(buffer, base::size(buffer), "%u", value);
  buffer[base::size(buffer) - 1] = '\0';
  return buffer;
}

// Helper function to format a SIZE_T value to a string. Does not allocate
// memory. The returned value is valid until the next call to this function.
// This function is not thread-safe.
const char* ConvertSizeTToString(SIZE_T value) {
  static_assert(sizeof(value) <= 8, "SIZE_T is more than 64 bits?");

  // The maximum value represented by a 64-bit SIZE_T value is
  // 18446744073709551616, which is 20 characters long. Leave space for a NULL
  // terminator character.
  static char buffer[21] = {};

  _snprintf(buffer, base::size(buffer), "%Iu", value);
  buffer[base::size(buffer) - 1] = '\0';
  return buffer;
}

// Exception handler. Adds crash keys with memory usage information, then
// hands control over to the Crashpad crash handler.
LONG WINAPI RecordMemoryUsageAndCrash(EXCEPTION_POINTERS* exception_pointers) {
  static PROCESS_MEMORY_COUNTERS counters = {};

  {
    base::AutoLock auto_lock(record_memory_usage_and_crash_lock.Get());

    // Call GetProcessMemoryInfo directly instead of using base::ProcessMetrics,
    // to keep stack allocations to a minimum.
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters,
                             sizeof(counters))) {
      // Crashpad pre-allocates space for 64 crash keys, so setting these here
      // does not allocate memory. Both key and value can be up to 256
      // characters long.
      SetCrashKey("PageFaultCount",
                  ConvertDwordToString(counters.PageFaultCount));
      SetCrashKey("PeakWorkingSetSize",
                  ConvertSizeTToString(counters.PeakWorkingSetSize));
      SetCrashKey("WorkingSetSize",
                  ConvertSizeTToString(counters.WorkingSetSize));
      SetCrashKey("QuotaPeakPagedPoolUsage",
                  ConvertSizeTToString(counters.QuotaPeakPagedPoolUsage));
      SetCrashKey("QuotaPagedPoolUsage",
                  ConvertSizeTToString(counters.QuotaPagedPoolUsage));
      SetCrashKey("QuotaPeakNonPagedPoolUsage",
                  ConvertSizeTToString(counters.QuotaPeakNonPagedPoolUsage));
      SetCrashKey("QuotaNonPagedPoolUsage",
                  ConvertSizeTToString(counters.QuotaNonPagedPoolUsage));
      SetCrashKey("PagefileUsage",
                  ConvertSizeTToString(counters.PagefileUsage));
      SetCrashKey("PeakPagefileUsage",
                  ConvertSizeTToString(counters.PeakPagefileUsage));
    }
  }

  crashpad::CrashpadClient::DumpAndCrash(exception_pointers);
  return EXCEPTION_CONTINUE_SEARCH;
}

// Wraps the DumpAndCrash function info a function that has the right return
// type to be passed to base::win::SetWinProcExceptionFilter.
int __cdecl HandleWinProcException(EXCEPTION_POINTERS* info) {
  return RecordMemoryUsageAndCrash(info);
}

}  // namespace

// static
CrashClient* CrashClient::GetInstance() {
  return CrashpadCrashClient::GetInstance();
}

// static
void CrashClient::GetClientId(base::string16* client_id) {
  CrashpadCrashClient::GetClientId(client_id);
}

// static
bool CrashClient::IsUploadEnabled() {
  return CrashpadCrashClient::IsUploadEnabled();
}

CrashpadCrashClient::~CrashpadCrashClient() = default;

bool CrashpadCrashClient::InitializeDatabaseOnly() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath database_path;
  if (!chrome_cleaner::GetAppDataProductDirectory(&database_path)) {
    LOG(ERROR) << "Failed to get AppData product directory";
    return false;
  }

  database_.reset(
      crashpad::CrashReportDatabase::Initialize(database_path).release());
  if (!database_) {
    LOG(ERROR) << "Failed to initialize Crashpad database.";
    return false;
  }

  return true;
}

bool CrashpadCrashClient::InitializeCrashReporting(Mode mode,
                                                   SandboxType process_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static bool initialized = false;
  DCHECK(!initialized);
  initialized = true;

  UseCrashKeysToAnnotate(crashpad::CrashpadInfo::GetCrashpadInfo());

  SetCrashKey("pid", base::NumberToString(_getpid()));

  static_assert(static_cast<int>(Mode::MODE_COUNT) == 2,
                "Update the annotation below if a new Mode is added");
  constexpr char kModeString[] = "mode";
  switch (mode) {
    case Mode::REPORTER:
      SetCrashKey(kModeString, "reporter");
      break;
    case Mode::CLEANER:
      SetCrashKey(kModeString, "cleaner");
      break;
    default:
      NOTREACHED();
  }

  constexpr char kProcessType[] = "process_type";
  switch (process_type) {
    case SandboxType::kNonSandboxed:
      SetCrashKey(kProcessType, "broker");
      break;
    case SandboxType::kEngine:
      SetCrashKey(
          kProcessType,
          base::ToLowerASCII(GetEngineName(Settings::GetInstance()->engine())));
      break;
    case SandboxType::kParser:
      SetCrashKey(kProcessType, "parser");
      break;
    case SandboxType::kZipArchiver:
      SetCrashKey(kProcessType, "zip_archiver");
      break;
    default:
      NOTREACHED();
  }

  SetCrashKey("uma", Settings::GetInstance()->metrics_enabled() ? "1" : "0");

  SetCrashKey("testing",
              base::CommandLine::ForCurrentProcess()->HasSwitch(kTestingSwitch)
                  ? "1"
                  : "0");

  base::string16 chrome_version;
  bool chrome_system_install;
  RetrieveChromeVersionAndInstalledDomain(&chrome_version,
                                          &chrome_system_install);
  SetCrashKey("ChromeVersion", base::UTF16ToUTF8(chrome_version));
  SetCrashKey("ChromeSystemInstall", chrome_system_install ? "1" : "0");

  SetCrashKeysFromCommandLine();

  static_assert(static_cast<int>(Mode::MODE_COUNT) == 2,
                "Update the condition below if a new Mode is added");
  if (mode == Mode::CLEANER)
    SetCrashKey("CleanupId", Settings::GetInstance()->cleanup_id());

  const std::string engine_version = Settings::GetInstance()->engine_version();
  if (!engine_version.empty())
    SetCrashKey("EngineVersion", engine_version);

  if (!InitializeDatabaseOnly())
    return false;

  // Replace Crashpad's exception filter with our own filter, which records
  // memory usage at the time of the crash, then tells Crashpad to dump and
  // crash.
  SetUnhandledExceptionFilter(&RecordMemoryUsageAndCrash);

  // Catch exceptions thrown from a window procedure.
  base::win::WinProcExceptionFilter exception_filter =
      base::win::SetWinProcExceptionFilter(&HandleWinProcException);
  LOG_IF(DFATAL, exception_filter) << "Exception filter already present";

  // Log completed crash reports in the logs that will be uploaded to Safe
  // Browsing.
  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  const crashpad::CrashReportDatabase::OperationStatus status_completed =
      database_->GetCompletedReports(&completed_reports);
  if (status_completed == crashpad::CrashReportDatabase::kNoError) {
    LOG(INFO) << "Found " << completed_reports.size()
              << " completed crash reports";
    for (const auto& report : completed_reports) {
      LOG(INFO) << "Crash since last run: ID \"" << report.id
                << "\", created at " << report.creation_time << ", "
                << report.upload_attempts << " upload attempts, file path \""
                << SanitizePath(report.file_path) << "\", unique ID \""
                << report.uuid.ToString()
                << "\"; uploaded: " << (report.uploaded ? "yes" : "no");
    }
  } else {
    LOG(ERROR) << "Failed to fetch completed crash reports: "
               << status_completed;
  }

  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  const crashpad::CrashReportDatabase::OperationStatus status_pending =
      database_->GetPendingReports(&pending_reports);
  if (status_pending == crashpad::CrashReportDatabase::kNoError) {
    LOG(INFO) << "Found " << pending_reports.size() << " pending crash reports";
    for (const auto& report : pending_reports) {
      LOG(INFO) << "Crash since last run: (pending), created at "
                << report.creation_time << ", " << report.upload_attempts
                << " upload attempts, file path \""
                << SanitizePath(report.file_path) << "\", unique ID \""
                << report.uuid.ToString() << "\"";
    }
  } else {
    LOG(ERROR) << "Failed to fetch pending crash reports: " << status_pending;
  }

  DeleteStaleReports();

  // Enable or disable crash reporting based on settings.
  crashpad::Settings* crashpad_settings = database_->GetSettings();
  DCHECK(crashpad_settings);
  crashpad_settings->SetUploadsEnabled(
      Settings::GetInstance()->allow_crash_report_upload());

  return true;
}

void CrashpadCrashClient::DeleteStaleReports() {
  const int kMaxReportCount = 3;

  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  if (database_->GetCompletedReports(&completed_reports) !=
      crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> not_uploaded_reports;
  for (const auto& report : completed_reports) {
    if (report.uploaded)
      database_->DeleteReport(report.uuid);
    else
      not_uploaded_reports.push_back(report);
  }

  // Sort reports from newest to oldest, then delete all but the newest
  // kMaxReportCount reports.
  std::sort(not_uploaded_reports.begin(), not_uploaded_reports.end(),
            [](const auto& report1, const auto& report2) {
              return report1.creation_time > report2.creation_time;
            });
  for (size_t i = kMaxReportCount; i < not_uploaded_reports.size(); ++i)
    database_->DeleteReport(not_uploaded_reports[i].uuid);
}

// static
CrashpadCrashClient* CrashpadCrashClient::GetInstance() {
  return base::Singleton<CrashpadCrashClient, base::LeakySingletonTraits<
                                                  CrashpadCrashClient>>::get();
}

// static
void CrashpadCrashClient::GetClientId(base::string16* client_id) {
  DCHECK(client_id);
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetInstance()->sequence_checker_);
  DCHECK(GetInstance()->database_) << "Crash reporting not initialized";
  crashpad::Settings* settings = GetInstance()->database_->GetSettings();
  DCHECK(settings);

  crashpad::UUID uuid;
  if (!settings->GetClientID(&uuid)) {
    LOG(ERROR) << "Unable to retrieve client ID from Crashpad database";
    *client_id = base::string16();
    return;
  }

  std::string uuid_string = uuid.ToString();
  base::ReplaceSubstringsAfterOffset(&uuid_string, 0, "-", "");
  *client_id = base::UTF8ToWide(uuid_string);
}

// static
bool CrashpadCrashClient::IsUploadEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(GetInstance()->sequence_checker_);
  DCHECK(GetInstance()->database_) << "Crash reporting not initialized";
  crashpad::Settings* settings = GetInstance()->database_->GetSettings();
  DCHECK(settings);
  bool upload_enabled = false;
  if (settings->GetUploadsEnabled(&upload_enabled)) {
    return upload_enabled;
  } else {
    LOG(ERROR) << "Unable to verify if crash uploads are enabled or not";
    return false;
  }
}

CrashpadCrashClient::CrashpadCrashClient() = default;

}  // namespace chrome_cleaner

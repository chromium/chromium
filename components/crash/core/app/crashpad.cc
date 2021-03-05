// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/auto_reset.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/client/annotation_list.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/client/simulate_crash.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif  // OS_POSIX

#if defined(OS_WIN)
#include "components/crash/core/app/crash_export_thunks.h"
#endif

namespace crash_reporter {

namespace {

base::FilePath* g_database_path;

crashpad::CrashReportDatabase* g_database;

bool LogMessageHandler(int severity,
                       const char* file,
                       int line,
                       size_t message_start,
                       const std::string& string) {
  // Only handle FATAL.
  if (severity != logging::LOG_FATAL) {
    return false;
  }

  // In case of an out-of-memory condition, this code could be reentered when
  // constructing and storing the key. Using a static is not thread-safe, but if
  // multiple threads are in the process of a fatal crash at the same time, this
  // should work.
  static bool guarded = false;
  if (guarded) {
    return false;
  }
  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.  This matches logging.cc.
  if (file) {
    const char* slash = strrchr(file, '/');
    if (slash) {
      file = slash + 1;
    }
  }

  CHECK_LE(message_start, string.size());
  std::string message = base::StringPrintf("%s:%d: %s", file, line,
                                           string.c_str() + message_start);
  static crashpad::StringAnnotation<512> crash_key("LOG_FATAL");
  crash_key.Set(message);

  // Rather than including the code to force the crash here, allow the caller to
  // do it.
  return false;
}

void InitializeDatabasePath(const base::FilePath& database_path) {
  DCHECK(!g_database_path);

  // Intentionally leaked.
  g_database_path = new base::FilePath(database_path);
}

void InitializeCrashpadImpl(bool initial_client,
                            const std::string& process_type,
                            const std::string& user_data_dir,
                            const base::FilePath& exe_path,
                            const std::vector<std::string>& initial_arguments,
                            bool embedded_handler) {
  static bool initialized = false;
  DCHECK(!initialized);
  initialized = true;

  const bool browser_process = process_type.empty();

  if (initial_client) {
#if defined(OS_APPLE)
    // "relauncher" is hard-coded because it's a Chrome --type, but this
    // component can't see Chrome's switches. This is only used for argument
    // sanitization.
    DCHECK(browser_process || process_type == "relauncher" ||
           process_type == "app_shim");
#elif defined(OS_WIN)
    // "Chrome Installer" is the name historically used for installer binaries
    // as processed by the backend.
    DCHECK(browser_process || process_type == "Chrome Installer" ||
           process_type == "notification-helper" ||
           process_type == "GCPW Installer" || process_type == "GCPW DLL");
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
    DCHECK(browser_process);
#else
#error Port.
#endif  // OS_APPLE
  } else {
    DCHECK(!browser_process);
  }

  // database_path is only valid in the browser process.
  base::FilePath database_path = internal::PlatformCrashpadInitialization(
      initial_client, browser_process, embedded_handler, user_data_dir,
      exe_path, initial_arguments);

#if defined(OS_APPLE)
#if defined(NDEBUG)
  const bool is_debug_build = false;
#else
  const bool is_debug_build = true;
#endif

  // Disable forwarding to the system's crash reporter in processes other than
  // the browser process. For the browser, the system's crash reporter presents
  // the crash UI to the user, so it's desirable there. Additionally, having
  // crash reports appear in ~/Library/Logs/DiagnosticReports provides a
  // fallback. Forwarding is turned off for debug-mode builds even for the
  // browser process, because the system's crash reporter can take a very long
  // time to chew on symbols.
  if (!browser_process || is_debug_build) {
    crashpad::CrashpadInfo::GetCrashpadInfo()
        ->set_system_crash_reporter_forwarding(crashpad::TriState::kDisabled);
  }
#endif  // OS_APPLE

  crashpad::AnnotationList::Register();

#if !defined(OS_IOS)
  static crashpad::StringAnnotation<24> ptype_key("ptype");
  ptype_key.Set(browser_process ? base::StringPiece("browser")
                                : base::StringPiece(process_type));

  static crashpad::StringAnnotation<12> pid_key("pid");
#if defined(OS_POSIX)
  pid_key.Set(base::NumberToString(getpid()));
#elif defined(OS_WIN)
  pid_key.Set(base::NumberToString(::GetCurrentProcessId()));
#endif

  static crashpad::StringAnnotation<24> osarch_key("osarch");
  osarch_key.Set(base::SysInfo::OperatingSystemArchitecture());
#else
  // "platform" is used to determine device_model on the crash server.
  static crashpad::StringAnnotation<24> platform("platform");
  platform.Set(base::SysInfo::HardwareModelName());
#endif  // OS_IOS

  logging::SetLogMessageHandler(LogMessageHandler);

  // If clients called CRASHPAD_SIMULATE_CRASH() instead of
  // base::debug::DumpWithoutCrashing(), these dumps would appear as crashes in
  // the correct function, at the correct file and line. This would be
  // preferable to having all occurrences show up in DumpWithoutCrashing() at
  // the same file and line.
  base::debug::SetDumpWithoutCrashingFunction(DumpWithoutCrashing);

#if defined(OS_APPLE)
  // On Mac, we only want the browser to initialize the database, but not the
  // relauncher.
  const bool should_initialize_database_and_set_upload_policy = browser_process;
#elif defined(OS_WIN)
  // On Windows, we want both the browser process and the installer and any
  // other "main, first process" to initialize things. There is no "relauncher"
  // on Windows, so this is synonymous with initial_client.
  const bool should_initialize_database_and_set_upload_policy = initial_client;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  const bool should_initialize_database_and_set_upload_policy = browser_process;
#endif
  if (should_initialize_database_and_set_upload_policy) {
    InitializeDatabasePath(database_path);

    g_database =
        crashpad::CrashReportDatabase::Initialize(database_path).release();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
    SetUploadConsent(crash_reporter_client->GetCollectStatsConsent());
#endif
  }
}

}  // namespace

void InitializeCrashpad(bool initial_client, const std::string& process_type) {
  InitializeCrashpadImpl(initial_client, process_type, std::string(),
                         base::FilePath(), std::vector<std::string>(), false);
}

#if defined(OS_WIN)
void InitializeCrashpadWithEmbeddedHandler(bool initial_client,
                                           const std::string& process_type,
                                           const std::string& user_data_dir,
                                           const base::FilePath& exe_path) {
  InitializeCrashpadImpl(initial_client, process_type, user_data_dir, exe_path,
                         std::vector<std::string>(), true);
}

void InitializeCrashpadWithDllEmbeddedHandler(
    bool initial_client,
    const std::string& process_type,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments) {
  InitializeCrashpadImpl(initial_client, process_type, user_data_dir, exe_path,
                         initial_arguments, true);
}
#endif  // OS_WIN

crashpad::CrashpadClient& GetCrashpadClient() {
  static crashpad::CrashpadClient* const client =
      new crashpad::CrashpadClient();
  return *client;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SetUploadConsent(bool consent) {
  if (!g_database)
    return;

  bool enable_uploads = false;
  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  if (!crash_reporter_client->ReportingIsEnforcedByPolicy(&enable_uploads)) {
    // Breakpad provided a --disable-breakpad switch to disable crash dumping
    // (not just uploading) here. Crashpad doesn't need it: dumping is enabled
    // unconditionally and uploading is gated on consent, which tests/bots
    // shouldn't have. As a precaution, uploading is also disabled on bots even
    // if consent is present.
    enable_uploads = consent && !crash_reporter_client->IsRunningUnattended();
  }

  crashpad::Settings* settings = g_database->GetSettings();
  settings->SetUploadsEnabled(enable_uploads &&
                              crash_reporter_client->GetCollectStatsInSample());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !defined(OS_ANDROID)
void DumpWithoutCrashing() {
  CRASHPAD_SIMULATE_CRASH();
}
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
void CrashWithoutDumping(const std::string& message) {
  crashpad::CrashpadClient::CrashWithoutDump(message);
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)

void GetReports(std::vector<Report>* reports) {
#if defined(OS_WIN)
  // On Windows, the crash client may be linked into another module, which
  // does the client registration. That means the global that holds the crash
  // report database lives across a module boundary, where the other module
  // implements the GetCrashReportsImpl function. Since the other module has
  // a separate allocation domain, this awkward copying is necessary.

  // Start with an arbitrary copy size.
  reports->resize(25);
  while (true) {
    size_t available_reports =
        GetCrashReports_ExportThunk(&reports->at(0), reports->size());
    if (available_reports <= reports->size()) {
      // The input size was large enough to capture all available crashes.
      // Trim the vector to the actual number of reports returned and return.
      reports->resize(available_reports);
      return;
    }

    // Resize to the number of available reports, plus some slop to all but
    // eliminate the possibility of running around the loop again due to a
    // newly arrived crash report.
    reports->resize(available_reports + 5);
  }
#else
  GetReportsImpl(reports);
#endif
}

void RequestSingleCrashUpload(const std::string& local_id) {
#if defined(OS_WIN)
  // On Windows, crash reporting may be implemented in another module, which is
  // why this can't call crash_reporter::RequestSingleCrashUpload directly.
  RequestSingleCrashUpload_ExportThunk(local_id.c_str());
#else
  crash_reporter::RequestSingleCrashUploadImpl(local_id);
#endif
}

base::FilePath GetCrashpadDatabasePath() {
#if defined(OS_WIN)
  return base::FilePath(GetCrashpadDatabasePath_ExportThunk());
#else
  return base::FilePath(GetCrashpadDatabasePathImpl());
#endif
}

void ClearReportsBetween(const base::Time& begin, const base::Time& end) {
#if defined(OS_WIN)
  ClearReportsBetween_ExportThunk(begin.ToTimeT(), end.ToTimeT());
#else
  ClearReportsBetweenImpl(begin.ToTimeT(), end.ToTimeT());
#endif
}

void GetReportsImpl(std::vector<Report>* reports) {
  reports->clear();

  if (!g_database) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> completed_reports;
  crashpad::CrashReportDatabase::OperationStatus status =
      g_database->GetCompletedReports(&completed_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  std::vector<crashpad::CrashReportDatabase::Report> pending_reports;
  status = g_database->GetPendingReports(&pending_reports);
  if (status != crashpad::CrashReportDatabase::kNoError) {
    return;
  }

  for (const crashpad::CrashReportDatabase::Report& completed_report :
       completed_reports) {
    Report report = {};

    // TODO(siggi): CHECK that this fits?
    base::strlcpy(report.local_id, completed_report.uuid.ToString().c_str(),
                  sizeof(report.local_id));

    report.capture_time = completed_report.creation_time;
    base::strlcpy(report.remote_id, completed_report.id.c_str(),
                  sizeof(report.remote_id));
    if (completed_report.uploaded) {
      report.upload_time = completed_report.last_upload_attempt_time;
      report.state = ReportUploadState::Uploaded;
    } else {
      report.upload_time = 0;
      report.state = ReportUploadState::NotUploaded;
    }
    reports->push_back(report);
  }

  for (const crashpad::CrashReportDatabase::Report& pending_report :
       pending_reports) {
    Report report = {};
    base::strlcpy(report.local_id, pending_report.uuid.ToString().c_str(),
                  sizeof(report.local_id));
    report.capture_time = pending_report.creation_time;
    report.upload_time = 0;
    report.state = pending_report.upload_explicitly_requested
                       ? ReportUploadState::Pending_UserRequested
                       : ReportUploadState::Pending;
    reports->push_back(report);
  }

  std::sort(reports->begin(), reports->end(),
            [](const Report& a, const Report& b) {
              return a.capture_time > b.capture_time;
            });
}

void RequestSingleCrashUploadImpl(const std::string& local_id) {
  if (!g_database)
    return;
  crashpad::UUID uuid;
  uuid.InitializeFromString(local_id);
  g_database->RequestUpload(uuid);
}

base::FilePath::StringType::const_pointer GetCrashpadDatabasePathImpl() {
  if (!g_database_path)
    return nullptr;

  return g_database_path->value().c_str();
}

void ClearReportsBetweenImpl(time_t begin, time_t end) {
  std::vector<Report> reports;
  GetReports(&reports);
  for (const Report& report : reports) {
    // Delete if either time lies in the range, as they both reveal that the
    // browser was open.
    if ((begin <= report.capture_time && report.capture_time <= end) ||
        (begin <= report.upload_time && report.upload_time <= end)) {
      crashpad::UUID uuid;
      uuid.InitializeFromString(report.local_id);
      g_database->DeleteReport(uuid);
    }
  }
}

namespace internal {

crashpad::CrashReportDatabase* GetCrashReportDatabase() {
  return g_database;
}

}  // namespace internal

}  // namespace crash_reporter

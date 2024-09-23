// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/crash_client.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "chrome/enterprise_companion/enterprise_companion.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/client/simulate_crash.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#include "third_party/crashpad/crashpad/util/misc/tri_state.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <iterator>

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/wrapped_window_proc.h"

namespace {

int __cdecl HandleWinProcException(EXCEPTION_POINTERS* exception_pointers) {
  crashpad::CrashpadClient::DumpAndCrash(exception_pointers);
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_companion {

namespace {

constexpr char kNoRateLimitSwitch[] = "no-rate-limit";
constexpr char kUsageStatsEnabledEnvVar[] = "GOOGLE_USAGE_STATS_ENABLED";
constexpr char kUsageStatsEnabledEnvVarValueEnabled[] = "1";

#if BUILDFLAG(IS_MAC)
constexpr char kResetCrashHandlerPortSwitch[] =
    "reset-own-crash-exception-port-to-system-default";
#endif  // BUILDFLAG(IS_MAC)

// Determines if crash dump uploading should be enabled.
bool ShouldEnableCrashUploads() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableUsageStatsSwitch)) {
    return true;
  }

  std::string env_usage_stats;
  if (base::Environment::Create()->GetVar(kUsageStatsEnabledEnvVar,
                                          &env_usage_stats) &&
      env_usage_stats == kUsageStatsEnabledEnvVarValueEnabled) {
    return true;
  }

  return false;
}

std::vector<std::string> MakeCrashHandlerArgs() {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(kCrashHandlerSwitch);
#if BUILDFLAG(IS_MAC)
  command_line.AppendSwitch(kResetCrashHandlerPortSwitch);
#endif  // BUILDFLAG(IS_MAC)

  // The first element in the command line arguments is the program name,
  // which must be skipped.
#if BUILDFLAG(IS_WIN)
  std::vector<std::string> args;
  base::ranges::transform(
      ++command_line.argv().begin(), command_line.argv().end(),
      std::back_inserter(args),
      [](const auto& arg) { return base::WideToUTF8(arg); });

  return args;
#else
  return {++command_line.argv().begin(), command_line.argv().end()};
#endif
}

class CrashClient {
 public:
  static CrashClient* GetInstance() {
    static base::NoDestructor<CrashClient> crash_client;
    return crash_client.get();
  }

  bool InitializeCrashReporting(
      std::optional<base::FilePath> crash_database_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    static bool initialized = false;
    CHECK(!initialized);
    initialized = true;

    if (!crash_database_path || !base::CreateDirectory(*crash_database_path)) {
      LOG(ERROR) << "Failed to get the database path.";
      return false;
    }

    database_ = crashpad::CrashReportDatabase::Initialize(*crash_database_path);
    if (!database_) {
      LOG(ERROR) << "Failed to initialize Crashpad database.";
      return false;
    }

    base::debug::SetDumpWithoutCrashingFunction(
        [] { CRASHPAD_SIMULATE_CRASH(); });

#if BUILDFLAG(IS_WIN)
    // Catch exceptions thrown from a window procedure.
    base::win::WinProcExceptionFilter exception_filter =
        base::win::SetWinProcExceptionFilter(&HandleWinProcException);
    LOG_IF(DFATAL, exception_filter) << "Exception filter already present";
#endif  // BUILDFLAG(IS_WIN)

    std::vector<crashpad::CrashReportDatabase::Report> reports_completed;
    const crashpad::CrashReportDatabase::OperationStatus status_completed =
        database_->GetCompletedReports(&reports_completed);
    if (status_completed == crashpad::CrashReportDatabase::kNoError) {
      VLOG(1) << "Found " << reports_completed.size()
              << " completed crash reports";
      for (const auto& report : reports_completed) {
        VLOG(3) << "Crash since last run: ID \"" << report.id
                << "\", created at " << report.creation_time << ", "
                << report.upload_attempts << " upload attempts, file path \""
                << report.file_path << "\", unique ID \""
                << report.uuid.ToString()
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

    if (ShouldEnableCrashUploads()) {
      VLOG(2) << "Crash uploading is enabled.";
      crashpad::Settings* crashpad_settings = database_->GetSettings();
      CHECK(crashpad_settings);
      LOG_IF(ERROR, !crashpad_settings->SetUploadsEnabled(true))
          << "Failed to set crash upload opt-in.";
    }

    return StartCrashReporter(*crash_database_path);
  }

 private:
  friend class base::NoDestructor<CrashClient>;

  CrashClient() = default;
  ~CrashClient() = default;

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<crashpad::CrashReportDatabase> database_;

  bool StartCrashReporter(base::FilePath database_path) {
    static base::NoDestructor<crashpad::CrashpadClient> client;
    static bool started = false;
    CHECK(!started);
    started = true;

    std::map<std::string, std::string> annotations;
    annotations["ver"] = kEnterpriseCompanionVersion;
    annotations["prod"] = CRASH_PRODUCT_NAME;

    // Save dereferenced memory from all registers on the crashing thread.
    // Crashpad saves up to 512 bytes per CPU register, and in the worst case,
    // ARM64 has 32 registers.
    constexpr uint32_t kIndirectMemoryLimit = 32 * 512;
    crashpad::CrashpadInfo::GetCrashpadInfo()
        ->set_gather_indirectly_referenced_memory(crashpad::TriState::kEnabled,
                                                  kIndirectMemoryLimit);
    std::vector<base::FilePath> attachments;
#if !BUILDFLAG(IS_MAC)  // Crashpad does not support attachments on macOS.
    std::optional<base::FilePath> log_file = GetLogFilePath();
    if (log_file) {
      attachments.push_back(*log_file);
    }
#endif
    if (!client->StartHandler(base::PathService::CheckedGet(base::FILE_EXE),
                              database_path,
                              /*metrics_dir=*/base::FilePath(),
                              GetGlobalConstants()->CrashUploadURL().spec(),
                              annotations, MakeCrashHandlerArgs(),
                              /*restartable=*/true,
                              /*asynchronous_start=*/false)) {
      VLOG(1) << "Failed to start handler.";
      return false;
    }

    VLOG(1) << "Crash handler launched and ready";
    return true;
  }
};

}  // namespace

std::optional<base::FilePath> GetDefaultCrashDatabasePath() {
  std::optional<base::FilePath> path = GetInstallDirectory();
  return path ? path->Append(FILE_PATH_LITERAL("Crashpad")) : path;
}

bool InitializeCrashReporting(
    std::optional<base::FilePath> crash_database_path) {
  return CrashClient::GetInstance()->InitializeCrashReporting(
      crash_database_path);
}

int CrashReporterMain() {
  base::CommandLine command_line = *base::CommandLine::ForCurrentProcess();
  CHECK(command_line.HasSwitch(kCrashHandlerSwitch));

  // Disable rate-limiting until this is fixed:
  //   https://bugs.chromium.org/p/crashpad/issues/detail?id=23
  command_line.AppendSwitch(kNoRateLimitSwitch);

  // Because of https://bugs.chromium.org/p/crashpad/issues/detail?id=82,
  // Crashpad fails on the presence of flags it doesn't handle.
  command_line.RemoveSwitch(kCrashHandlerSwitch);
  command_line.RemoveSwitch(kLoggingModuleSwitch);

  const std::vector<base::CommandLine::StringType> argv = command_line.argv();

  // |storage| must be declared before |argv_as_utf8|, to ensure it outlives
  // |argv_as_utf8|, which will hold pointers into |storage|.
  std::vector<std::string> storage;
  auto argv_as_utf8 = std::make_unique<char*[]>(argv.size() + 1);
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
#if BUILDFLAG(IS_WIN)
    storage.push_back(base::WideToUTF8(argv[i]));
#else
    storage.push_back(argv[i]);
#endif
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;

  return crashpad::HandlerMain(argv.size(), argv_as_utf8.get(),
                               /*user_stream_sources=*/nullptr);
}

}  // namespace enterprise_companion

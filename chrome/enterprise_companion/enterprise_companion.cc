// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/enterprise_companion/app/app.h"
#include "chrome/enterprise_companion/crash_client.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "chrome/enterprise_companion/flags.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/ipc_support.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#endif

namespace enterprise_companion {

namespace {

constexpr int64_t kLogRotateAtSize = 1024 * 1024;  // 1 MiB.

void InitLogging() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kLoggingModuleSwitch)) {
    command_line->AppendSwitchASCII(kLoggingModuleSwitch,
                                    kLoggingModuleSwitchValue);
  }

  logging::LoggingSettings settings{.logging_dest = logging::LOG_TO_STDERR};
  std::optional<base::FilePath> log_file_path = GetLogFilePath();
  if (!log_file_path) {
    VLOG(1) << "Error getting log file path.";
  } else {
    base::CreateDirectory(log_file_path->DirName());

    // Rotate the log if needed.
    std::optional<int64_t> size = base::GetFileSize(*log_file_path);
    if (size.has_value() && size.value() >= kLogRotateAtSize) {
      base::ReplaceFile(*log_file_path,
                        log_file_path->AddExtension(FILE_PATH_LITERAL(".old")),
                        nullptr);
    }
    settings.log_file_path = log_file_path->value().c_str();
    settings.logging_dest |= logging::LOG_TO_FILE;
  }

  logging::InitLogging(settings);
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);
}

void InitThreadPool() {
  base::PlatformThread::SetName("EnterpriseCompanion");
  base::ThreadPoolInstance::Create("EnterpriseCompanion");

  // Reuses the logic in base::ThreadPoolInstance::StartWithDefaultParams.
  const size_t max_num_foreground_threads =
      static_cast<size_t>(std::max(3, base::SysInfo::NumberOfProcessors() - 1));
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
  base::ThreadPoolInstance::Get()->Start(init_params);
}

std::unique_ptr<App> CreateAppForCommandLine(base::CommandLine* command_line) {
  if (command_line->HasSwitch(kShutdownSwitch)) {
    return CreateAppShutdown();
  }

  if (command_line->HasSwitch(kFetchPoliciesSwitch)) {
    return CreateAppFetchPolicies();
  }

  if (command_line->HasSwitch(kInstallSwitch)) {
    return CreateAppInstall();
  }

  if (command_line->HasSwitch(kUninstallSwitch)) {
    return CreateAppUninstall();
  }

#if BUILDFLAG(IS_MAC)
  if (command_line->HasSwitch(kNetWorkerSwitch)) {
    return CreateAppNetWorker();
  }
#endif

  return CreateAppServer();
}

constexpr const char* BuildFlavor() {
#if defined(NDEBUG)
  return "opt";
#else
  return "debug";
#endif
}

constexpr const char* BuildArch() {
#if defined(ARCH_CPU_64_BITS)
  return "64 bits";
#elif defined(ARCH_CPU_32_BITS)
  return "32 bits";
#else
#error CPU architecture is unknown.
#endif
}

std::string OperatingSystemVersion() {
#if BUILDFLAG(IS_WIN)
  const base::win::OSInfo::VersionNumber v =
      base::win::OSInfo::GetInstance()->version_number();
  return base::StringPrintf("%u.%u.%u.%u", v.major, v.minor, v.build, v.patch);
#else
  return base::SysInfo().OperatingSystemVersion();
#endif
}

}  // namespace

int EnterpriseCompanionMain(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  InitLogging();
  VLOG(1) << "Version: " << kEnterpriseCompanionVersion << ", " << BuildFlavor()
          << ", " << BuildArch()
          << ", command line: " << command_line->GetCommandLineString();
  VLOG(1) << "OS version: " << OperatingSystemVersion()
          << ", System uptime (seconds): "
          << base::SysInfo::Uptime().InSeconds() << ", parent pid: "
          << base::GetParentProcessId(base::GetCurrentProcessHandle());
  InitThreadPool();
  base::AtExitManager exit_manager;

  base::SingleThreadTaskExecutor main_task_executor;

  if (command_line->HasSwitch(kCrashHandlerSwitch)) {
    return CrashReporterMain();
  }
  InitializeCrashReporting();

  // Records a backtrace in the log, crashes the program, saves a crash dump,
  // and reports the crash.
  CHECK(!command_line->HasSwitch(kCrashMeSwitch))
      << kCrashMeSwitch << " switch was used.";

  ScopedIPCSupportWrapper ipc_support;

  EnterpriseCompanionStatus status =
      CreateAppForCommandLine(command_line)->Run();
  LOG_IF(ERROR, !status.ok())
      << "Application completed with error: " << status.description();
  return !status.ok();
}

std::optional<base::FilePath> GetLogFilePath() {
  std::optional<base::FilePath> path = GetInstallDirectory();
  return path ? path->Append(FILE_PATH_LITERAL("enterprise_companion.log"))
              : path;
}

}  // namespace enterprise_companion

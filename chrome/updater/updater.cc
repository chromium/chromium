// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include <iterator>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_install.h"
#include "chrome/updater/app/app_net_worker.h"
#include "chrome/updater/app/app_recover.h"
#include "chrome/updater/app/app_server.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/app/app_uninstall_self.h"
#include "chrome/updater/app/app_update.h"
#include "chrome/updater/app/app_wake.h"
#include "chrome/updater/app/app_wakeall.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/update_usage_stats_task.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"

#if BUILDFLAG(IS_WIN)
#include "base/debug/alias.h"
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "chrome/updater/app/server/win/service_main.h"
#include "chrome/updater/util/win_util.h"
#endif

// Instructions For Windows.
// - To install only the updater, run "updatersetup.exe" from the build out dir.
// - To install Chrome and the updater, do the same but use the --app-id:
//    updatersetup.exe --install --app-id={8A69D345-D564-463c-AFF1-A69D9E530F96}
// - To uninstall, run "updater.exe --uninstall" from its install directory,
// which is under %LOCALAPPDATA%\Google\GoogleUpdater, or from the |out|
// directory of the build.
// - To debug, append the following arguments to any updater command line:
//    --enable-logging --vmodule=*/chrome/updater/*=2,*/components/winhttp/*=2.

namespace updater {
namespace {

void ReinitializeLoggingAfterCrashHandler(UpdaterScope updater_scope) {
  // Initializing the logging more than two times is not supported. In this
  // case, logging has been initialized once in the updater main, and the
  // the second time by the crash handler.
  // Reinitializing the log is not possible if the vlog switch is
  // already present on the command line. The code in this function relies
  // on undocumented behavior of the logging object, and it could break.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(kLoggingModuleSwitch);
  InitLogging(updater_scope);
}

void InitializeCrashReporting(UpdaterScope updater_scope) {
  if (!CrashClient::GetInstance()->InitializeCrashReporting(updater_scope)) {
    VLOG(1) << "Crash reporting is not available.";
    return;
  }
  if (AreRawUsageStatsEnabled(updater_scope)) {
    CrashClient::GetInstance()->SetUploadsEnabled(true);
  }
  crash_reporter::InitializeCrashKeys();
  crash_keys::SetSwitchesFromCommandLine(
      *base::CommandLine::ForCurrentProcess(), nullptr);
  VLOG(1) << "Crash reporting initialized.";
}

int HandleUpdaterCommands(UpdaterScope updater_scope,
                          const base::CommandLine* command_line) {
  // Used for unit test purposes. There is no need to run with a crash handler.
  if (command_line->HasSwitch(kTestSwitch)) {
    return kErrorOk;
  }

  if (command_line->HasSwitch(kCrashHandlerSwitch)) {
    const int retval = CrashReporterMain();

    // The crash handler mutates the logging object, so the updater process
    // stops logging to the log file aftern `CrashReporterMain()` returns.
    ReinitializeLoggingAfterCrashHandler(updater_scope);
    return retval;
  }

  // Starts and connects to the external crash handler as early as possible.
  StartCrashReporter(updater_scope, kUpdaterVersion);

  InitializeCrashReporting(updater_scope);

  // Make the process more resilient to memory allocation issues.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();

  InitializeThreadPool("updater");
  const base::ScopedClosureRunner shutdown_thread_pool(base::BindOnce([] {
    // For the updater, it is important to join all threads before `UpdaterMain`
    // exits, otherwise the behavior of the program is undefined. The threads
    // in the pool can still run after shutdown to handle CONTINUE_ON_SHUTDOWN
    // tasks, for example. In Chrome, the thread pool is leaked for this reason
    // and there is no way to join its threads in production code. The updater
    // has no such requirements (crbug.com/1484776).
    base::ThreadPoolInstance* thread_pool = base::ThreadPoolInstance::Get();
    thread_pool->Shutdown();
    thread_pool->JoinForTesting();  // IN-TEST
    base::ThreadPoolInstance::Set(nullptr);
  }));

#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    PLOG(ERROR) << "Failed to initialize COM";
    return kErrorComInitializationFailed;
  }

  // Failing to disable COM exception handling is a critical error.
  CHECK(SUCCEEDED(DisableCOMExceptionHandling()))
      << "Failed to disable COM exception handling.";
  base::win::RegisterInvalidParamHandler();
  VLOG(1) << GetUACState();
#endif

  // Records a backtrace in the log, crashes the program, saves a crash dump,
  // and reports the crash.
  CHECK(!command_line->HasSwitch(kCrashMeSwitch)) << "--crash-me was used.";

  // As long as this object is alive, all Mojo API surface relevant to IPC
  // connections is usable, and message pipes which span a process boundary will
  // continue to function.
  ScopedIPCSupportWrapper ipc_support;

  // Only tasks and timers are supported on the main sequence.
  base::SingleThreadTaskExecutor main_task_executor;

  if (command_line->HasSwitch(kInstallSwitch) ||
      command_line->HasSwitch(kHandoffSwitch)) {
    return MakeAppInstall(command_line->HasSwitch(kSilentSwitch))->Run();
  }

  if (command_line->HasSwitch(kServerSwitch)) {
    return MakeAppServer()->Run();
  }

  if (command_line->HasSwitch(kUpdateSwitch)) {
    return MakeAppUpdate()->Run();
  }

#if BUILDFLAG(IS_WIN)
  if (command_line->HasSwitch(kWindowsServiceSwitch)) {
    return ServiceMain::RunWindowsService(command_line);
  }

  if (command_line->HasSwitch(kHealthCheckSwitch)) {
    return kErrorOk;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (command_line->HasSwitch(kUninstallSwitch) ||
      command_line->HasSwitch(kUninstallIfUnusedSwitch)) {
    return MakeAppUninstall()->Run();
  }

  if (command_line->HasSwitch(kUninstallSelfSwitch)) {
    return MakeAppUninstallSelf()->Run();
  }

  if (command_line->HasSwitch(kRecoverSwitch) ||
      command_line->HasSwitch(kBrowserVersionSwitch)) {
    return MakeAppRecover()->Run();
  }

  if (command_line->HasSwitch(kWakeSwitch)) {
    return MakeAppWake()->Run();
  }

  if (command_line->HasSwitch(kWakeAllSwitch)) {
    return MakeAppWakeAll()->Run();
  }

#if BUILDFLAG(IS_MAC)
  if (command_line->HasSwitch(kNetWorkerSwitch)) {
    return MakeAppNetWorker()->Run();
  }
#endif  // BUILDFLAG(IS_MAC)

  VLOG(1) << "Unknown command line switch.";
  return kErrorUnknownCommandLine;
}

// Returns the string literal corresponding to an updater command, which
// is present on the updater process command line. Returns an empty string
// if the command is not found.
const char* GetUpdaterCommand(const base::CommandLine* command_line) {
  // Contains the literals which are associated with specific updater commands.
  static constexpr const char* commands[] = {
      kWindowsServiceSwitch,
      kCrashHandlerSwitch,
      kInstallSwitch,
      kRecoverSwitch,
      kServerSwitch,
      kTestSwitch,
      kUninstallIfUnusedSwitch,
      kUninstallSelfSwitch,
      kUninstallSwitch,
      kUpdateSwitch,
      kWakeSwitch,
      kWakeAllSwitch,
      kHealthCheckSwitch,
      kHandoffSwitch,
      kNetWorkerSwitch,
  };
  const auto it = base::ranges::find_if(commands, [command_line](auto cmd) {
    return command_line->HasSwitch(cmd);
  });
  // Return the command. As a workaround for recovery component invocations
  // that do not pass --recover, report the browser version switch as --recover.
  return it != std::end(commands)
             ? *it
             : (command_line->HasSwitch(kBrowserVersionSwitch) ? kRecoverSwitch
                                                               : "");
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

base::CommandLine::StringType GetCommandLineString() {
#if BUILDFLAG(IS_WIN)
  // Gets the raw command line on Windows, because
  // `base::CommandLine::GetCommandLineString()` could return an invalid string
  // after the class re-arranges the legacy command line arguments.
  return ::GetCommandLine();
#else
  return base::CommandLine::ForCurrentProcess()->GetCommandLineString();
#endif
}

void EnableLoggingByDefault() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kEnableLoggingSwitch)) {
    command_line->AppendSwitch(kEnableLoggingSwitch);
  }
  if (!command_line->HasSwitch(kLoggingModuleSwitch)) {
    command_line->AppendSwitchASCII(kLoggingModuleSwitch,
                                    kLoggingModuleSwitchValue);
  }
}

}  // namespace

int UpdaterMain(int argc, const char* const* argv) {
#if BUILDFLAG(IS_WIN)
  CHECK(EnableSecureDllLoading());
  EnableProcessHeapMetadataProtection();
#endif

  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_WIN)
  *command_line = GetCommandLineLegacyCompatible();
#endif
  EnableLoggingByDefault();
  const UpdaterScope updater_scope = GetUpdaterScope();
  InitLogging(updater_scope);
  VLOG(1) << "Version: " << kUpdaterVersion << ", " << BuildFlavor() << ", "
          << BuildArch() << ", command line: " << GetCommandLineString();
  VLOG(1) << "OS version: " << OperatingSystemVersion()
          << ", System uptime (seconds): "
          << base::SysInfo::Uptime().InSeconds() << ", parent pid: "
          << base::GetParentProcessId(base::GetCurrentProcessHandle());
  const int exit_code = HandleUpdaterCommands(updater_scope, command_line);
  VLOG(1) << __func__ << " (--" << GetUpdaterCommand(command_line) << ")"
          << " returned " << exit_code << ".";

#if BUILDFLAG(IS_WIN)
  base::AtExitManager::ProcessCallbacksNow();

  ::SetLastError(ERROR_SUCCESS);
  const bool terminate_result =
      ::TerminateProcess(::GetCurrentProcess(), static_cast<UINT>(exit_code));

  // Capture error information in case TerminateProcess fails so that it may be
  // found in a post-return crash dump if the process crashes on exit.
  const DWORD terminate_error_code = ::GetLastError();
  DWORD exit_codes[] = {
      0xDEADBECF,
      static_cast<DWORD>(exit_code),
      static_cast<DWORD>(terminate_result),
      terminate_error_code,
      0xDEADBEDF,
  };
  base::debug::Alias(exit_codes);
#endif  // BUILDFLAG(IS_WIN)

  return exit_code;
}

}  // namespace updater

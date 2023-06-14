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
#include "base/message_loop/message_pump_type.h"
#include "base/process/memory.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_install.h"
#include "chrome/updater/app/app_recover.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/app/app_uninstall_self.h"
#include "chrome/updater/app/app_update.h"
#include "chrome/updater/app/app_wake.h"
#include "chrome/updater/app/app_wakeall.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_POSIX)
#include "chrome/updater/ipc/ipc_support.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/process_startup_helper.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/server/win/server.h"
#include "chrome/updater/app/server/win/service_main.h"
#include "chrome/updater/util/win_util.h"
#elif BUILDFLAG(IS_POSIX)
#include "chrome/updater/app/server/posix/app_server_posix.h"
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

void InitializeCrashReporting(UpdaterScope updater_scope,
                              const base::CommandLine& command_line) {
  crash_reporter::InitializeCrashKeys();
  static crash_reporter::CrashKeyString<16> crash_key_process_type(
      "process_type");
  crash_key_process_type.Set("updater");
  crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
  if (!CrashClient::GetInstance()->InitializeCrashReporting(updater_scope)) {
    VLOG(1) << "Crash reporting is not available.";
    return;
  }
  VLOG(1) << "Crash reporting initialized.";
}

int HandleUpdaterCommands(UpdaterScope updater_scope,
                          const base::CommandLine* command_line) {
  // Used for unit test purposes. There is no need to run with a crash handler.
  if (command_line->HasSwitch(kTestSwitch))
    return kErrorOk;

  if (command_line->HasSwitch(kCrashHandlerSwitch)) {
    const int retval = CrashReporterMain();

    // The crash handler mutates the logging object, so the updater process
    // stops logging to the log file aftern `CrashReporterMain()` returns.
    ReinitializeLoggingAfterCrashHandler(updater_scope);
    return retval;
  }

  // Starts and connects to the external crash handler as early as possible.
  StartCrashReporter(updater_scope, kUpdaterVersion);

  InitializeCrashReporting(updater_scope, *command_line);

  // Make the process more resilient to memory allocation issues.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();

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

  InitializeThreadPool("updater");
  const base::ScopedClosureRunner shutdown_thread_pool(
      base::BindOnce([] { base::ThreadPoolInstance::Get()->Shutdown(); }));
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  // Records a backtrace in the log, crashes the program, saves a crash dump,
  // and reports the crash.
  CHECK(!command_line->HasSwitch(kCrashMeSwitch)) << "--crash-me was used.";

#if BUILDFLAG(IS_POSIX)
  // As long as this object is alive, all Mojo API surface relevant to IPC
  // connections is usable, and message pipes which span a process boundary will
  // continue to function.
  ScopedIPCSupportWrapper ipc_support;
#endif

  if (command_line->HasSwitch(kServerSwitch)) {
#if BUILDFLAG(IS_WIN)
    // By design, Windows uses a leaky singleton server for its RPC server.
    return AppServerSingletonInstance()->Run();
#else
    return MakeAppServer()->Run();
#endif
  }

  if (command_line->HasSwitch(kUpdateSwitch))
    return MakeAppUpdate()->Run();

#if BUILDFLAG(IS_WIN)
  if (command_line->HasSwitch(kWindowsServiceSwitch))
    return ServiceMain::RunWindowsService(command_line);

  if (command_line->HasSwitch(kHealthCheckSwitch)) {
    return kErrorOk;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (command_line->HasSwitch(kInstallSwitch) ||
      command_line->HasSwitch(kTagSwitch) ||
      command_line->HasSwitch(kRuntimeSwitch) ||
      command_line->HasSwitch(kHandoffSwitch)) {
    return MakeAppInstall(command_line->HasSwitch(kSilentSwitch))->Run();
  }

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

  VLOG(1) << "Unknown command line switch.";
  return kErrorUnknownCommandLine;
}

// Returns the string literal corresponding to an updater command, which
// is present on the updater process command line. Returns an empty string
// if the command is not found.
const char* GetUpdaterCommand(const base::CommandLine* command_line) {
  // Contains the literals which are associated with specific updater commands.
  const char* commands[] = {
      kWindowsServiceSwitch, kCrashHandlerSwitch,
      kInstallSwitch,        kRecoverSwitch,
      kServerSwitch,         kTagSwitch,
      kTestSwitch,           kUninstallIfUnusedSwitch,
      kUninstallSelfSwitch,  kUninstallSwitch,
      kUpdateSwitch,         kWakeSwitch,
      kWakeAllSwitch,        kHealthCheckSwitch,
      kHandoffSwitch,        kRuntimeSwitch,
  };
  const char** it = base::ranges::find_if(commands, [command_line](auto cmd) {
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

}  // namespace

int UpdaterMain(int argc, const char* const* argv) {
#if BUILDFLAG(IS_WIN)
  CHECK(EnableSecureDllLoading());
  EnableProcessHeapMetadataProtection();
#endif

  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  const UpdaterScope updater_scope = GetUpdaterScope();
  InitLogging(updater_scope);

  VLOG(1) << "Version " << kUpdaterVersion << ", " << BuildFlavor() << ", "
          << BuildArch() << ", command line: " << GetCommandLineString();
  const int retval = HandleUpdaterCommands(updater_scope, command_line);
  VLOG(1) << __func__ << " (--" << GetUpdaterCommand(command_line) << ")"
          << " returned " << retval << ".";
  return retval;
}

}  // namespace updater

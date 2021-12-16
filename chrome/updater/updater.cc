// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include <algorithm>
#include <iterator>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/memory.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/app_install.h"
#include "chrome/updater/app/app_uninstall.h"
#include "chrome/updater/app/app_update.h"
#include "chrome/updater/app/app_wake.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if defined(OS_WIN)
#include "base/win/process_startup_helper.h"
#include "chrome/updater/app/server/win/server.h"
#include "chrome/updater/app/server/win/service_main.h"
#include "chrome/updater/win/win_util.h"
#elif defined(OS_MAC)
#include "chrome/updater/app/server/mac/server.h"
#elif defined(OS_LINUX)
#include "chrome/updater/app/server/linux/server.h"
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
// - To run the `updater --install` from the `out` directory of the build,
//   use --install-from-out-dir command line switch in addition to other
//   arguments for --install.

namespace updater {
namespace {

// The log file is created in DIR_LOCAL_APP_DATA or DIR_ROAMING_APP_DATA.
void InitLogging(UpdaterScope updater_scope) {
  logging::LoggingSettings settings;
  const absl::optional<base::FilePath> log_dir =
      GetBaseDirectory(updater_scope);
  if (!log_dir) {
    LOG(ERROR) << "Error getting base dir.";
    return;
  }
  const auto log_file = log_dir->Append(FILE_PATH_LITERAL("updater.log"));
  settings.log_file_path = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // enable_process_id
                       true,    // enable_thread_id
                       true,    // enable_timestamp
                       false);  // enable_tickcount
}

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
  crash_reporter::InitializeCrashKeys();
  static crash_reporter::CrashKeyString<16> crash_key_process_type(
      "process_type");
  crash_key_process_type.Set("updater");
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
    return 0;

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
#if defined(OS_WIN)
  base::win::RegisterInvalidParamHandler();

  VLOG(1) << GetUACState();
#endif

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  if (command_line->HasSwitch(kCrashMeSwitch)) {
    // Records a backtrace in the log, crashes the program, saves a crash dump,
    // and reports the crash.
    CHECK(false) << "--crash-me was used.";
  }

  if (command_line->HasSwitch(kServerSwitch)) {
#if defined(OS_WIN)
    // By design, Windows uses a leaky singleton server for its RPC server.
    return AppServerSingletonInstance()->Run();
#else
    return MakeAppServer()->Run();
#endif
  }

  if (command_line->HasSwitch(kUpdateSwitch))
    return MakeAppUpdate()->Run();

#if defined(OS_WIN)
  if (command_line->HasSwitch(kComServiceSwitch))
    return ServiceMain::RunComService(command_line);
#endif  // OS_WIN

  if (command_line->HasSwitch(kInstallSwitch) ||
      command_line->HasSwitch(kTagSwitch)) {
    return MakeAppInstall()->Run();
  }

  if (command_line->HasSwitch(kUninstallSwitch) ||
      command_line->HasSwitch(kUninstallSelfSwitch) ||
      command_line->HasSwitch(kUninstallIfUnusedSwitch)) {
    return MakeAppUninstall()->Run();
  }

  if (command_line->HasSwitch(kWakeSwitch)) {
    return MakeAppWake()->Run();
  }

  VLOG(1) << "Unknown command line switch.";
  return -1;
}

// Returns the string literal corresponding to an updater command, which
// is present on the updater process command line. Returns an empty string
// if the command is not found.
const char* GetUpdaterCommand(const base::CommandLine* command_line) {
  // Contains the literals which are associated with specific updater commands.
  const char* commands[] = {
      kComServiceSwitch,
      kCrashHandlerSwitch,
      kInstallSwitch,
      kServerSwitch,
      kTagSwitch,
      kTestSwitch,
      kUninstallIfUnusedSwitch,
      kUninstallSelfSwitch,
      kUninstallSwitch,
      kUpdateSwitch,
      kWakeSwitch,
  };
  const char** it = std::find_if(
      std::begin(commands), std::end(commands),
      [command_line](auto cmd) { return command_line->HasSwitch(cmd); });
  return it != std::end(commands) ? *it : "";
}

}  // namespace

int UpdaterMain(int argc, const char* const* argv) {
  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  const UpdaterScope updater_scope = GetUpdaterScope();
  InitLogging(updater_scope);

  VLOG(1) << "Version " << kUpdaterVersion
          << ", command line: " << command_line->GetCommandLineString();
  const int retval = HandleUpdaterCommands(updater_scope, command_line);
  VLOG(1) << __func__ << " (--" << GetUpdaterCommand(command_line) << ")"
          << " returned " << retval << ".";
  return retval;
}

}  // namespace updater

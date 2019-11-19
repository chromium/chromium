// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "chrome/updater/crash_client.h"
#include "chrome/updater/crash_reporter.h"
#include "chrome/updater/update_apps.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
#include "components/crash/core/common/crash_key.h"

#if defined(OS_WIN)
#include "chrome/updater/win/install_app.h"
#include "chrome/updater/win/setup/uninstall.h"
#endif

// To install the updater on Windows, run "updatersetup.exe" from the
// build directory.
//
// To uninstall, run "updater.exe --uninstall" from its install directory,
// which is under %LOCALAPPDATA%\Google\GoogleUpdater, or from the |out|
// directory of the build.
//
// To debug, use the command line arguments:
//    --enable-logging --vmodule=*/chrome/updater/*=2.

namespace updater {

namespace {

void ThreadPoolStart() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Updater");
}

void ThreadPoolStop() {
  base::ThreadPoolInstance::Get()->Shutdown();
}

// The log file is created in DIR_LOCAL_APP_DATA or DIR_APP_DATA.
void InitLogging(const base::CommandLine& command_line) {
  logging::LoggingSettings settings;
  base::FilePath log_dir;
  GetProductDirectory(&log_dir);
  const auto log_file = log_dir.Append(FILE_PATH_LITERAL("updater.log"));
  settings.log_file_path = log_file.value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(true,    // enable_process_id
                       true,    // enable_thread_id
                       true,    // enable_timestamp
                       false);  // enable_tickcount
  VLOG(1) << "Log file " << settings.log_file_path;
}

void InitializeUpdaterMain() {
  crash_reporter::InitializeCrashKeys();

  static crash_reporter::CrashKeyString<16> crash_key_process_type(
      "process_type");
  crash_key_process_type.Set("updater");

  if (CrashClient::GetInstance()->InitializeCrashReporting())
    VLOG(1) << "Crash reporting initialized.";
  else
    VLOG(1) << "Crash reporting is not available.";

  StartCrashReporter(UPDATER_VERSION_STRING);

  ThreadPoolStart();
}

void TerminateUpdaterMain() {
  ThreadPoolStop();
}

int UpdaterUpdateApps() {
  return UpdateApps();
}

int UpdaterInstallApp() {
#if defined(OS_WIN)
  // TODO(sorin): pick up the app id from the tag. https://crbug.com/1014298
  return InstallApp({kChromeAppId});
#else
  NOTREACHED();
  return -1;
#endif
}

int UpdaterUninstall() {
#if defined(OS_WIN)
  return Uninstall();
#else
  return -1;
#endif
}

}  // namespace

int HandleUpdaterCommands(const base::CommandLine* command_line) {
  DCHECK(!command_line->HasSwitch(kCrashHandlerSwitch));

  if (command_line->HasSwitch(kCrashMeSwitch)) {
    int* ptr = nullptr;
    return *ptr;
  }

  if (command_line->HasSwitch(kInstallSwitch))
    return UpdaterInstallApp();

  if (command_line->HasSwitch(kUninstallSwitch))
    return UpdaterUninstall();

  if (command_line->HasSwitch(kUpdateAppsSwitch))
    return UpdaterUpdateApps();

  VLOG(1) << "Unknown command line switch.";
  return -1;
}

int UpdaterMain(int argc, const char* const* argv) {
  base::PlatformThread::SetName("UpdaterMain");
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kTestSwitch))
    return 0;

  InitLogging(*command_line);

  if (command_line->HasSwitch(kCrashHandlerSwitch))
    return CrashReporterMain();

  InitializeUpdaterMain();
  const auto result = HandleUpdaterCommands(command_line);
  TerminateUpdaterMain();
  return result;
}

}  // namespace updater

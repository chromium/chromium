// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/system_tracing_util.h"

#include "base/path_service.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/install_util.h"

namespace installer {

namespace {

base::CommandLine MakeCommand(bool register_service) {
  base::FilePath setup_exe =
      InstallUtil::GetChromeUninstallCmd(/*system_level=*/true).GetProgram();
  if (setup_exe.empty()) {
    return base::CommandLine(base::CommandLine::NO_PROGRAM);
  }
  base::CommandLine command(setup_exe);
  command.AppendSwitch(register_service ? switches::kEnableSystemTracing
                                        : switches::kDisableSystemTracing);
  command.AppendSwitch(switches::kSystemLevel);
  command.AppendSwitch(switches::kVerboseLogging);
  InstallUtil::AppendModeAndChannelSwitches(&command);
  return command;
}

bool DoServiceRegistration(bool register_service) {
  if (!IsSystemTracingServiceSupported()) {
    return false;
  }

  DWORD exit_code = 0;
  return InstallUtil::ExecuteExeAsAdmin(MakeCommand(register_service),
                                        &exit_code) &&
         exit_code == INSTALL_REPAIRED;
}

}  // namespace

bool IsSystemTracingServiceSupported() {
  // Only supported for browsers of per-machine installs.
  return !InstallUtil::IsPerUserInstall() && IsCurrentProcessInstalled();
}

bool IsSystemTracingServiceRegistered() {
  return !InstallUtil::IsPerUserInstall() &&
         InstallServiceWorkItem::IsComServiceInstalled(
             install_static::GetTracingServiceClsid());
}

bool ElevateAndRegisterSystemTracingService() {
  return DoServiceRegistration(/*register_service=*/true);
}

bool ElevateAndDeregisterSystemTracingService() {
  return DoServiceRegistration(/*register_service=*/false);
}

}  // namespace installer

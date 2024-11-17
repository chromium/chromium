// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/updater_service_delegate.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "chrome/updater/app/app_server_win.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/windows_services/service_program/service.h"

namespace updater {

namespace {

HRESULT RunWakeTask() {
  base::CommandLine run_updater_wake_command(
      base::CommandLine::ForCurrentProcess()->GetProgram());
  run_updater_wake_command.AppendSwitch(kWakeSwitch);
  run_updater_wake_command.AppendSwitch(kSystemSwitch);
  VLOG(2) << "Launching Wake command: "
          << run_updater_wake_command.GetCommandLineString();

  base::LaunchOptions options;
  options.start_hidden = true;
  const base::Process process =
      base::LaunchProcess(run_updater_wake_command, options);
  return process.IsValid() ? S_OK : HRESULTFromLastError();
}

}  // namespace

int UpdaterServiceDelegate::RunWindowsService() {
  UpdaterServiceDelegate delegate;
  return Service(delegate).Start();
}

UpdaterServiceDelegate::UpdaterServiceDelegate() = default;
UpdaterServiceDelegate::~UpdaterServiceDelegate() = default;

bool UpdaterServiceDelegate::PreRun() {
  return true;  // This delegate implements `Run()`.
}

void UpdaterServiceDelegate::OnServiceControlStop() {
  GetAppServerWinInstance()->Stop();
}

HRESULT UpdaterServiceDelegate::Run(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kComServiceSwitch)) {
    VLOG(2) << "Running COM server within the Windows Service";
    return RunCOMServer();
  }

  if (IsInternalService()) {
    VLOG(2) << "Running Wake task from the Windows Service";
    return RunWakeTask();
  }

  return S_OK;
}

HRESULT UpdaterServiceDelegate::RunCOMServer() {
  return GetAppServerWinInstance()->Run();
}

}  // namespace updater

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/version.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/update_service_impl_inactive.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/update_service_internal_impl.h"
#include "chrome/updater/update_service_internal_impl_inactive.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "components/prefs/pref_service.h"

namespace updater {

namespace {

bool IsInternalService() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             kServerServiceSwitch) == kServerUpdateServiceInternalSwitchValue;
}

}  // namespace

AppServer::AppServer() = default;

AppServer::~AppServer() = default;

void AppServer::Initialize() {
  first_task_ = ModeCheck();
}

base::OnceClosure AppServer::ModeCheck() {
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  if (!global_prefs) {
    return base::BindOnce(&AppServer::Shutdown, this,
                          kErrorFailedToLockPrefsMutex);
  }

  const base::Version this_version(UPDATER_VERSION_STRING);
  const base::Version active_version(global_prefs->GetActiveVersion());

  VLOG(2) << "This version: " << this_version.GetString()
          << ", active version: " << active_version.GetString();

  if (this_version < active_version) {
    global_prefs = nullptr;
    uninstall_self_ = true;
    if (IsInternalService()) {
      return base::BindOnce(&AppServer::ActiveDutyInternal, this,
                            MakeInactiveUpdateServiceInternal());
    }
    return base::BindOnce(&AppServer::ActiveDuty, this,
                          MakeInactiveUpdateService());
  }

  if (active_version != base::Version("0") && active_version != this_version) {
    std::unique_ptr<LocalPrefs> local_prefs = CreateLocalPrefs();
    if (!local_prefs->GetQualified()) {
      global_prefs = nullptr;
      return base::BindOnce(&AppServer::Qualify, this, std::move(local_prefs));
    }
  }

  if (this_version > active_version || global_prefs->GetSwapping()) {
    if (!SwapVersions(global_prefs.get()))
      return base::BindOnce(&AppServer::Shutdown, this, kErrorFailedToSwap);
  }

  if (IsInternalService()) {
    return base::BindOnce(&AppServer::ActiveDutyInternal, this,
                          base::MakeRefCounted<UpdateServiceInternalImpl>());
  }
  config_ = base::MakeRefCounted<Configurator>(std::move(global_prefs));
  return base::BindOnce(&AppServer::ActiveDuty, this,
                        base::MakeRefCounted<UpdateServiceImpl>(config_));
}

void AppServer::Uninitialize() {
  if (config_)
    PrefsCommitPendingWrites(config_->GetPrefService());
  if (uninstall_self_) {
    VLOG(1) << "Uninstalling version " << UPDATER_VERSION_STRING;
    UninstallSelf();
  } else {
    MaybeUninstall();
  }
}

void AppServer::MaybeUninstall() {
  if (!config_)
    return;

  auto persisted_data =
      base::MakeRefCounted<PersistedData>(config_->GetPrefService());
  const std::vector<std::string> app_ids = persisted_data->GetAppIds();
  if (app_ids.size() == 1 && base::Contains(app_ids, kUpdaterAppId)) {
    base::CommandLine command_line(
        base::CommandLine::ForCurrentProcess()->GetProgram());
    command_line.AppendSwitch(kUninstallIfUnusedSwitch);
    if (updater_scope() == UpdaterScope::kSystem)
      command_line.AppendSwitch(kSystemSwitch);
    command_line.AppendSwitch("--enable-logging");
    command_line.AppendSwitchASCII("--vmodule", "*/updater/*=2");
    DVLOG(2) << "Launching uninstall command: "
             << command_line.GetCommandLineString();

    base::Process process = base::LaunchProcess(command_line, {});
    if (!process.IsValid()) {
      DVLOG(2) << "Invalid process launching command: "
               << command_line.GetCommandLineString();
    }
  }
}

void AppServer::FirstTaskRun() {
  std::move(first_task_).Run();
}

void AppServer::Qualify(std::unique_ptr<LocalPrefs> local_prefs) {
  // For now, assume qualification succeeds.
  DVLOG(2) << __func__;
  local_prefs->SetQualified(true);
  PrefsCommitPendingWrites(local_prefs->GetPrefService());

  // Start ActiveDuty with inactive service implementations. To use active
  // implementations, the server would have to ModeCheck again.
  if (IsInternalService()) {
    ActiveDutyInternal(MakeInactiveUpdateServiceInternal());
  } else {
    ActiveDuty(MakeInactiveUpdateService());
  }
}

bool AppServer::SwapVersions(GlobalPrefs* global_prefs) {
  global_prefs->SetSwapping(true);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  bool result = SwapRPCInterfaces();
  if (!result)
    return false;
  global_prefs->SetActiveVersion(UPDATER_VERSION_STRING);
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  return true;
}

}  // namespace updater

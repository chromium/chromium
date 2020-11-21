// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
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
#include "chrome/updater/updater_version.h"
#include "components/prefs/pref_service.h"

namespace updater {

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
    uninstall_ = true;
    return base::BindOnce(&AppServer::ActiveDuty, this,
                          MakeInactiveUpdateService(),
                          MakeInactiveUpdateServiceInternal());
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

  config_ = base::MakeRefCounted<Configurator>(std::move(global_prefs));
  return base::BindOnce(
      &AppServer::ActiveDuty, this,
      base::MakeRefCounted<UpdateServiceImpl>(config_),
      base::MakeRefCounted<UpdateServiceInternalImpl>(config_));
}

void AppServer::Uninitialize() {
  if (config_)
    PrefsCommitPendingWrites(config_->GetPrefService());
  if (uninstall_) {
    VLOG(1) << "Uninstalling version " << UPDATER_VERSION_STRING;
    UninstallSelf();
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
  ActiveDuty(MakeInactiveUpdateService(), MakeInactiveUpdateServiceInternal());
}

bool AppServer::SwapVersions(GlobalPrefs* global_prefs) {
  global_prefs->SetSwapping(true);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  bool result = SwapRPCInterfaces();
  if (!result)
    return false;
  global_prefs->SetActiveVersion(UPDATER_VERSION_STRING);
  scoped_refptr<PersistedData> persisted_data =
      base::MakeRefCounted<PersistedData>(global_prefs->GetPrefService());
  if (!persisted_data->GetProductVersion(kUpdaterAppId).IsValid()) {
    persisted_data->SetProductVersion(kUpdaterAppId,
                                      base::Version(UPDATER_VERSION_STRING));
  }
  global_prefs->SetSwapping(false);
  PrefsCommitPendingWrites(global_prefs->GetPrefService());
  return true;
}

}  // namespace updater

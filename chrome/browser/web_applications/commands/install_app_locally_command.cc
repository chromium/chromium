// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_app_locally_command.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

InstallAppLocallyCommand::InstallAppLocallyCommand(
    const webapps::AppId& app_id,
    base::OnceClosure install_callback)
    : WebAppCommand<AppLock>("InstallAppLocallyCommand",
                             AppLockDescription(app_id),
                             std::move(install_callback)),
      app_id_(app_id) {
  GetMutableDebugValue().Set("app_id", app_id_);
}

InstallAppLocallyCommand::~InstallAppLocallyCommand() = default;

void InstallAppLocallyCommand::StartWithLock(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  if (app_lock_->registrar().IsNotInRegistrar(app_id_)) {
    GetMutableDebugValue().Set("command_result", "app_not_in_registry");
    CompleteAndSelfDestruct(CommandResult::kSuccess);
    return;
  }

  // Setting app to be installed with OS integration before calling
  // Synchronize() helps trigger the OS integration.
  if (!app_lock_->registrar().IsInstallState(
          app_id_, {proto::INSTALLED_WITH_OS_INTEGRATION})) {
    ScopedRegistryUpdate update = app_lock_->sync_bridge().BeginUpdate();
    WebApp* web_app_to_update = update->UpdateApp(app_id_);
    if (web_app_to_update) {
      web_app_to_update->SetInstallState(
          proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    }
  }

  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = true;
  synchronize_options.add_to_quick_launch_bar = false;
  synchronize_options.reason = SHORTCUT_CREATION_BY_USER;
  app_lock_->os_integration_manager().Synchronize(
      app_id_,
      base::BindOnce(&InstallAppLocallyCommand::OnOsIntegrationSynchronized,
                     weak_factory_.GetWeakPtr()),
      synchronize_options);
}

void InstallAppLocallyCommand::OnOsIntegrationSynchronized() {
  const base::Time& install_time = base::Time::Now();
  {
    // Updating install time on app.
    ScopedRegistryUpdate update = app_lock_->sync_bridge().BeginUpdate();
    WebApp* web_app_to_update = update->UpdateApp(app_id_);
    if (web_app_to_update) {
      web_app_to_update->SetFirstInstallTime(install_time);
    }
  }

  app_lock_->install_manager().NotifyWebAppInstalledWithOsHooks(app_id_);
  app_lock_->registrar().NotifyWebAppFirstInstallTimeChanged(app_id_,
                                                             install_time);
  GetMutableDebugValue().Set("command_result", "success");
  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app

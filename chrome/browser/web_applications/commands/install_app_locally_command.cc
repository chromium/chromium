// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_app_locally_command.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
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
    : WebAppCommandTemplate<AppLock>("InstallAppLocallyCommand"),
      app_lock_description_(std::make_unique<AppLockDescription>(app_id)),
      app_id_(app_id),
      install_callback_(std::move(install_callback)) {
  debug_log_.Set("app_id", app_id_);
}

InstallAppLocallyCommand::~InstallAppLocallyCommand() = default;

const LockDescription& InstallAppLocallyCommand::lock_description() const {
  return *app_lock_description_;
}

void InstallAppLocallyCommand::StartWithLock(
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  if (!app_lock_->registrar().IsInstalled(app_id_)) {
    debug_log_.Set("command_result", "app_not_in_registry");
    ReportResultAndShutdown(CommandResult::kSuccess);
    return;
  }

  // Setting app to be locally installed before calling Synchronize() helps
  // trigger the OS integration.
  if (!app_lock_->registrar().IsLocallyInstalled(app_id_)) {
    ScopedRegistryUpdate update = app_lock_->sync_bridge().BeginUpdate();
    WebApp* web_app_to_update = update->UpdateApp(app_id_);
    if (web_app_to_update) {
      web_app_to_update->SetIsLocallyInstalled(/*is_locally_installed=*/true);
    }
  }

  // Install OS hooks first.
  InstallOsHooksOptions options;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = false;
  options.os_hooks[OsHookType::kShortcuts] = true;
  options.os_hooks[OsHookType::kShortcutsMenu] = true;
  options.os_hooks[OsHookType::kFileHandlers] = true;
  options.os_hooks[OsHookType::kProtocolHandlers] = true;
  options.os_hooks[OsHookType::kRunOnOsLogin] =
      (app_lock_->registrar().GetAppRunOnOsLoginMode(app_id_).value ==
       RunOnOsLoginMode::kWindowed);

  // Installed WebApp here is user uninstallable app, but it needs to
  // check user uninstall-ability if there are apps with different source types.
  // WebApp::CanUserUninstallApp will handles it.
  const web_app::WebApp* web_app = app_lock_->registrar().GetAppById(app_id_);
  options.os_hooks[OsHookType::kUninstallationViaOsSettings] =
      web_app->CanUserUninstallWebApp();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  options.os_hooks[web_app::OsHookType::kUrlHandlers] = true;
#else
  options.os_hooks[web_app::OsHookType::kUrlHandlers] = false;
#endif

  auto os_hooks_barrier = OsIntegrationManager::GetBarrierForSynchronize(
      base::BindOnce(&InstallAppLocallyCommand::OnOsHooksInstalled,
                     weak_factory_.GetWeakPtr()));

  app_lock_->os_integration_manager().InstallOsHooks(
      app_id_, os_hooks_barrier, /*web_app_info=*/nullptr, options);

  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = options.add_to_desktop;
  synchronize_options.add_to_quick_launch_bar = options.add_to_quick_launch_bar;
  synchronize_options.reason = options.reason;
  app_lock_->os_integration_manager().Synchronize(
      app_id_, base::BindOnce(os_hooks_barrier, OsHooksErrors()),
      synchronize_options);
}

void InstallAppLocallyCommand::OnOsHooksInstalled(
    const OsHooksErrors os_hooks_errors) {
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
  debug_log_.Set("command_result", "success");
  ReportResultAndShutdown(CommandResult::kSuccess);
}

void InstallAppLocallyCommand::OnShutdown() {
  ReportResultAndShutdown(CommandResult::kShutdown);
}

base::Value InstallAppLocallyCommand::ToDebugValue() const {
  base::Value::Dict value = debug_log_.Clone();
  return base::Value(std::move(value));
}

void InstallAppLocallyCommand::ReportResultAndShutdown(CommandResult result) {
  DCHECK(!install_callback_.is_null());
  SignalCompletionAndSelfDestruct(result, std::move(install_callback_));
}

}  // namespace web_app

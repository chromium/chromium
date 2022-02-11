// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"

#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

namespace {

void UpdateRunOnOsLoginOsIntegration(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    const AppId& app_id,
    RunOnOsLoginMode effective_mode) {
  DCHECK(registrar);
  DCHECK(os_integration_manager);

  absl::optional<RunOnOsLoginMode> os_integration_state =
      registrar->GetExpectedRunOnOsLoginOsIntegrationState(app_id);

  if (os_integration_state && effective_mode == *os_integration_state) {
    return;
  }

  if (effective_mode == RunOnOsLoginMode::kNotRun) {
    web_app::OsHooksOptions os_hooks;
    os_hooks[web_app::OsHookType::kRunOnOsLogin] = true;
    os_integration_manager->UninstallOsHooks(app_id, os_hooks,
                                             base::DoNothing());
  } else {
    web_app::InstallOsHooksOptions install_options;
    install_options.os_hooks[web_app::OsHookType::kRunOnOsLogin] = true;
    os_integration_manager->InstallOsHooks(app_id, base::DoNothing(), nullptr,
                                           std::move(install_options));
  }
}

}  // namespace

void PersistRunOnOsLoginUserChoice(WebAppProvider* provider,
                                   const AppId& app_id,
                                   RunOnOsLoginMode new_user_mode) {
  DCHECK(provider);

  if (!provider->registrar().IsInstalled(app_id)) {
    return;
  }

  const auto current_mode =
      provider->registrar().GetAppRunOnOsLoginMode(app_id);

  // Early return if policy does not allow the user to change value, or if the
  // new value is the same as the old value.
  if (!current_mode.user_controllable || new_user_mode == current_mode.value) {
    return;
  }

  provider->sync_bridge().SetAppRunOnOsLoginMode(app_id, new_user_mode);
  UpdateRunOnOsLoginOsIntegration(&provider->registrar(),
                                  &provider->os_integration_manager(), app_id,
                                  new_user_mode);
}

void SyncRunOnOsLoginOsIntegrationState(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    const AppId& app_id) {
  DCHECK(registrar);
  DCHECK(os_integration_manager);
  RunOnOsLoginMode effective_mode =
      registrar->GetAppRunOnOsLoginMode(app_id).value;
  UpdateRunOnOsLoginOsIntegration(registrar, os_integration_manager, app_id,
                                  effective_mode);
}

}  // namespace web_app

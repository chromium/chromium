// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"

#include "base/no_destructor.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

namespace {

base::OnceClosure& GetRunOnOsLoginOsHooksChangedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

void MaybeCallTestingCallback(OsHooksErrors errors) {
  if (GetRunOnOsLoginOsHooksChangedCallbackForTesting()) {  // IN-TEST
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        std::move(
            GetRunOnOsLoginOsHooksChangedCallbackForTesting()));  // IN-TEST
  }
}

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
    MaybeCallTestingCallback(OsHooksErrors());
    return;
  }

  if (effective_mode == RunOnOsLoginMode::kNotRun) {
    web_app::OsHooksOptions os_hooks;
    os_hooks[web_app::OsHookType::kRunOnOsLogin] = true;
    os_integration_manager->UninstallOsHooks(
        app_id, os_hooks, base::BindOnce(&MaybeCallTestingCallback));
  } else {
    web_app::InstallOsHooksOptions install_options;
    install_options.os_hooks[web_app::OsHookType::kRunOnOsLogin] = true;
    os_integration_manager->InstallOsHooks(
        app_id, base::BindOnce(&MaybeCallTestingCallback), nullptr,
        std::move(install_options));
  }
}

}  // namespace

void SetRunOnOsLoginOsHooksChangedCallbackForTesting(  // IN-TEST
    base::OnceClosure callback) {
  GetRunOnOsLoginOsHooksChangedCallbackForTesting() =  // IN-TEST
      std::move(callback);
}

void PersistRunOnOsLoginUserChoice(WebAppRegistrar* registrar,
                                   OsIntegrationManager* os_integration_manager,
                                   WebAppSyncBridge* sync_bridge,
                                   const AppId& app_id,
                                   RunOnOsLoginMode new_user_mode) {
  DCHECK(registrar);
  DCHECK(os_integration_manager);
  DCHECK(sync_bridge);

  if (!registrar->IsLocallyInstalled(app_id)) {
    MaybeCallTestingCallback(OsHooksErrors());
    return;
  }

  const auto current_mode = registrar->GetAppRunOnOsLoginMode(app_id);

  // Early return if policy does not allow the user to change value, or if the
  // new value is the same as the old value.
  if (!current_mode.user_controllable || new_user_mode == current_mode.value) {
    MaybeCallTestingCallback(OsHooksErrors());
    return;
  }

  {
    ScopedRegistryUpdate update(sync_bridge);
    update->UpdateApp(app_id)->SetRunOnOsLoginMode(new_user_mode);
  }
  registrar->NotifyWebAppRunOnOsLoginModeChanged(app_id, new_user_mode);

  UpdateRunOnOsLoginOsIntegration(registrar, os_integration_manager, app_id,
                                  new_user_mode);
}

void SyncRunOnOsLoginOsIntegrationState(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    const AppId& app_id) {
  DCHECK(registrar);
  DCHECK(os_integration_manager);

  if (!registrar->IsLocallyInstalled(app_id)) {
    MaybeCallTestingCallback(OsHooksErrors());
    return;
  }

  RunOnOsLoginMode effective_mode =
      registrar->GetAppRunOnOsLoginMode(app_id).value;
  UpdateRunOnOsLoginOsIntegration(registrar, os_integration_manager, app_id,
                                  effective_mode);
}

}  // namespace web_app

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/uninstallation_via_os_settings_sub_manager.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/win/uninstallation_via_os_settings.h"

namespace web_app {

namespace {

bool IsOsUninstallationSupported() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

bool ShouldRegisterOsUninstall(
    const proto::WebAppOsIntegrationState& os_integration_state) {
  return os_integration_state.has_uninstall_registration() &&
         os_integration_state.uninstall_registration().registered_with_os();
}

}  // namespace

UninstallationViaOsSettingsSubManager::UninstallationViaOsSettingsSubManager(
    Profile& profile,
    WebAppRegistrar& registrar)
    : profile_(profile), registrar_(registrar) {}

UninstallationViaOsSettingsSubManager::
    ~UninstallationViaOsSettingsSubManager() = default;

void UninstallationViaOsSettingsSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_uninstall_registration());

  const WebApp* web_app = registrar_->GetAppById(app_id);

  proto::OsUninstallRegistration* os_uninstall_registration =
      desired_state.mutable_uninstall_registration();

  bool should_register = IsOsUninstallationSupported() &&
                         registrar_->IsLocallyInstalled(app_id) &&
                         web_app->CanUserUninstallWebApp();
  os_uninstall_registration->set_registered_with_os(should_register);
  os_uninstall_registration->set_display_name(
      registrar_->GetAppShortName(app_id));

  std::move(configure_done).Run();
}

void UninstallationViaOsSettingsSubManager::Start() {}

void UninstallationViaOsSettingsSubManager::Shutdown() {}

void UninstallationViaOsSettingsSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  if (!ShouldRegisterOsUninstall(current_state) &&
      !ShouldRegisterOsUninstall(desired_state)) {
    std::move(callback).Run();
    return;
  }

  if (ShouldRegisterOsUninstall(desired_state) &&
      ShouldRegisterOsUninstall(current_state) &&
      desired_state.uninstall_registration().SerializeAsString() ==
          current_state.uninstall_registration().SerializeAsString()) {
    std::move(callback).Run();
    return;
  }

  if (ShouldRegisterOsUninstall(current_state)) {
    bool result =
        UnregisterUninstallationViaOsSettingsWithOs(app_id, &profile_.get());
    base::UmaHistogramBoolean("WebApp.OsSettingsUninstallUnregistration.Result",
                              result);
  }

  if (ShouldRegisterOsUninstall(desired_state)) {
    bool result = RegisterUninstallationViaOsSettingsWithOs(
        app_id, desired_state.uninstall_registration().display_name(),
        &profile_.get());
    base::UmaHistogramBoolean("WebApp.OsSettingsUninstallRegistration.Result",
                              result);
  }

  std::move(callback).Run();
}

}  // namespace web_app

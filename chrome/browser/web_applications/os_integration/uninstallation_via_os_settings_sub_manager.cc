// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/uninstallation_via_os_settings_sub_manager.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
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

bool g_skip_execute_os_settings_sub_manager_for_testing = false;

UninstallationViaOsSettingsSubManager::UninstallationViaOsSettingsSubManager(
    const base::FilePath& profile_path,
    WebAppProvider& provider)
    : profile_path_(profile_path), provider_(provider) {}

UninstallationViaOsSettingsSubManager::
    ~UninstallationViaOsSettingsSubManager() = default;

void UninstallationViaOsSettingsSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_uninstall_registration());

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(configure_done).Run();
    return;
  }

  proto::OsUninstallRegistration* os_uninstall_registration =
      desired_state.mutable_uninstall_registration();

  bool should_register =
      IsOsUninstallationSupported() &&
      provider_->registrar_unsafe().IsLocallyInstalled(app_id) &&
      web_app->CanUserUninstallWebApp();
  os_uninstall_registration->set_registered_with_os(should_register);
  os_uninstall_registration->set_display_name(
      provider_->registrar_unsafe().GetAppShortName(app_id));

  std::move(configure_done).Run();
}

void UninstallationViaOsSettingsSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  if (g_skip_execute_os_settings_sub_manager_for_testing) {
    CHECK_IS_TEST();
    std::move(callback).Run();
    return;
  }

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
    CompleteUnregistration(app_id);
  }

  if (ShouldRegisterOsUninstall(desired_state)) {
    bool result = RegisterUninstallationViaOsSettingsWithOs(
        app_id, desired_state.uninstall_registration().display_name(),
        profile_path_);
    base::UmaHistogramBoolean("WebApp.OsSettingsUninstallRegistration.Result",
                              result);
  }

  std::move(callback).Run();
}

void UninstallationViaOsSettingsSubManager::ForceUnregister(
    const AppId& app_id,
    base::OnceClosure callback) {
  if (IsOsUninstallationSupported()) {
    CompleteUnregistration(app_id);
  }
  std::move(callback).Run();
}

void UninstallationViaOsSettingsSubManager::CompleteUnregistration(
    const AppId& app_id) {
  bool result =
      UnregisterUninstallationViaOsSettingsWithOs(app_id, profile_path_);
  base::UmaHistogramBoolean("WebApp.OsSettingsUninstallUnregistration.Result",
                            result);
}

}  // namespace web_app

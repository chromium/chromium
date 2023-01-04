// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/uninstallation_via_os_settings_sub_manager.h"

#include <utility>

#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

namespace {

bool IsOsUninstallationSupported() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

}  // namespace

UninstallationViaOsSettingsSubManager::UninstallationViaOsSettingsSubManager(
    WebAppRegistrar& registrar)
    : registrar_(registrar) {}

UninstallationViaOsSettingsSubManager::
    ~UninstallationViaOsSettingsSubManager() = default;

void UninstallationViaOsSettingsSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_uninstall_registration());

  if (!IsOsUninstallationSupported() ||
      !registrar_->IsLocallyInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  const WebApp* web_app = registrar_->GetAppById(app_id);

  proto::OsUninstallRegistration* os_uninstall_registration =
      desired_state.mutable_uninstall_registration();

  os_uninstall_registration->set_registered_with_os(
      web_app->CanUserUninstallWebApp());
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
  // Not implemented yet.
  std::move(callback).Run();
}

}  // namespace web_app

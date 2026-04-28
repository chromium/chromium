// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_user_installed_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

IsolatedWebAppUserInstalledManager::IsolatedWebAppUserInstalledManager(
    Profile& profile)
    : profile_(profile) {}

IsolatedWebAppUserInstalledManager::~IsolatedWebAppUserInstalledManager() =
    default;

void IsolatedWebAppUserInstalledManager::Start() {
  runtime_data_subscription_ =
      ChromeIwaRuntimeDataProvider::GetInstance().OnRuntimeDataChanged(
          base::BindRepeating(
              &IsolatedWebAppUserInstalledManager::OnRuntimeDataChanged,
              weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppUserInstalledManager::SetProvider(
    base::PassKey<WebAppProvider>,
    WebAppProvider& provider) {
  provider_ = &provider;
}

void IsolatedWebAppUserInstalledManager::Install(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppInstallSource& source,
    const std::optional<IwaVersion>& expected_version,
    InstallIsolatedWebAppCallback callback) {
  // That would be caught later anyway. This check just saves resources.
  if (!ChromeIwaRuntimeDataProvider::GetInstance().GetUserInstallAllowlistData(
          url_info.web_bundle_id().id())) {
    std::move(callback).Run(base::unexpected(InstallIsolatedWebAppCommandError{
        "IWA with WebAppManagement::Type::kIwaUserInstalled must be on the "
        "user install allowlist."}));
    return;
  }

  provider_->scheduler().InstallIsolatedWebApp(
      url_info, source, expected_version,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, std::move(callback));
}

void IsolatedWebAppUserInstalledManager::OnRuntimeDataChanged() {
  ChromeIwaRuntimeDataProvider& data_provider =
      ChromeIwaRuntimeDataProvider::GetInstance();

  for (const WebApp& iwa : provider_->registrar_unsafe().GetApps(
           WebAppFilter::UserInstalledIsolatedWebApp())) {
    if (data_provider.IsBundleBlocklisted(
            IwaOrigin::Create(iwa.scope())->web_bundle_id().id())) {
      provider_->scheduler().RemoveInstallManagementMaybeUninstall(
          iwa.app_id(), WebAppManagement::kIwaUserInstalled,
          webapps::WebappUninstallSource::kIwaBlocklisted, base::DoNothing());
    }
  }
}

}  // namespace web_app

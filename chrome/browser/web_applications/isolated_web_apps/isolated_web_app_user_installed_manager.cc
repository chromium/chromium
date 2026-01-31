// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_user_installed_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

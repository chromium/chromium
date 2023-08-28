// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/browser_shortcuts.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

namespace {

base::OnceClosure* GetInitializedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> g_initialized_callback;
  return g_initialized_callback.get();
}

}  // namespace

namespace web_app {

BrowserShortcuts::BrowserShortcuts(apps::AppServiceProxy* proxy)
    : apps::ShortcutPublisher(proxy),
      profile_(proxy->profile()),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile_)) {
  Initialize();
}

BrowserShortcuts::~BrowserShortcuts() = default;

void BrowserShortcuts::SetInitializedCallbackForTesting(
    base::OnceClosure callback) {
  static base::NoDestructor<base::OnceClosure> g_initialized_callback;
  *GetInitializedCallbackForTesting() = std::move(callback);
}

void BrowserShortcuts::Initialize() {
  CHECK(profile_);
  if (!AreWebAppsEnabled(profile_)) {
    return;
  }

  CHECK(provider_);
  provider_->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&BrowserShortcuts::InitBrowserShortcuts, AsWeakPtr()));
}

void BrowserShortcuts::InitBrowserShortcuts() {
  // Register publisher for shortcuts created from browser.
  RegisterShortcutPublisher(apps::AppType::kChromeApp);

  for (const AppId& web_app_id : provider_->registrar_unsafe().GetAppIds()) {
    MaybePublishBrowserShortcut(web_app_id);
  }

  install_manager_observation_.Observe(&provider_->install_manager());

  if (*GetInitializedCallbackForTesting()) {
    std::move(*GetInitializedCallbackForTesting()).Run();
  }
}

bool BrowserShortcuts::IsShortcut(const AppId& app_id) {
  if (base::FeatureList::IsEnabled(features::kCrosWebAppShortcutUiUpdate)) {
    return provider_->registrar_unsafe().IsShortcutApp(app_id);
  } else {
    return false;
  }
}

void BrowserShortcuts::MaybePublishBrowserShortcut(const AppId& app_id) {
  if (!IsShortcut(app_id)) {
    return;
  }
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    return;
  }
  apps::ShortcutPtr shortcut = std::make_unique<apps::Shortcut>(
      app_constants::kChromeAppId, web_app->app_id());
  shortcut->name =
      provider_->registrar_unsafe().GetAppShortName(web_app->app_id());
  shortcut->shortcut_source = apps::ShortcutSource::kUser;
  apps::ShortcutPublisher::PublishShortcut(std::move(shortcut));
}

void BrowserShortcuts::LaunchShortcut(const std::string& host_app_id,
                                      const std::string& local_id,
                                      int64_t display_id) {
  apps::AppLaunchParams params(
      local_id, apps::LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::LaunchSource::kFromAppListGrid, display_id);
  provider_->scheduler().LaunchAppWithCustomParams(std::move(params),
                                                   base::DoNothing());
}

void BrowserShortcuts::RemoveShortcut(const std::string& host_app_id,
                                      const std::string& local_shortcut_id,
                                      apps::UninstallSource uninstall_source) {
  if (!IsShortcut(local_shortcut_id)) {
    return;
  }

  const WebApp* web_app =
      provider_->registrar_unsafe().GetAppById(local_shortcut_id);
  if (!web_app) {
    return;
  }

  auto origin = url::Origin::Create(web_app->start_url());

  CHECK(
      provider_->registrar_unsafe().CanUserUninstallWebApp(web_app->app_id()));
  webapps::WebappUninstallSource webapp_uninstall_source =
      ConvertUninstallSourceToWebAppUninstallSource(uninstall_source);
  provider_->scheduler().UninstallWebApp(
      web_app->app_id(), webapp_uninstall_source, base::DoNothing());
}

void BrowserShortcuts::OnWebAppInstalled(const AppId& app_id) {
  MaybePublishBrowserShortcut(app_id);
}

void BrowserShortcuts::OnWebAppInstalledWithOsHooks(const AppId& app_id) {
  MaybePublishBrowserShortcut(app_id);
}

void BrowserShortcuts::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void BrowserShortcuts::OnWebAppUninstalled(
    const AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  if (!IsShortcut(app_id)) {
    return;
  }
  apps::ShortcutPublisher::ShortcutRemoved(
      apps::GenerateShortcutId(app_constants::kChromeAppId, app_id));
}

}  // namespace web_app

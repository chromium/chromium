// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/browser_shortcuts.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

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
  proxy_ = proxy;
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

  for (const webapps::AppId& web_app_id :
       provider_->registrar_unsafe().GetAppIds()) {
    MaybePublishBrowserShortcut(web_app_id);
  }

  install_manager_observation_.Observe(&provider_->install_manager());
  registrar_observation_.Observe(&provider_->registrar_unsafe());

  if (*GetInitializedCallbackForTesting()) {
    std::move(*GetInitializedCallbackForTesting()).Run();
  }
}

void BrowserShortcuts::MaybePublishBrowserShortcut(const webapps::AppId& app_id,
                                                   bool raw_icon_updated) {
  if (!IsAppServiceShortcut(app_id, *provider_)) {
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

  apps::IconEffects icon_effects = apps::IconEffects::kRoundCorners;
  icon_effects |= web_app->is_generated_icon()
                      ? apps::IconEffects::kCrOsStandardMask
                      : apps::IconEffects::kCrOsStandardIcon;
  shortcut->icon_key = apps::IconKey(raw_icon_updated, icon_effects);
  shortcut->allow_removal =
      provider_->registrar_unsafe().CanUserUninstallWebApp(web_app->app_id());
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
  if (!IsAppServiceShortcut(local_shortcut_id, *provider_)) {
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

void BrowserShortcuts::GetCompressedIconData(
    const std::string& shortcut_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  std::string local_id = proxy_->ShortcutRegistryCache()->GetShortcutLocalId(
      apps::ShortcutId(shortcut_id));
  apps::GetWebAppCompressedIconData(profile_, local_id, size_in_dip,
                                    scale_factor, std::move(callback));
}

void BrowserShortcuts::OnWebAppInstalled(const webapps::AppId& app_id) {
  MaybePublishBrowserShortcut(app_id);
}

void BrowserShortcuts::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  MaybePublishBrowserShortcut(app_id);
}

void BrowserShortcuts::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void BrowserShortcuts::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  // Once a web app has been uninstalled, the WebAppRegistrar can no longer
  // be used to determine if it is a shortcut. Here we check if we have got an
  // app registered in AppRegistryCache that can be be uninstalled. If this is
  // registered as an app, we do not update for shortcut.
  bool found = apps::AppServiceProxyFactory::GetForProfile(profile_)
                   ->AppRegistryCache()
                   .ForOneApp(app_id, [](const apps::AppUpdate& update) {});
  if (found) {
    return;
  }
  apps::ShortcutPublisher::ShortcutRemoved(
      apps::GenerateShortcutId(app_constants::kChromeAppId, app_id));
}

void BrowserShortcuts::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void BrowserShortcuts::OnWebAppUserDisplayModeChanged(
    const webapps::AppId& app_id,
    mojom::UserDisplayMode user_display_mode) {
  MaybePublishBrowserShortcut(app_id);
}

}  // namespace web_app

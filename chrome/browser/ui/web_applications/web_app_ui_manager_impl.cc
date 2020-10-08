// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif

namespace web_app {

namespace {

bool IsAppInstalled(apps::AppServiceProxy* proxy, const AppId& app_id) {
  bool installed = false;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&installed](const apps::AppUpdate& update) {
        installed =
            update.Readiness() != apps::mojom::Readiness::kUninstalledByUser;
      });
  return installed;
}

}  // namespace

// static
std::unique_ptr<WebAppUiManager> WebAppUiManager::Create(Profile* profile) {
  return std::make_unique<WebAppUiManagerImpl>(profile);
}

// static
bool WebAppUiManager::ShouldHideAppFromUser(const AppId& app_id) {
#if defined(OS_CHROMEOS)
  return app_list::HideInLauncherById(app_id);
#else
  return false;
#endif
}

// static
WebAppUiManagerImpl* WebAppUiManagerImpl::Get(Profile* profile) {
  auto* provider = WebAppProvider::Get(profile);
  return provider ? provider->ui_manager().AsImpl() : nullptr;
}

WebAppUiManagerImpl::WebAppUiManagerImpl(Profile* profile)
    : dialog_manager_(std::make_unique<WebAppDialogManager>(profile)),
      profile_(profile) {}

WebAppUiManagerImpl::~WebAppUiManagerImpl() = default;

void WebAppUiManagerImpl::SetSubsystems(
    AppRegistryController* app_registry_controller) {
  app_registry_controller_ = app_registry_controller;
}

void WebAppUiManagerImpl::Start() {
  DCHECK(!started_);
  started_ = true;

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!IsBrowserForInstalledApp(browser))
      continue;

    ++num_windows_for_apps_map_[GetAppIdForBrowser(browser)];
  }

  extensions::ExtensionSystem::Get(profile_)
      ->app_sorting()
      ->InitializePageOrdinalMapFromWebApps();

  BrowserList::AddObserver(this);
}

void WebAppUiManagerImpl::Shutdown() {
  BrowserList::RemoveObserver(this);
  started_ = false;
}

WebAppDialogManager& WebAppUiManagerImpl::dialog_manager() {
  return *dialog_manager_;
}

WebAppUiManagerImpl* WebAppUiManagerImpl::AsImpl() {
  return this;
}

size_t WebAppUiManagerImpl::GetNumWindowsForApp(const AppId& app_id) {
  DCHECK(started_);

  auto it = num_windows_for_apps_map_.find(app_id);
  if (it == num_windows_for_apps_map_.end())
    return 0;

  return it->second;
}

void WebAppUiManagerImpl::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  DCHECK(started_);

  const size_t num_windows_for_app = GetNumWindowsForApp(app_id);
  if (num_windows_for_app == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  windows_closed_requests_map_[app_id].push_back(std::move(callback));
}

void WebAppUiManagerImpl::UninstallAndReplaceIfExists(
    const std::vector<AppId>& from_apps,
    const AppId& to_app) {
  bool has_migrated = false;
  for (const AppId& from_app : from_apps) {
    apps::AppServiceProxy* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile_);
    if (!IsAppInstalled(proxy, from_app))
      continue;

    if (!has_migrated) {
#if defined(OS_CHROMEOS)
      // Grid position in app list.
      auto* app_list_syncable_service =
          app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
      if (app_list_syncable_service->GetSyncItem(from_app)) {
        app_list_syncable_service->TransferItemAttributes(from_app, to_app);
        has_migrated = true;
      }
#endif

      // If migration of user/UI data is required for other app types consider
      // generalising this operation to be part of app service.
      const extensions::Extension* from_extension =
          extensions::ExtensionRegistry::Get(profile_)
              ->enabled_extensions()
              .GetByID(from_app);
      if (from_extension) {
        // Grid position in chrome://apps.
        extensions::AppSorting* app_sorting =
            extensions::ExtensionSystem::Get(profile_)->app_sorting();
        app_sorting->SetAppLaunchOrdinal(
            to_app, app_sorting->GetAppLaunchOrdinal(from_app));
        app_sorting->SetPageOrdinal(to_app,
                                    app_sorting->GetPageOrdinal(from_app));

        // User pref for window/tab launch.
        switch (extensions::GetLaunchContainer(
            extensions::ExtensionPrefs::Get(profile_), from_extension)) {
          case extensions::LaunchContainer::kLaunchContainerWindow:
          case extensions::LaunchContainer::kLaunchContainerPanelDeprecated:
            app_registry_controller_->SetAppUserDisplayMode(
                to_app, DisplayMode::kStandalone, /*is_user_action=*/false);
            break;
          case extensions::LaunchContainer::kLaunchContainerTab:
          case extensions::LaunchContainer::kLaunchContainerNone:
            app_registry_controller_->SetAppUserDisplayMode(
                to_app, DisplayMode::kBrowser, /*is_user_action=*/false);
            break;
        }

        has_migrated = true;
      }
    }

    proxy->UninstallSilently(from_app,
                             apps::mojom::UninstallSource::kMigration);
  }
}

bool WebAppUiManagerImpl::CanAddAppToQuickLaunchBar() const {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

void WebAppUiManagerImpl::AddAppToQuickLaunchBar(const AppId& app_id) {
  DCHECK(CanAddAppToQuickLaunchBar());
#if defined(OS_CHROMEOS)
  // ChromeLauncherController does not exist in unit tests.
  if (auto* controller = ChromeLauncherController::instance()) {
    controller->PinAppWithID(app_id);
    controller->UpdateV1AppState(app_id);
  }
#endif  // defined(OS_CHROMEOS)
}

bool WebAppUiManagerImpl::IsInAppWindow(content::WebContents* web_contents,
                                        const AppId* app_id) const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (app_id)
    return AppBrowserController::IsForWebAppBrowser(browser, *app_id);
  return AppBrowserController::IsForWebAppBrowser(browser);
}

void WebAppUiManagerImpl::NotifyOnAssociatedAppChanged(
    content::WebContents* web_contents,
    const AppId& previous_app_id,
    const AppId& new_app_id) const {
  WebAppMetrics* web_app_metrics = WebAppMetrics::Get(profile_);
  // Unavailable in guest sessions.
  if (!web_app_metrics)
    return;
  web_app_metrics->NotifyOnAssociatedAppChanged(web_contents, previous_app_id,
                                                new_app_id);
}

bool WebAppUiManagerImpl::CanReparentAppTabToWindow(
    const AppId& app_id,
    bool shortcut_created) const {
#if defined(OS_MAC)
  // On macOS it is only possible to reparent the window when the shortcut (app
  // shim) was created. See https://crbug.com/915571.
  return shortcut_created;
#else
  return true;
#endif
}

void WebAppUiManagerImpl::ReparentAppTabToWindow(content::WebContents* contents,
                                                 const AppId& app_id,
                                                 bool shortcut_created) {
  DCHECK(CanReparentAppTabToWindow(app_id, shortcut_created));
  // Reparent the tab into an app window immediately.
  ReparentWebContentsIntoAppBrowser(contents, app_id);
}

void WebAppUiManagerImpl::OnBrowserAdded(Browser* browser) {
  DCHECK(started_);
  if (!IsBrowserForInstalledApp(browser))
    return;

  ++num_windows_for_apps_map_[GetAppIdForBrowser(browser)];
}

void WebAppUiManagerImpl::OnBrowserRemoved(Browser* browser) {
  DCHECK(started_);
  if (!IsBrowserForInstalledApp(browser))
    return;

  const auto& app_id = GetAppIdForBrowser(browser);

  size_t& num_windows_for_app = num_windows_for_apps_map_[app_id];
  DCHECK_GT(num_windows_for_app, 0u);
  --num_windows_for_app;

  if (num_windows_for_app > 0)
    return;

  auto it = windows_closed_requests_map_.find(app_id);
  if (it == windows_closed_requests_map_.end())
    return;

  for (auto& callback : it->second)
    std::move(callback).Run();

  windows_closed_requests_map_.erase(app_id);
}

bool WebAppUiManagerImpl::IsBrowserForInstalledApp(Browser* browser) {
  if (browser->profile() != profile_)
    return false;

  if (!browser->app_controller())
    return false;

  if (!browser->app_controller()->HasAppId())
    return false;

  return true;
}

const AppId WebAppUiManagerImpl::GetAppIdForBrowser(Browser* browser) {
  return browser->app_controller()->GetAppId();
}

}  // namespace web_app

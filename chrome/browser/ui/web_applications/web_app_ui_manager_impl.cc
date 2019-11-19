// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif

namespace web_app {

// static
std::unique_ptr<WebAppUiManager> WebAppUiManager::Create(Profile* profile) {
  return std::make_unique<WebAppUiManagerImpl>(profile);
}

// static
WebAppUiManagerImpl* WebAppUiManagerImpl::Get(Profile* profile) {
  auto* provider = WebAppProvider::Get(profile);
  return provider ? provider->ui_manager().AsImpl() : nullptr;
}

WebAppUiManagerImpl::WebAppUiManagerImpl(Profile* profile) : profile_(profile) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!IsBrowserForInstalledApp(browser))
      continue;

    ++num_windows_for_apps_map_[GetAppIdForBrowser(browser)];
  }

  BrowserList::AddObserver(this);

  dialog_manager_ = std::make_unique<WebAppDialogManager>(profile);
}

WebAppUiManagerImpl::~WebAppUiManagerImpl() {
  BrowserList::RemoveObserver(this);
}

WebAppDialogManager& WebAppUiManagerImpl::dialog_manager() {
  return *dialog_manager_;
}

WebAppUiManagerImpl* WebAppUiManagerImpl::AsImpl() {
  return this;
}

size_t WebAppUiManagerImpl::GetNumWindowsForApp(const AppId& app_id) {
  auto it = num_windows_for_apps_map_.find(app_id);
  if (it == num_windows_for_apps_map_.end())
    return 0;

  return it->second;
}

void WebAppUiManagerImpl::NotifyOnAllAppWindowsClosed(
    const AppId& app_id,
    base::OnceClosure callback) {
  const size_t num_windows_for_app = GetNumWindowsForApp(app_id);
  if (num_windows_for_app == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  windows_closed_requests_map_[app_id].push_back(std::move(callback));
}

void WebAppUiManagerImpl::UninstallAndReplace(
    const std::vector<AppId>& from_apps,
    const AppId& to_app) {
  bool has_migrated = false;
  for (const AppId& from_app : from_apps) {
    if (!has_migrated) {
#if defined(OS_CHROMEOS)
      auto* app_list_syncable_service =
          app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
      if (app_list_syncable_service->GetSyncItem(from_app)) {
        app_list_syncable_service->TransferItemAttributes(from_app, to_app);
        has_migrated = true;
      }
#endif
    }

    if (apps::AppServiceProxyFactory::IsEnabled()) {
      apps::AppServiceProxy* proxy =
          apps::AppServiceProxyFactory::GetForProfile(profile_);
      proxy->Uninstall(from_app, nullptr /* parent_window */);
    }
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

bool WebAppUiManagerImpl::IsInAppWindow(
    content::WebContents* web_contents) const {
  return AppBrowserController::IsForWebAppBrowser(
      chrome::FindBrowserWithWebContents(web_contents));
}

bool WebAppUiManagerImpl::CanReparentAppTabToWindow(
    const AppId& app_id,
    bool shortcut_created) const {
#if defined(OS_MACOSX)
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
  if (!IsBrowserForInstalledApp(browser)) {
    return;
  }

  ++num_windows_for_apps_map_[GetAppIdForBrowser(browser)];
}

void WebAppUiManagerImpl::OnBrowserRemoved(Browser* browser) {
  if (!IsBrowserForInstalledApp(browser)) {
    return;
  }

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

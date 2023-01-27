// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/commands/launch_web_app_command.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/process/process.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/gfx/native_widget_types.h"
#endif  // BUILDFLAG(IS_WIN)

namespace web_app {

namespace {

#if BUILDFLAG(IS_WIN)
// ScopedKeepAlive not only keeps the process from terminating early
// during uninstall, it also ensures the process will terminate when it
// is destroyed if there is no active browser window.
void UninstallWebAppWithDialogFromStartupSwitch(const AppId& app_id,
                                                WebAppProvider* provider) {
  // ScopedKeepAlive does not only keeps the process from early termination,
  // but ensure the process termination when there is no active browser window.
  std::unique_ptr<ScopedKeepAlive> scoped_keep_alive =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::WEB_APP_UNINSTALL,
                                        KeepAliveRestartOption::DISABLED);
  if (provider->install_finalizer().CanUserUninstallWebApp(app_id)) {
    WebAppUiManagerImpl::Get(provider)->dialog_manager().UninstallWebApp(
        app_id, webapps::WebappUninstallSource::kOsSettings,
        gfx::kNullNativeWindow,
        base::BindOnce([](std::unique_ptr<ScopedKeepAlive> scoped_keep_alive,
                          webapps::UninstallResultCode code) {},
                       std::move(scoped_keep_alive)));
  } else {
    // There is a chance that a previous invalid uninstall operation (due
    // to a crash or otherwise) could end up orphaning an OsSettings entry.
    // In this case we clean up the OsSettings entry.
    web_app::OsHooksOptions options;
    options[OsHookType::kUninstallationViaOsSettings] = true;

    auto synchronize_barrier =
        web_app::OsIntegrationManager::GetBarrierForSynchronize(base::BindOnce(
            [](std::unique_ptr<ScopedKeepAlive> scoped_keep_alive,
               OsHooksErrors os_hooks_errors) {},
            std::move(scoped_keep_alive)));
    provider->os_integration_manager().UninstallOsHooks(app_id, options,
                                                        synchronize_barrier);
    provider->scheduler().SynchronizeOsIntegration(
        app_id, base::BindOnce(synchronize_barrier, OsHooksErrors()));
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

// static
std::unique_ptr<WebAppUiManager> WebAppUiManager::Create(Profile* profile) {
  return std::make_unique<WebAppUiManagerImpl>(profile);
}

// static
WebAppUiManagerImpl* WebAppUiManagerImpl::Get(
    web_app::WebAppProvider* provider) {
  return provider ? provider->ui_manager().AsImpl() : nullptr;
}

WebAppUiManagerImpl::WebAppUiManagerImpl(Profile* profile)
    : dialog_manager_(std::make_unique<WebAppDialogManager>(profile)),
      profile_(profile) {}

WebAppUiManagerImpl::~WebAppUiManagerImpl() = default;

void WebAppUiManagerImpl::SetSubsystems(
    WebAppSyncBridge* sync_bridge,
    OsIntegrationManager* os_integration_manager) {
  sync_bridge_ = sync_bridge;
  os_integration_manager_ = os_integration_manager;
}

void WebAppUiManagerImpl::Start() {
  DCHECK(!started_);
  started_ = true;

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!IsBrowserForInstalledApp(browser))
      continue;

    ++num_windows_for_apps_map_[GetAppIdForBrowser(browser)];
  }

  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::BindOnce(&WebAppUiManagerImpl::OnExtensionSystemReady,
                                weak_ptr_factory_.GetWeakPtr()));

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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  windows_closed_requests_map_[app_id].push_back(std::move(callback));
}

void WebAppUiManagerImpl::OnExtensionSystemReady() {
  extensions::ExtensionSystem::Get(profile_)
      ->app_sorting()
      ->InitializePageOrdinalMapFromWebApps();
}

bool WebAppUiManagerImpl::CanAddAppToQuickLaunchBar() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

void WebAppUiManagerImpl::AddAppToQuickLaunchBar(const AppId& app_id) {
  DCHECK(CanAddAppToQuickLaunchBar());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeShelfController does not exist in unit tests.
  if (auto* controller = ChromeShelfController::instance()) {
    PinAppWithIDToShelf(app_id);
    controller->UpdateV1AppState(app_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool WebAppUiManagerImpl::IsAppInQuickLaunchBar(const AppId& app_id) const {
  DCHECK(CanAddAppToQuickLaunchBar());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeShelfController does not exist in unit tests.
  if (auto* controller = ChromeShelfController::instance()) {
    return IsAppWithIDPinnedToShelf(app_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
}

bool WebAppUiManagerImpl::IsInAppWindow(content::WebContents* web_contents,
                                        const AppId* app_id) const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (app_id)
    return AppBrowserController::IsForWebApp(browser, *app_id);
  return AppBrowserController::IsWebApp(browser);
}

void WebAppUiManagerImpl::NotifyOnAssociatedAppChanged(
    content::WebContents* web_contents,
    const absl::optional<AppId>& previous_app_id,
    const absl::optional<AppId>& new_app_id) const {
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
#if BUILDFLAG(IS_MAC)
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

void WebAppUiManagerImpl::ShowWebAppIdentityUpdateDialog(
    const std::string& app_id,
    bool title_change,
    bool icon_change,
    const std::u16string& old_title,
    const std::u16string& new_title,
    const SkBitmap& old_icon,
    const SkBitmap& new_icon,
    content::WebContents* web_contents,
    web_app::AppIdentityDialogCallback callback) {
  chrome::ShowWebAppIdentityUpdateDialog(
      app_id, title_change, icon_change, old_title, new_title, old_icon,
      new_icon, web_contents, std::move(callback));
}

base::Value WebAppUiManagerImpl::LaunchWebApp(
    apps::AppLaunchParams params,
    LaunchWebAppWindowSetting launch_setting,
    Profile& profile,
    LaunchWebAppCallback callback,
    AppLock& lock) {
  return ::web_app::LaunchWebApp(std::move(params), launch_setting, profile,
                                 std::move(callback), lock);
}

void WebAppUiManagerImpl::MaybeTransferAppAttributes(
    const AppId& from_extension_or_app,
    const AppId& to_app) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Grid position in app list.
  auto* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  bool to_app_in_shelf =
      app_list_syncable_service->GetPinPosition(to_app).IsValid();
  // If the new app is already pinned to the shelf don't transfer UI prefs
  // across as that could cause it to become unpinned.
  if (!to_app_in_shelf) {
    app_list_syncable_service->TransferItemAttributes(from_extension_or_app,
                                                      to_app);
  }
#endif
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

#if BUILDFLAG(IS_WIN)
void WebAppUiManagerImpl::UninstallWebAppFromStartupSwitch(
    const AppId& app_id) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile_);
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&UninstallWebAppWithDialogFromStartupSwitch,
                                app_id, provider));
}
#endif  //  BUILDFLAG(IS_WIN)

bool WebAppUiManagerImpl::IsBrowserForInstalledApp(Browser* browser) {
  if (browser->profile() != profile_)
    return false;

  if (!browser->app_controller())
    return false;

  return true;
}

AppId WebAppUiManagerImpl::GetAppIdForBrowser(Browser* browser) {
  return browser->app_controller()->app_id();
}

}  // namespace web_app

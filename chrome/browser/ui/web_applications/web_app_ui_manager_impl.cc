// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include <map>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/strings/string_piece_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/commands/launch_web_app_command.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/ui/web_applications/web_app_run_on_os_login_notification.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_uninstall_dialog_user_options.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/native_window_tracker.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/aura/window.h"
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "components/sync/model/string_ordinal.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN)
#include "base/process/process.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#endif  // BUILDFLAG(IS_WIN)

namespace base {
class FilePath;
}

namespace web_app {

class AppLock;

namespace {

#if BUILDFLAG(IS_WIN)
// ScopedKeepAlive not only keeps the process from terminating early
// during uninstall, it also ensures the process will terminate when it
// is destroyed if there is no active browser window.
void UninstallWebAppWithDialogFromStartupSwitch(const webapps::AppId& app_id,
                                                WebAppProvider* provider) {
  // ScopedKeepAlive does not only keeps the process from early termination,
  // but ensure the process termination when there is no active browser window.
  std::unique_ptr<ScopedKeepAlive> scoped_keep_alive =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::WEB_APP_UNINSTALL,
                                        KeepAliveRestartOption::DISABLED);
  if (provider->registrar_unsafe().CanUserUninstallWebApp(app_id)) {
    provider->ui_manager().PresentUserUninstallDialog(
        app_id, webapps::WebappUninstallSource::kOsSettings,
        gfx::NativeWindow(),
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

    // This is necessary to remove all OS integrations if the app has
    // been uninstalled.
    SynchronizeOsOptions synchronize_options;
    synchronize_options.force_unregister_os_integration = true;
    provider->scheduler().SynchronizeOsIntegration(
        app_id, base::BindOnce(synchronize_barrier, OsHooksErrors()),
        synchronize_options);
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

// static
std::unique_ptr<WebAppUiManager> WebAppUiManager::Create(Profile* profile) {
  return std::make_unique<WebAppUiManagerImpl>(profile);
}

WebAppUiManagerImpl::WebAppUiManagerImpl(Profile* profile)
    : profile_(profile) {}

WebAppUiManagerImpl::~WebAppUiManagerImpl() = default;

void WebAppUiManagerImpl::Start() {
  DCHECK(!started_);
  started_ = true;

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!IsBrowserForInstalledApp(browser)) {
      continue;
    }

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

WebAppUiManagerImpl* WebAppUiManagerImpl::AsImpl() {
  return this;
}

size_t WebAppUiManagerImpl::GetNumWindowsForApp(const webapps::AppId& app_id) {
  DCHECK(started_);

  auto it = num_windows_for_apps_map_.find(app_id);
  if (it == num_windows_for_apps_map_.end()) {
    return 0;
  }

  return it->second;
}

void WebAppUiManagerImpl::CloseAppWindows(const webapps::AppId& app_id) {
  DCHECK(started_);

  for (auto* browser : *BrowserList::GetInstance()) {
    const AppBrowserController* app_controller = browser->app_controller();
    if (app_controller && app_controller->app_id() == app_id) {
      browser->window()->Close();
    }
  }
}

void WebAppUiManagerImpl::NotifyOnAllAppWindowsClosed(
    const webapps::AppId& app_id,
    base::OnceClosure callback) {
  DCHECK(started_);

  const size_t num_windows_for_app = GetNumWindowsForApp(app_id);
  if (num_windows_for_app == 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

void WebAppUiManagerImpl::AddAppToQuickLaunchBar(const webapps::AppId& app_id) {
  DCHECK(CanAddAppToQuickLaunchBar());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeShelfController does not exist in unit tests.
  if (auto* controller = ChromeShelfController::instance()) {
    PinAppWithIDToShelf(app_id);
    controller->UpdateV1AppState(app_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool WebAppUiManagerImpl::IsAppInQuickLaunchBar(
    const webapps::AppId& app_id) const {
  DCHECK(CanAddAppToQuickLaunchBar());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeShelfController does not exist in unit tests.
  if (auto* controller = ChromeShelfController::instance()) {
    return controller->shelf_model()->IsAppPinned(app_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
}

bool WebAppUiManagerImpl::IsInAppWindow(content::WebContents* web_contents,
                                        const webapps::AppId* app_id) const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (app_id) {
    return AppBrowserController::IsForWebApp(browser, *app_id);
  }
  return AppBrowserController::IsWebApp(browser);
}

void WebAppUiManagerImpl::NotifyOnAssociatedAppChanged(
    content::WebContents* web_contents,
    const absl::optional<webapps::AppId>& previous_app_id,
    const absl::optional<webapps::AppId>& new_app_id) const {
  WebAppMetrics* web_app_metrics = WebAppMetrics::Get(profile_);
  // Unavailable in guest sessions.
  if (!web_app_metrics) {
    return;
  }
  web_app_metrics->NotifyOnAssociatedAppChanged(web_contents, previous_app_id,
                                                new_app_id);
}

bool WebAppUiManagerImpl::CanReparentAppTabToWindow(
    const webapps::AppId& app_id,
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
                                                 const webapps::AppId& app_id,
                                                 bool shortcut_created) {
  DCHECK(CanReparentAppTabToWindow(app_id, shortcut_created));
  // Reparent the tab into an app window immediately.
  ReparentWebContentsIntoAppBrowser(contents, app_id);
}

void WebAppUiManagerImpl::ShowWebAppFileLaunchDialog(
    const std::vector<base::FilePath>& file_paths,
    const webapps::AppId& app_id,
    WebAppLaunchAcceptanceCallback launch_callback) {
  chrome::ShowWebAppFileLaunchDialog(file_paths, profile_, app_id,
                                     std::move(launch_callback));
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

void WebAppUiManagerImpl::ShowWebAppSettings(const webapps::AppId& app_id) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile_);
  if (!provider) {
    return;
  }

  GURL start_url = provider->registrar_unsafe().GetAppStartUrl(app_id);
  if (!start_url.is_valid()) {
    return;
  }

  chrome::ShowSiteSettings(profile_, start_url);
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

#if BUILDFLAG(IS_CHROMEOS)
void WebAppUiManagerImpl::MigrateLauncherState(
    const webapps::AppId& from_app_id,
    const webapps::AppId& to_app_id,
    base::OnceClosure callback) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      lacros_service->GetInterfaceVersion<crosapi::mojom::WebAppService>() <
          int{crosapi::mojom::WebAppService::MethodMinVersions::
                  kMigrateLauncherStateMinVersion}) {
    LOG(WARNING) << "Ash version does not support MigrateLauncherState().";
    std::move(callback).Run();
    return;
  }
  // Forward the call to the Ash build of this method (see next #if branch).
  lacros_service->GetRemote<crosapi::mojom::WebAppService>()
      ->MigrateLauncherState(from_app_id, to_app_id, std::move(callback));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  auto* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  bool to_app_in_shelf =
      app_list_syncable_service->GetPinPosition(to_app_id).IsValid();
  // If the new app is already pinned to the shelf don't transfer UI prefs
  // across as that could cause it to become unpinned.
  if (!to_app_in_shelf) {
    app_list_syncable_service->TransferItemAttributes(from_app_id, to_app_id);
  }
  std::move(callback).Run();
#else
  static_assert(false);
#endif
}

void WebAppUiManagerImpl::DisplayRunOnOsLoginNotification(
    const std::vector<std::string>& app_names,
    base::WeakPtr<Profile> profile) {
  web_app::DisplayRunOnOsLoginNotification(app_names, profile);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

content::WebContents* WebAppUiManagerImpl::CreateNewTab() {
  NavigateParams params(profile_, GURL(url::kAboutBlankURL),
                        ui::PAGE_TRANSITION_FROM_API);
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  return handle->GetWebContents();
}

bool WebAppUiManagerImpl::IsWebContentsActiveTabInBrowser(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return browser &&
         browser->tab_strip_model() &&
         browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

void WebAppUiManagerImpl::TriggerInstallDialog(
    content::WebContents* web_contents) {
  web_app::CreateWebAppFromManifest(
      web_contents, /*bypass_service_worker_check=*/true,
      // TODO(issuetracker.google.com/283034487): Consider passing in the
      // install source from the caller.
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, base::DoNothing());
}

void WebAppUiManagerImpl::PresentUserUninstallDialog(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    BrowserWindow* parent_window,
    UninstallCompleteCallback callback) {
  PresentUserUninstallDialog(
      app_id, uninstall_source,
      parent_window ? parent_window->GetNativeWindow() : nullptr,
      std::move(callback), base::DoNothing());
}

void WebAppUiManagerImpl::PresentUserUninstallDialog(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow native_window,
    UninstallCompleteCallback callback) {
  PresentUserUninstallDialog(app_id, uninstall_source, native_window,
                             std::move(callback), base::DoNothing());
}

void WebAppUiManagerImpl::PresentUserUninstallDialog(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent_window,
    UninstallCompleteCallback uninstall_complete_callback,
    UninstallScheduledCallback uninstall_scheduled_callback) {
  std::unique_ptr<views::NativeWindowTracker> parent_window_tracker;
  if (parent_window) {
    parent_window_tracker = views::NativeWindowTracker::Create(parent_window);
  }

  if (parent_window && parent_window_tracker->WasNativeWindowDestroyed()) {
    OnUninstallCancelled(std::move(uninstall_complete_callback),
                         std::move(uninstall_scheduled_callback));
    return;
  }

  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile_);
  CHECK(provider);

  provider->icon_manager().ReadIcons(
      app_id, IconPurpose::ANY,
      provider->registrar_unsafe().GetAppDownloadedIconSizesAny(app_id),
      base::BindOnce(&WebAppUiManagerImpl::OnIconsReadForUninstall,
                     weak_ptr_factory_.GetWeakPtr(), app_id, uninstall_source,
                     parent_window, std::move(parent_window_tracker),
                     std::move(uninstall_complete_callback),
                     std::move(uninstall_scheduled_callback)));
}

void WebAppUiManagerImpl::OnBrowserAdded(Browser* browser) {
  DCHECK(started_);
  if (!IsBrowserForInstalledApp(browser)) {
    return;
  }

  ++num_windows_for_apps_map_[GetAppIdForBrowser(browser)];
}

void WebAppUiManagerImpl::OnBrowserRemoved(Browser* browser) {
  DCHECK(started_);
  if (!IsBrowserForInstalledApp(browser)) {
    return;
  }

  const auto& app_id = GetAppIdForBrowser(browser);

  size_t& num_windows_for_app = num_windows_for_apps_map_[app_id];
  DCHECK_GT(num_windows_for_app, 0u);
  --num_windows_for_app;

  if (num_windows_for_app > 0) {
    return;
  }

  auto it = windows_closed_requests_map_.find(app_id);
  if (it == windows_closed_requests_map_.end()) {
    return;
  }

  for (auto& callback : it->second) {
    std::move(callback).Run();
  }

  windows_closed_requests_map_.erase(app_id);
}

#if BUILDFLAG(IS_WIN)
void WebAppUiManagerImpl::UninstallWebAppFromStartupSwitch(
    const webapps::AppId& app_id) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile_);
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&UninstallWebAppWithDialogFromStartupSwitch,
                                app_id, provider));
}
#endif  //  BUILDFLAG(IS_WIN)

bool WebAppUiManagerImpl::IsBrowserForInstalledApp(Browser* browser) {
  if (browser->profile() != profile_) {
    return false;
  }

  if (!browser->app_controller()) {
    return false;
  }

  return true;
}

webapps::AppId WebAppUiManagerImpl::GetAppIdForBrowser(Browser* browser) {
  return browser->app_controller()->app_id();
}

void WebAppUiManagerImpl::OnIconsReadForUninstall(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent_window,
    std::unique_ptr<views::NativeWindowTracker> parent_window_tracker,
    UninstallCompleteCallback complete_callback,
    UninstallScheduledCallback uninstall_scheduled_callback,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  if (parent_window && parent_window_tracker->WasNativeWindowDestroyed()) {
    OnUninstallCancelled(std::move(complete_callback),
                         std::move(uninstall_scheduled_callback));
    return;
  }

  chrome::ShowWebAppUninstallDialog(
      profile_, app_id, uninstall_source, parent_window,
      std::move(icon_bitmaps),
      base::BindOnce(&WebAppUiManagerImpl::ScheduleUninstallIfUserRequested,
                     weak_ptr_factory_.GetWeakPtr(), app_id, uninstall_source,
                     std::move(complete_callback),
                     std::move(uninstall_scheduled_callback)));
}

void WebAppUiManagerImpl::ScheduleUninstallIfUserRequested(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    UninstallCompleteCallback complete_callback,
    UninstallScheduledCallback uninstall_scheduled_callback,
    web_app::UninstallUserOptions uninstall_options) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile_);
  CHECK(provider);

  const bool uninstall_scheduled =
      uninstall_options.user_wants_uninstall &&
      provider->registrar_unsafe().CanUserUninstallWebApp(app_id);
  std::move(uninstall_scheduled_callback).Run(uninstall_scheduled);
  if (!uninstall_scheduled) {
    std::move(complete_callback).Run(webapps::UninstallResultCode::kCancelled);
    return;
  }

  UninstallCompleteCallback final_callback;
  if (uninstall_options.clear_site_data) {
    CHECK(uninstall_options.user_wants_uninstall);
    const GURL app_start_url =
        provider->registrar_unsafe().GetAppStartUrl(app_id);
    final_callback =
        base::BindOnce(&WebAppUiManagerImpl::ClearWebAppSiteDataIfNeeded,
                       weak_ptr_factory_.GetWeakPtr(), app_start_url,
                       std::move(complete_callback));
  } else {
    final_callback = std::move(complete_callback);
  }

  provider->scheduler().UninstallWebApp(app_id, uninstall_source,
                                        std::move(final_callback));
}

void WebAppUiManagerImpl::OnUninstallCancelled(
    UninstallCompleteCallback complete_callback,
    UninstallScheduledCallback uninstall_scheduled_callback) {
  std::move(uninstall_scheduled_callback).Run(false);
  std::move(complete_callback).Run(webapps::UninstallResultCode::kCancelled);
}

void WebAppUiManagerImpl::ClearWebAppSiteDataIfNeeded(
    const GURL app_start_url,
    UninstallCompleteCallback uninstall_complete_callback,
    webapps::UninstallResultCode uninstall_code) {
  // This callback should be run at the very end of the uninstallation + site
  // data removal process (if any).
  base::OnceClosure final_uninstall_callback =
      base::BindOnce(std::move(uninstall_complete_callback), uninstall_code);

  // Only clear site data if the uninstallation has succeeded, i.e. either the
  // app has been uninstalled completely, or it was previously uninstalled but
  // some data had been left over.
  if (webapps::UninstallSucceeded(uninstall_code)) {
    content::ClearSiteData(base::BindRepeating(
                               [](content::BrowserContext* browser_context) {
                                 return browser_context;
                               },
                               base::Unretained(profile_)),
                           /*storage_partition_config=*/absl::nullopt,
                           url::Origin::Create(app_start_url),
                           content::ClearSiteDataTypeSet::All(),
                           /*storage_buckets_to_remove=*/{},
                           /*avoid_closing_connections=*/false,
                           /*cookie_partition_key=*/absl::nullopt,
                           /*storage_key=*/absl::nullopt,
                           /*partitioned_state_allowed_only=*/false,
                           std::move(final_uninstall_callback));
  } else {
    std::move(final_uninstall_callback).Run();
  }
}

}  // namespace web_app

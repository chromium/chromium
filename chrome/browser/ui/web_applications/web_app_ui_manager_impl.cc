// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include <map>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/one_shot_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/ui/web_applications/web_app_run_on_os_login_notification.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_uninstall_dialog_user_options.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_data.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/native_window_tracker.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#else
#include "chrome/browser/ui/web_applications/web_app_relaunch_notification.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_MAC)
#include "ui/aura/window.h"
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chromeos/ash/components/nonclosable_app_ui/nonclosable_app_ui_utils.h"
#include "components/sync/model/string_ordinal.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/startup/first_run_service.h"
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
void UninstallWebAppWithDialogFromStartupSwitch(
    std::unique_ptr<ScopedKeepAlive> scoped_keep_alive,
    const webapps::AppId& app_id,
    WebAppProvider* provider) {
  if (provider->registrar_unsafe().CanUserUninstallWebApp(app_id)) {
    provider->ui_manager().PresentUserUninstallDialog(
        app_id, webapps::WebappUninstallSource::kOsSettings,
        gfx::NativeWindow(),
        base::BindOnce(
            [](std::unique_ptr<ScopedKeepAlive> scoped_keep_alive,
               webapps::UninstallResultCode code) {
              // This ensures that the scoped_keep_alive will be deleted in the
              // next message loop, giving objects like DialogDelegate enough
              // time to shut itself down. See crbug.com/1506302 for more
              // information.
              base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
                  FROM_HERE, std::move(scoped_keep_alive));
            },
            std::move(scoped_keep_alive)));
  } else {
    // This is necessary to remove all OS integrations if the app has
    // been uninstalled.
    SynchronizeOsOptions synchronize_options;
    synchronize_options.force_unregister_os_integration = true;
    provider->scheduler().SynchronizeOsIntegration(
        app_id,
        base::BindOnce(
            [](std::unique_ptr<ScopedKeepAlive> scoped_keep_alive) {},
            std::move(scoped_keep_alive)),
        synchronize_options);
  }
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
void ShowNonclosableAppToast(const web_app::WebAppRegistrar& registrar,
                             const webapps::AppId& app_id) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* const service = chromeos::LacrosService::Get();
  if (!service->IsRegistered<crosapi::mojom::NonclosableAppToastService>() ||
      !service->IsAvailable<crosapi::mojom::NonclosableAppToastService>()) {
    return;
  }

  crosapi::mojom::NonclosableAppToastService* const
      nonclosable_app_toast_service =
          service->GetRemote<crosapi::mojom::NonclosableAppToastService>()
              .get();
  nonclosable_app_toast_service->OnUserAttemptedClose(
      app_id, registrar.GetAppShortName(app_id));
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ShowNonclosableAppToast(app_id, registrar.GetAppShortName(app_id));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  for (Browser* browser : *BrowserList::GetInstance()) {
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

bool WebAppUiManagerImpl::IsInAppWindow(
    content::WebContents* web_contents) const {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  return AppBrowserController::IsWebApp(browser);
}

const webapps::AppId* WebAppUiManagerImpl::GetAppIdForWindow(
    const content::WebContents* web_contents) const {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (AppBrowserController::IsWebApp(browser)) {
    return &browser->app_controller()->app_id();
  }
  return nullptr;
}

void WebAppUiManagerImpl::NotifyOnAssociatedAppChanged(
    content::WebContents* web_contents,
    const std::optional<webapps::AppId>& previous_app_id,
    const std::optional<webapps::AppId>& new_app_id) const {
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

Browser* WebAppUiManagerImpl::ReparentAppTabToWindow(
    content::WebContents* contents,
    const webapps::AppId& app_id,
    bool shortcut_created) {
  DCHECK(CanReparentAppTabToWindow(app_id, shortcut_created));
  // Reparent the tab into an app window immediately.
  return ReparentWebContentsIntoAppBrowser(contents, app_id);
}

Browser* WebAppUiManagerImpl::ReparentAppTabToWindow(
    content::WebContents* contents,
    const webapps::AppId& app_id,
    base::OnceCallback<void(content::WebContents*)> completion_callback) {
  return ReparentWebContentsIntoAppBrowser(contents, app_id,
                                           std::move(completion_callback));
}

void WebAppUiManagerImpl::ShowWebAppFileLaunchDialog(
    const std::vector<base::FilePath>& file_paths,
    const webapps::AppId& app_id,
    WebAppLaunchAcceptanceCallback launch_callback) {
  ::web_app::ShowWebAppFileLaunchDialog(file_paths, profile_, app_id,
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
  ::web_app::ShowWebAppIdentityUpdateDialog(
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

void WebAppUiManagerImpl::LaunchWebApp(apps::AppLaunchParams params,
                                       LaunchWebAppWindowSetting launch_setting,
                                       Profile& profile,
                                       LaunchWebAppDebugValueCallback callback,
                                       WithAppResources& lock) {
  ::web_app::LaunchWebApp(std::move(params), launch_setting, profile, lock,
                          std::move(callback));
}

void WebAppUiManagerImpl::WaitForFirstRunService(
    Profile& profile,
    FirstRunServiceCompletedCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  FirstRunService* first_run_service =
      FirstRunServiceFactory::GetForBrowserContextIfExists(&profile);
  if (first_run_service) {
    first_run_service->OpenFirstRunIfNeeded(
        FirstRunService::EntryPoint::kWebAppLaunch, std::move(callback));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  std::move(callback).Run(/*success=*/true);
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
    const base::flat_map<webapps::AppId, RoolNotificationBehavior>& apps,
    base::WeakPtr<Profile> profile) {
  web_app::DisplayRunOnOsLoginNotification(apps, profile);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void WebAppUiManagerImpl::NotifyAppRelaunchState(
    const webapps::AppId& placeholder_app_id,
    const webapps::AppId& final_app_id,
    const std::u16string& final_app_name,
    base::WeakPtr<Profile> profile,
    AppRelaunchState relaunch_state) {
#if BUILDFLAG(IS_CHROMEOS)
  web_app::NotifyAppRelaunchState(placeholder_app_id, final_app_id,
                                  final_app_name, std::move(profile),
                                  relaunch_state);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

content::WebContents* WebAppUiManagerImpl::CreateNewTab() {
  NavigateParams params(profile_, GURL(url::kAboutBlankURL),
                        ui::PAGE_TRANSITION_FROM_API);
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  if (handle) {
    return handle->GetWebContents();
  }
  return nullptr;
}

bool WebAppUiManagerImpl::IsWebContentsActiveTabInBrowser(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  return browser && browser->tab_strip_model() &&
         browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

void WebAppUiManagerImpl::TriggerInstallDialog(
    content::WebContents* web_contents) {
  web_app::CreateWebAppFromManifest(
      web_contents,
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

void WebAppUiManagerImpl::LaunchOrFocusIsolatedWebAppInstaller(
    const base::FilePath& bundle_path) {
  auto it = active_installers_.find(bundle_path);

  if (it == active_installers_.end()) {
    // If no installer exists for the path, we create a new coordinator
    active_installers_[bundle_path] = ::web_app::LaunchIsolatedWebAppInstaller(
        profile_, bundle_path,
        base::BindOnce(&WebAppUiManagerImpl::OnIsolatedWebAppInstallerClosed,
                       weak_ptr_factory_.GetWeakPtr(), bundle_path));
  } else {
    // If an installer already exists for |path|, we focus the existing
    // installer.
    FocusIsolatedWebAppInstaller(it->second);
  }
}

void WebAppUiManagerImpl::OnIsolatedWebAppInstallerClosed(
    base::FilePath bundle_path) {
  auto it = active_installers_.find(bundle_path);
  CHECK(it != active_installers_.end())
      << "Installer with path " << bundle_path
      << " is being closed, but it is not found in the list of active "
         "installers.";
  active_installers_.erase(it);
}

void WebAppUiManagerImpl::MaybeCreateEnableSupportedLinksInfobar(
    content::WebContents* web_contents,
    const std::string& launch_name) {
#if !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<apps::EnableLinkCapturingInfoBarDelegate> delegate =
      apps::EnableLinkCapturingInfoBarDelegate::MaybeCreate(web_contents,
                                                            launch_name);
  if (delegate) {
    infobars::ContentInfoBarManager::FromWebContents(web_contents)
        ->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

void WebAppUiManagerImpl::MaybeShowIPHPromoForAppsLaunchedViaLinkCapturing(
    Browser* browser,
    Profile* profile,
    const std::string& app_id) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);
  CHECK(provider);

  if (!provider->registrar_unsafe().CapturesLinksInScope(app_id)) {
    return;
  }

  const Browser* app_browser =
      browser ? browser : AppBrowserController::FindForWebApp(*profile, app_id);
  if (!app_browser) {
    return;
  }

  if (WebAppPrefGuardrails::GetForNavigationCapturingIph(
          app_browser->profile()->GetPrefs())
          .IsBlockedByGuardrails(app_id)) {
    return;
  }

  web_app::PostCallbackOnBrowserActivation(
      app_browser, kToolbarAppMenuButtonElementId,
      base::BindOnce(
          &WebAppUiManagerImpl::ShowIPHPromoForAppsLaunchedViaLinkCapturing,
          weak_ptr_factory_.GetWeakPtr(), app_browser, app_id));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

void WebAppUiManagerImpl::OnBrowserAdded(Browser* browser) {
  DCHECK(started_);
  if (!IsBrowserForInstalledApp(browser)) {
    return;
  }

  ++num_windows_for_apps_map_[GetAppIdForBrowser(browser)];

#if BUILDFLAG(IS_CHROMEOS)
  browser->tab_strip_model()->AddObserver(this);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void WebAppUiManagerImpl::OnBrowserCloseCancelled(Browser* browser,
                                                  BrowserClosingStatus reason) {
  DCHECK(started_);
  if (!IsBrowserForInstalledApp(browser) ||
      reason != BrowserClosingStatus::kDeniedByPolicy) {
    return;
  }

  ShowNonclosableAppToast(
      WebAppProvider::GetForWebApps(profile_)->registrar_unsafe(),
      GetAppIdForBrowser(browser));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void WebAppUiManagerImpl::OnBrowserRemoved(Browser* browser) {
  DCHECK(started_);
  if (!IsBrowserForInstalledApp(browser)) {
    return;
  }

  const auto& app_id = GetAppIdForBrowser(browser);

  size_t& num_windows_for_app = num_windows_for_apps_map_[app_id];
  DCHECK_GT(num_windows_for_app, 0u);
  --num_windows_for_app;

#if BUILDFLAG(IS_CHROMEOS)
  browser->tab_strip_model()->RemoveObserver(this);
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_CHROMEOS)
void WebAppUiManagerImpl::TabCloseCancelled(
    const content::WebContents* contents) {
  const webapps::AppId* app_id = GetAppIdForWindow(contents);
  if (!app_id) {
    return;
  }

  ShowNonclosableAppToast(
      WebAppProvider::GetForWebApps(profile_)->registrar_unsafe(), *app_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
void WebAppUiManagerImpl::UninstallWebAppFromStartupSwitch(
    const webapps::AppId& app_id) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile_);
  // ScopedKeepAlive not only keeps the process from terminating early
  // during uninstall, it also ensures the process will terminate in the next
  // message loop if there are no active browser windows.
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&UninstallWebAppWithDialogFromStartupSwitch,
                                std::make_unique<ScopedKeepAlive>(
                                    KeepAliveOrigin::WEB_APP_UNINSTALL,
                                    KeepAliveRestartOption::DISABLED),
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

  ShowWebAppUninstallDialog(
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

  provider->scheduler().RemoveUserUninstallableManagements(
      app_id, uninstall_source, std::move(final_callback));
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
    content::ClearSiteData(profile_->GetWeakPtr(),
                           /*storage_partition_config=*/std::nullopt,
                           url::Origin::Create(app_start_url),
                           content::ClearSiteDataTypeSet::All(),
                           /*storage_buckets_to_remove=*/{},
                           /*avoid_closing_connections=*/false,
                           /*cookie_partition_key=*/std::nullopt,
                           /*storage_key=*/std::nullopt,
                           /*partitioned_state_allowed_only=*/false,
                           std::move(final_uninstall_callback));
  } else {
    std::move(final_uninstall_callback).Run();
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void WebAppUiManagerImpl::ShowIPHPromoForAppsLaunchedViaLinkCapturing(
    const Browser* browser,
    const webapps::AppId& app_id,
    bool is_activated) {
  if (!is_activated) {
    return;
  }

  user_education::FeaturePromoParams promo_params(
      feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch, app_id);
  promo_params.close_callback =
      base::BindOnce(&WebAppUiManagerImpl::OnIPHPromoResponseForLinkCapturing,
                     weak_ptr_factory_.GetWeakPtr(), browser, app_id);
  promo_params.show_promo_result_callback =
      base::BindOnce([](user_education::FeaturePromoResult result) {
        if (result) {
          base::RecordAction(
              base::UserMetricsAction("LinkCapturingIPHAppBubbleShown"));
        }
      });

  browser->window()->MaybeShowFeaturePromo(std::move(promo_params));
}

void WebAppUiManagerImpl::OnIPHPromoResponseForLinkCapturing(
    const Browser* browser,
    const webapps::AppId& app_id) {
  if (!browser) {
    return;
  }

  const auto* const feature_promo_controller =
      browser->window()->GetFeaturePromoController(
          base::PassKey<WebAppUiManagerImpl>());
  if (!feature_promo_controller) {
    return;
  }

  user_education::FeaturePromoClosedReason close_reason;
  feature_promo_controller->HasPromoBeenDismissed(
      {feature_engagement::kIPHDesktopPWAsLinkCapturingLaunch, app_id},
      &close_reason);
  switch (close_reason) {
    case user_education::FeaturePromoClosedReason::kAction:
      base::RecordAction(
          base::UserMetricsAction("LinkCapturingIPHAppBubbleAccepted"));
      WebAppPrefGuardrails::GetForNavigationCapturingIph(
          browser->profile()->GetPrefs())
          .RecordAccept(app_id);
      break;
    case user_education::FeaturePromoClosedReason::kDismiss:
    case user_education::FeaturePromoClosedReason::kCancel:
      base::RecordAction(
          base::UserMetricsAction("LinkCapturingIPHAppBubbleNotAccepted"));
      WebAppPrefGuardrails::GetForNavigationCapturingIph(
          browser->profile()->GetPrefs())
          .RecordDismiss(app_id, base::Time::Now());
      break;
    default:
      break;
  }
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace web_app

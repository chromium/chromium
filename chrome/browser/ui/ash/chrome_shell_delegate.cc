// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <memory>
#include <utility>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "cc/input/touch_action.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/ash/back_gesture_contextual_nudge_delegate.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/chrome_accessibility_delegate.h"
#include "chrome/browser/ui/ash/desks_client.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/ash/tab_scrubber.h"
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_properties.h"
#include "components/favicon/core/favicon_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "url/gurl.h"

namespace {

const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

// Browser tests are always started with --disable-logging-redirect, so we need
// independent option here.
absl::optional<bool> disable_logging_redirect_for_testing;

// Returns the TabStripModel that associates with |window| if the given |window|
// contains a browser frame, otherwise returns nullptr.
TabStripModel* GetTabstripModelForWindowIfAny(aura::Window* window) {
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  return browser_view ? browser_view->browser()->tab_strip_model() : nullptr;
}

content::WebContents* GetActiveWebContentsForNativeBrowserWindow(
    gfx::NativeWindow window) {
  if (!window)
    return nullptr;

  TabStripModel* tab_strip_model = GetTabstripModelForWindowIfAny(window);
  return tab_strip_model ? tab_strip_model->GetActiveWebContents() : nullptr;
}

// Returns the list of URLs that are open in |tab_strip_model|.
std::vector<GURL> GetURLsIfApplicable(TabStripModel* tab_strip_model) {
  DCHECK(tab_strip_model);

  std::vector<GURL> urls;
  for (int i = 0; i < tab_strip_model->count(); ++i)
    urls.push_back(tab_strip_model->GetWebContentsAt(i)->GetLastCommittedURL());
  return urls;
}

// Returns true if `window` is supported in desk templates feature.
bool IsWindowSupportedForDeskTemplate(aura::Window* window,
                                      Profile* user_profile) {
  // For now we'll crostini and lacros windows in desk template. We'll also
  // ignore ARC apps unless the flag is turned on.
  const ash::AppType app_type =
      static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
  switch (app_type) {
    case ash::AppType::NON_APP:
    case ash::AppType::CROSTINI_APP:
    case ash::AppType::LACROS:
      return false;
    case ash::AppType::ARC_APP:
      if (!app_restore::features::IsArcAppsForDesksTemplatesEnabled())
        return false;
      break;
    case ash::AppType::BROWSER:
    case ash::AppType::CHROME_APP:
    case ash::AppType::SYSTEM_APP:
      break;
  }

  DCHECK(user_profile);
  // Exclude window that does not asscociate with a full restore app id.
  const std::string app_id = full_restore::GetAppId(window);
  if (app_id.empty())
    return false;

  // Exclude incognito browser window.
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  if (browser_view && browser_view->GetIncognito())
    return false;

  return true;
}

}  // namespace

ChromeShellDelegate::ChromeShellDelegate() = default;

ChromeShellDelegate::~ChromeShellDelegate() = default;

bool ChromeShellDelegate::CanShowWindowForUser(
    const aura::Window* window) const {
  return ::CanShowWindowForUser(window,
                                base::BindRepeating(&GetActiveBrowserContext));
}

std::unique_ptr<ash::CaptureModeDelegate>
ChromeShellDelegate::CreateCaptureModeDelegate() const {
  return std::make_unique<ChromeCaptureModeDelegate>();
}

void ChromeShellDelegate::OpenKeyboardShortcutHelpPage() const {
  chrome::ScopedTabbedBrowserDisplayer scoped_tabbed_browser_displayer(
      ProfileManager::GetActiveUserProfile());
  NavigateParams params(scoped_tabbed_browser_displayer.browser(),
                        GURL(kKeyboardShortcutHelpPageUrl),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  Navigate(&params);
}

bool ChromeShellDelegate::CanGoBack(gfx::NativeWindow window) const {
  content::WebContents* contents =
      GetActiveWebContentsForNativeBrowserWindow(window);
  return contents ? contents->GetController().CanGoBack() : false;
}

void ChromeShellDelegate::SetTabScrubberEnabled(bool enabled) {
  TabScrubber::GetInstance()->SetEnabled(enabled);
}

bool ChromeShellDelegate::AllowDefaultTouchActions(gfx::NativeWindow window) {
  content::WebContents* contents =
      GetActiveWebContentsForNativeBrowserWindow(window);
  if (!contents)
    return true;
  content::RenderWidgetHostView* render_widget_host_view =
      contents->GetRenderWidgetHostView();
  if (!render_widget_host_view)
    return true;
  content::RenderWidgetHost* render_widget_host =
      render_widget_host_view->GetRenderWidgetHost();
  if (!render_widget_host)
    return true;
  absl::optional<cc::TouchAction> allowed_touch_action =
      render_widget_host->GetAllowedTouchAction();
  return allowed_touch_action.has_value()
             ? *allowed_touch_action != cc::TouchAction::kNone
             : true;
}

bool ChromeShellDelegate::ShouldWaitForTouchPressAck(gfx::NativeWindow window) {
  content::WebContents* contents =
      GetActiveWebContentsForNativeBrowserWindow(window);
  if (!contents)
    return false;
  content::RenderWidgetHostView* render_widget_host_view =
      contents->GetRenderWidgetHostView();
  if (!render_widget_host_view)
    return false;
  return !!render_widget_host_view->GetRenderWidgetHost();
}

bool ChromeShellDelegate::IsTabDrag(const ui::OSExchangeData& drop_data) {
  DCHECK(ash::features::IsWebUITabStripTabDragIntegrationEnabled());
  return tab_strip_ui::IsDraggedTab(drop_data);
}

int ChromeShellDelegate::GetBrowserWebUITabStripHeight() {
  DCHECK(ash::features::IsWebUITabStripTabDragIntegrationEnabled());
  return TabStripUILayout::GetContainerHeight();
}

void ChromeShellDelegate::BindBluetoothSystemFactory(
    mojo::PendingReceiver<device::mojom::BluetoothSystemFactory> receiver) {
  content::GetDeviceService().BindBluetoothSystemFactory(std::move(receiver));
}

void ChromeShellDelegate::BindFingerprint(
    mojo::PendingReceiver<device::mojom::Fingerprint> receiver) {
  content::GetDeviceService().BindFingerprint(std::move(receiver));
}

void ChromeShellDelegate::BindMultiDeviceSetup(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  chromeos::multidevice_setup::MultiDeviceSetupService* service =
      chromeos::multidevice_setup::MultiDeviceSetupServiceFactory::
          GetForProfile(ProfileManager::GetPrimaryUserProfile());
  if (service)
    service->BindMultiDeviceSetup(std::move(receiver));
}

media_session::MediaSessionService*
ChromeShellDelegate::GetMediaSessionService() {
  return &content::GetMediaSessionService();
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new ChromeAccessibilityDelegate;
}

std::unique_ptr<ash::BackGestureContextualNudgeDelegate>
ChromeShellDelegate::CreateBackGestureContextualNudgeDelegate(
    ash::BackGestureContextualNudgeController* controller) {
  return std::make_unique<BackGestureContextualNudgeDelegate>(controller);
}

std::unique_ptr<ash::NearbyShareDelegate>
ChromeShellDelegate::CreateNearbyShareDelegate(
    ash::NearbyShareController* controller) const {
  return std::make_unique<NearbyShareDelegateImpl>(controller);
}

bool ChromeShellDelegate::IsSessionRestoreInProgress() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return SessionRestore::IsRestoring(profile);
}

void ChromeShellDelegate::SetUpEnvironmentForLockedFullscreen(bool locked) {
  // Reset the clipboard and kill dev tools when entering or exiting locked
  // fullscreen (security concerns).
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  content::DevToolsAgentHost::DetachAllClients();

  // TODO(crbug/1243104): This might be interesting for DLP to change.
  // Disable both screenshots and video screen captures via the capture mode
  // feature.
  ChromeCaptureModeDelegate::Get()->SetIsScreenCaptureLocked(locked);

  // Get the primary profile as that's what ARC and Assistant are attached to.
  const Profile* profile = ProfileManager::GetPrimaryUserProfile();
  // Commands below require profile.
  if (!profile) {
    return;
  }

  // Disable ARC while in the locked fullscreen mode.
  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  if (arc_session_manager && arc::IsArcAllowedForProfile(profile)) {
    if (locked) {
      // Disable ARC, preserve data.
      arc_session_manager->RequestDisable();
    } else {
      // Re-enable ARC if needed.
      if (arc::IsArcPlayStoreEnabledForProfile(profile))
        arc_session_manager->RequestEnable();
    }
  }

  if (assistant::IsAssistantAllowedForProfile(profile) ==
      chromeos::assistant::AssistantAllowedState::ALLOWED) {
    ash::AssistantState::Get()->NotifyLockedFullScreenStateChanged(locked);
  }
}

bool ChromeShellDelegate::IsUiDevToolsStarted() const {
  return ChromeBrowserMainExtraPartsViews::Get()->GetUiDevToolsServerInstance();
}

void ChromeShellDelegate::StartUiDevTools() {
  ChromeBrowserMainExtraPartsViews::Get()->CreateUiDevTools();
}

void ChromeShellDelegate::StopUiDevTools() {
  ChromeBrowserMainExtraPartsViews::Get()->DestroyUiDevTools();
}

int ChromeShellDelegate::GetUiDevToolsPort() const {
  return ChromeBrowserMainExtraPartsViews::Get()
      ->GetUiDevToolsServerInstance()
      ->port();
}

bool ChromeShellDelegate::IsLoggingRedirectDisabled() const {
  if (disable_logging_redirect_for_testing.has_value())
    return disable_logging_redirect_for_testing.value();

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableLoggingRedirect);
}

base::FilePath ChromeShellDelegate::GetPrimaryUserDownloadsFolder() const {
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return base::FilePath();

  Profile* user_profile =
      ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (user_profile)
    return file_manager::util::GetDownloadsFolderForProfile(user_profile);

  return base::FilePath();
}

std::unique_ptr<app_restore::AppLaunchInfo>
ChromeShellDelegate::GetAppLaunchDataForDeskTemplate(
    aura::Window* window) const {
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(active_user);
  Profile* user_profile =
      ash::ProfileHelper::Get()->GetProfileByUser(active_user);
  if (!user_profile)
    return nullptr;

  if (!IsWindowSupportedForDeskTemplate(window, user_profile))
    return nullptr;

  // Get |full_restore_data| from FullRestoreSaveHandler which contains all
  // restoring information for all apps running on the device.
  const app_restore::RestoreData* full_restore_data =
      full_restore::FullRestoreSaveHandler::GetInstance()->GetRestoreData(
          user_profile->GetPath());
  DCHECK(full_restore_data);

  const std::string app_id = full_restore::GetAppId(window);
  DCHECK(!app_id.empty());

  const int32_t window_id = window->GetProperty(app_restore::kWindowIdKey);
  std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id);
  auto* tab_strip_model = GetTabstripModelForWindowIfAny(window);
  if (tab_strip_model) {
    app_launch_info->urls = GetURLsIfApplicable(tab_strip_model);
    app_launch_info->active_tab_index = tab_strip_model->active_index();
  }
  const std::string* app_name =
      window->GetProperty(app_restore::kBrowserAppNameKey);
  if (app_name)
    app_launch_info->app_name = *app_name;

  // Read all other relevant app launching information from
  // |app_restore_data| to |app_launch_info|.
  const app_restore::AppRestoreData* app_restore_data =
      full_restore_data->GetAppRestoreData(app_id, window_id);
  if (app_restore_data) {
    app_launch_info->app_type_browser = app_restore_data->app_type_browser;
    app_launch_info->event_flag = app_restore_data->event_flag;
    app_launch_info->container = app_restore_data->container;
    app_launch_info->disposition = app_restore_data->disposition;
    app_launch_info->file_paths = app_restore_data->file_paths;
    if (app_restore_data->intent.has_value() &&
        app_restore_data->intent.value()) {
      app_launch_info->intent = app_restore_data->intent.value()->Clone();
    }
  }

  auto& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(user_profile)
          ->AppRegistryCache();
  const apps::mojom::AppType app_type = app_registry_cache.GetAppType(app_id);
  if (app_id != extension_misc::kChromeAppId &&
      (app_type == apps::mojom::AppType::kExtension ||
       app_type == apps::mojom::AppType::kWeb)) {
    // If these values are not present, we will not be able to restore the
    // application. See http://crbug.com/1232520 for more information.
    if (!app_launch_info->container.has_value() ||
        !app_launch_info->disposition.has_value()) {
      return nullptr;
    }
  }

  return app_launch_info;
}

void ChromeShellDelegate::OpenFeedbackPageForPersistentDesksBar() {
  chrome::OpenFeedbackDialog(/*browser=*/nullptr,
                             chrome::kFeedbackSourceBentoBar,
                             /*description_template=*/"#BentoBar\n\n");
}

desks_storage::DeskModel* ChromeShellDelegate::GetDeskModel() {
  return DesksClient::Get()->GetDeskModel();
}

void ChromeShellDelegate::GetFaviconForUrl(
    const std::string& page_url,
    int desired_icon_size,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) const {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile(),
          ServiceAccessType::EXPLICIT_ACCESS);

  favicon_service->GetRawFaviconForPageURL(
      GURL(page_url), {favicon_base::IconType::kFavicon}, desired_icon_size,
      /*fallback_to_host=*/false, std::move(callback), tracker);
}

void ChromeShellDelegate::GetIconForAppId(
    const std::string& app_id,
    int desired_icon_size,
    base::OnceCallback<void(apps::mojom::IconValuePtr icon_value)> callback)
    const {
  auto* app_service_proxy = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile());
  if (!app_service_proxy) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  app_service_proxy->LoadIcon(
      app_service_proxy->AppRegistryCache().GetAppType(app_id), app_id,
      apps::mojom::IconType::kStandard, desired_icon_size,
      /*allow_placeholder_icon=*/false, std::move(callback));
}

void ChromeShellDelegate::LaunchAppsFromTemplate(
    std::unique_ptr<ash::DeskTemplate> desk_template) {
  DesksClient::Get()->LaunchAppsFromTemplate(std::move(desk_template));
}

// static
void ChromeShellDelegate::SetDisableLoggingRedirectForTesting(bool value) {
  disable_logging_redirect_for_testing = value;
}

// static
void ChromeShellDelegate::ResetDisableLoggingRedirectForTesting() {
  disable_logging_redirect_for_testing.reset();
}

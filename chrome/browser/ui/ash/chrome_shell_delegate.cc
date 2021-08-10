// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <memory>
#include <utility>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screenshot_delegate.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "cc/input/touch_action.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/ash/back_gesture_contextual_nudge_delegate.h"
#include "chrome/browser/ui/ash/chrome_accessibility_delegate.h"
#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/ash/tab_scrubber.h"
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
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/app_restore_data.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/restore_data.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
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
  // For now we'll ignore ARC, crostini and lacros windows in desk template.
  const ash::AppType app_type =
      static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
  if (app_type != ash::AppType::BROWSER &&
      app_type != ash::AppType::CHROME_APP &&
      app_type != ash::AppType::SYSTEM_APP) {
    return false;
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

aura::Window* ChromeShellDelegate::CreateBrowserForTabDrop(
    aura::Window* source_window,
    const ui::OSExchangeData& drop_data) {
  DCHECK(ash::features::IsWebUITabStripTabDragIntegrationEnabled());

  BrowserView* source_view = BrowserView::GetBrowserViewForNativeWindow(
      source_window->GetToplevelWindow());
  if (!source_view)
    return nullptr;

  Browser::CreateParams params = source_view->browser()->create_params();
  params.user_gesture = true;
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = Browser::Create(params);
  if (!browser)
    return nullptr;

  if (!tab_strip_ui::DropTabsInNewBrowser(browser, drop_data)) {
    browser->window()->Close();
    return nullptr;
  }

  // TODO(https://crbug.com/1069869): evaluate whether the above
  // failures can happen in valid states, and if so whether we need to
  // reflect failure in UX.

  // TODO(crbug.com/1225667): Loosen restriction for SplitViewController to be
  // able to snap a window without calling Show(). It will simplify the logic
  // without having to set and clear ash::kIsDraggingTabsKey by calling Show()
  // after snapping the window to the right place.

  // We need to mark the newly created window with |ash::kIsDraggingTabsKey|
  // and clear it afterwards in order to prevent
  // SplitViewController::AutoSnapController from snapping it on Show().
  aura::Window* window = browser->window()->GetNativeWindow();
  window->SetProperty(ash::kIsDraggingTabsKey, true);
  browser->window()->Show();
  window->ClearProperty(ash::kIsDraggingTabsKey);
  return window;
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

std::unique_ptr<ash::ScreenshotDelegate>
ChromeShellDelegate::CreateScreenshotDelegate() {
  return std::make_unique<ChromeScreenshotGrabber>();
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

std::unique_ptr<full_restore::AppLaunchInfo>
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
  const full_restore::RestoreData* full_restore_data =
      full_restore::FullRestoreSaveHandler::GetInstance()->GetRestoreData(
          user_profile->GetPath());
  DCHECK(full_restore_data);

  const std::string app_id = full_restore::GetAppId(window);
  DCHECK(!app_id.empty());

  const int32_t window_id = window->GetProperty(full_restore::kWindowIdKey);
  std::unique_ptr<full_restore::AppLaunchInfo> app_launch_info =
      std::make_unique<full_restore::AppLaunchInfo>(app_id, window_id);
  auto* tab_strip_model = GetTabstripModelForWindowIfAny(window);
  if (tab_strip_model) {
    app_launch_info->urls = GetURLsIfApplicable(tab_strip_model);
    app_launch_info->active_tab_index = tab_strip_model->active_index();
  }
  const std::string* app_name =
      window->GetProperty(full_restore::kBrowserAppNameKey);
  if (app_name)
    app_launch_info->app_name = *app_name;

  // Read all other relevant app launching information from
  // |app_restore_data| to |app_launch_info|.
  const full_restore::AppRestoreData* app_restore_data =
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

  return app_launch_info;
}

void ChromeShellDelegate::OpenFeedbackPageForPersistentDesksBar() {
  chrome::OpenFeedbackDialog(/*browser=*/nullptr,
                             chrome::kFeedbackSourceBentoBar,
                             /*description_template=*/"#BentoBar\n\n");
}

// static
void ChromeShellDelegate::SetDisableLoggingRedirectForTesting(bool value) {
  disable_logging_redirect_for_testing = value;
}

// static
void ChromeShellDelegate::ResetDisableLoggingRedirectForTesting() {
  disable_logging_redirect_for_testing.reset();
}

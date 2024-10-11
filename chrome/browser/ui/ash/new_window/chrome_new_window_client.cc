// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/new_window/chrome_new_window_client.h"

#include <string>
#include <utility>
#include <vector>

#include "apps/launcher.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/apps/calculator_app/calculator_app_utils.h"
#include "chrome/browser/ash/system_web_apps/apps/camera_app/chrome_camera_app_ui_delegate.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/network/network_portal_signin_window.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_base.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_constants.h"

namespace {

void RestoreTabUsingProfile(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  service->RestoreMostRecentEntry(nullptr);
}

bool IsIncognitoAllowed() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile && IncognitoModePrefs::IsIncognitoAllowed(profile);
}

// Returns URL path and query without the "/" prefix. For example, for the URL
// "chrome://settings/networks/?type=WiFi" returns "networks/?type=WiFi".
std::string GetPathAndQuery(const GURL& url) {
  std::string result = url.path();
  if (!result.empty() && result[0] == '/')
    result.erase(0, 1);
  if (url.has_query()) {
    result += '?';
    result += url.query();
  }
  return result;
}

feedback::FeedbackSource MapToChromeSource(
    ash::NewWindowDelegate::FeedbackSource source) {
  switch (source) {
    case ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceAsh:
      return feedback::FeedbackSource::kFeedbackSourceAsh;
    case ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceAssistant:
      return feedback::FeedbackSource::kFeedbackSourceAssistant;
    case ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceQuickAnswers:
      return feedback::FeedbackSource::kFeedbackSourceQuickAnswers;
    case ash::NewWindowDelegate::FeedbackSource::
        kFeedbackSourceChannelIndicator:
      return feedback::FeedbackSource::kFeedbackSourceChannelIndicator;
  }
}

// When the Files SWA is enabled: Open Files SWA.
// Returns true if it opens the SWA.
// `target_directory` is optional, if provided it opens the Files SWA in the
// given directory, instead of the default directory.
bool OpenFilesSwa(Profile* const profile,
                  base::FilePath target_directory = {}) {
  GURL directory_url;
  if (!target_directory.empty() &&
      !file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, target_directory, file_manager::util::GetFileManagerURL(),
          &directory_url)) {
    LOG(WARNING) << "Failed to convert the path to FileSystemURL: "
                 << target_directory << " using the default directory";
  }

  std::u16string title;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  GURL files_swa_url =
      ::file_manager::util::GetFileManagerMainPageUrlWithParams(
          ui::SelectFileDialog::SELECT_NONE, title,
          /*current_directory_url=*/directory_url,
          /*selection_url=*/{},
          /*target_name=*/{}, &file_type_info,
          /*file_type_index=*/0,
          /*search_query=*/{},
          /*show_android_picker_apps=*/false,
          /*volume_filter=*/{});

  ash::SystemAppLaunchParams params;
  params.url = files_swa_url;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                               params);
  return true;
}

}  // namespace

ChromeNewWindowClient::ChromeNewWindowClient() {
  arc::ArcIntentHelperBridge::SetControlCameraAppDelegate(this);
}

ChromeNewWindowClient::~ChromeNewWindowClient() {
  arc::ArcIntentHelperBridge::SetControlCameraAppDelegate(nullptr);
}

// static
ChromeNewWindowClient* ChromeNewWindowClient::Get() {
  return static_cast<ChromeNewWindowClient*>(
      ash::NewWindowDelegate::GetInstance());
}

// TabRestoreHelper is used to restore a tab. In particular when the user
// attempts to a restore a tab if the TabRestoreService hasn't finished loading
// this waits for it. Once the TabRestoreService finishes loading the tab is
// restored.
class ChromeNewWindowClient::TabRestoreHelper
    : public sessions::TabRestoreServiceObserver {
 public:
  TabRestoreHelper(ChromeNewWindowClient* delegate,
                   Profile* profile,
                   sessions::TabRestoreService* service)
      : delegate_(delegate), profile_(profile), tab_restore_service_(service) {
    tab_restore_service_->AddObserver(this);
  }

  TabRestoreHelper(const TabRestoreHelper&) = delete;
  TabRestoreHelper& operator=(const TabRestoreHelper&) = delete;

  ~TabRestoreHelper() override { tab_restore_service_->RemoveObserver(this); }

  sessions::TabRestoreService* tab_restore_service() {
    return tab_restore_service_;
  }

  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override {
  }

  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override {
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override {
    RestoreTabUsingProfile(profile_);
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

 private:
  raw_ptr<ChromeNewWindowClient> delegate_;
  raw_ptr<Profile> profile_;
  raw_ptr<sessions::TabRestoreService> tab_restore_service_;
};

void ChromeNewWindowClient::NewTab() {
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (browser && browser->is_type_normal()) {
    chrome::NewTab(browser);
    return;
  }

  // Display a browser, setting the focus to the location bar after it is shown.
  {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    bool is_otr_forced =
        IncognitoModePrefs::ShouldOpenSubsequentBrowsersInIncognito(
            *base::CommandLine::ForCurrentProcess(), profile->GetPrefs());

    if (is_otr_forced) {
      profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    }
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    browser = displayer.browser();
    chrome::NewTab(browser);
  }

  browser->SetFocusToLocationBar();
}

void ChromeNewWindowClient::NewWindow(bool is_incognito,
                                      bool should_trigger_session_restore) {
  if (is_incognito && !IsIncognitoAllowed())
    return;

  Browser* browser = chrome::FindBrowserWithActiveWindow();
  Profile* profile = (browser && browser->profile())
                         ? browser->profile()->GetOriginalProfile()
                         : ProfileManager::GetActiveUserProfile();
  chrome::NewEmptyWindow(
      is_incognito ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                   : profile,
      should_trigger_session_restore);
}

void ChromeNewWindowClient::NewWindowForDetachingTab(
    aura::Window* source_window,
    const ui::OSExchangeData& drop_data,
    NewWindowForDetachingTabCallback closure) {
  BrowserView* source_view = BrowserView::GetBrowserViewForNativeWindow(
      source_window->GetToplevelWindow());
  if (!source_view) {
    std::move(closure).Run(/*new_window=*/nullptr);
    return;
  }

  Browser::CreateParams params = source_view->browser()->create_params();
  params.user_gesture = true;
  params.initial_show_state = ui::mojom::WindowShowState::kDefault;
  Browser* browser = Browser::Create(params);
  if (!browser) {
    std::move(closure).Run(/*new_window=*/nullptr);
    return;
  }

  if (!tab_strip_ui::DropTabsInNewBrowser(browser, drop_data)) {
    browser->window()->Close();
    std::move(closure).Run(/*new_window=*/nullptr);
    return;
  }

  // TODO(crbug.com/40126106): evaluate whether the above
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
  std::move(closure).Run(window);
}

namespace {
WindowOpenDisposition ToWindowOpenDisposition(
    ash::NewWindowDelegate::Disposition disposition) {
  switch (disposition) {
    case ash::NewWindowDelegate::Disposition::kNewForegroundTab:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case ash::NewWindowDelegate::Disposition::kNewWindow:
      return WindowOpenDisposition::NEW_WINDOW;
    case ash::NewWindowDelegate::Disposition::kOffTheRecord:
      return WindowOpenDisposition::OFF_THE_RECORD;
    case ash::NewWindowDelegate::Disposition::kSwitchToTab:
      return WindowOpenDisposition::SWITCH_TO_TAB;
  }
}
}  // namespace

void ChromeNewWindowClient::OpenUrl(const GURL& url,
                                    OpenUrlFrom from,
                                    Disposition disposition) {
  // Opens a URL in a new tab. If the URL is for a chrome://settings page,
  // opens settings in a new window.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if ((url.SchemeIs(url::kAboutScheme) ||
       url.SchemeIs(content::kChromeUIScheme))) {
    // Show browser settings (e.g. chrome://settings). This may open in a window
    // or a tab depending on feature SplitSettings.
    if (url.host() == chrome::kChromeUISettingsHost) {
      std::string sub_page = GetPathAndQuery(url);
      chrome::ShowSettingsSubPageForProfile(profile, sub_page);
      return;
    }
    // OS settings are shown in a window.
    if (url.host() == chrome::kChromeUIOSSettingsHost) {
      std::string sub_page = GetPathAndQuery(url);
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                                   sub_page);
      return;
    }
  }

  NavigateParams navigate_params(
      profile, url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));
  navigate_params.disposition = ToWindowOpenDisposition(disposition);

  // If the |from| is kUserInteraction, then the page will load with a user
  // activation. This means it will be able to autoplay media without
  // restriction.
  if (from == OpenUrlFrom::kUserInteraction)
    navigate_params.was_activated = blink::mojom::WasActivatedOption::kYes;

  Navigate(&navigate_params);

  if (navigate_params.browser) {
    // The browser window might be on another user's desktop, and hence not
    // visible. Ensure the browser becomes visible on this user's desktop.
    multi_user_util::MoveWindowToCurrentDesktop(
        navigate_params.browser->window()->GetNativeWindow());
  }

  auto* tab = navigate_params.navigated_or_inserted_contents.get();
  if (from == OpenUrlFrom::kArc && tab) {
    // Add a flag to remember this tab originated in the ARC context.
    tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                     std::make_unique<arc::ArcWebContentsData>(tab));
  }
}

void ChromeNewWindowClient::OpenCalculator() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  proxy->Launch(ash::calculator_app::GetInstalledCalculatorAppId(profile),
                ui::EF_NONE, apps::LaunchSource::kFromKeyboard);
}

void ChromeNewWindowClient::OpenFileManager() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  if (OpenFilesSwa(profile)) {
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);

  auto launch_files_app = [proxy](const apps::AppUpdate& update) {
    if (update.Readiness() != apps::Readiness::kReady) {
      LOG(WARNING)
          << "Couldn't launch Files app because it isn't ready, readiness: "
          << static_cast<int>(update.Readiness());
      return;
    }

    proxy->Launch(update.AppId(),
                  apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                      /*prefer_container=*/true),
                  apps::LaunchSource::kFromKeyboard);
  };

  bool result = proxy->AppRegistryCache().ForOneApp(
      file_manager::kFileManagerAppId, std::move(launch_files_app));
  DCHECK(result);
}

void ChromeNewWindowClient::OpenDownloadsFolder() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  base::FilePath target_directory =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  if (OpenFilesSwa(profile, target_directory)) {
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  auto downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  DCHECK(proxy);

  auto launch_files_app = [proxy,
                           downloads_path](const apps::AppUpdate& update) {
    if (update.Readiness() != apps::Readiness::kReady) {
      LOG(WARNING)
          << "Couldn't launch Files app because it isn't ready, readiness: "
          << static_cast<int>(update.Readiness());
      return;
    }

    std::vector<base::FilePath> launch_files;
    launch_files.push_back(downloads_path);
    proxy->LaunchAppWithFiles(
        update.AppId(),
        apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            /*prefer_container=*/true),
        apps::LaunchSource::kFromKeyboard, std::move(launch_files));
  };

  bool result = proxy->AppRegistryCache().ForOneApp(
      file_manager::kFileManagerAppId, launch_files_app);
  DCHECK(result);
}

void ChromeNewWindowClient::OpenCrosh() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::CROSH);
}

void ChromeNewWindowClient::OpenGetHelp() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  chrome::ShowHelpForProfile(profile, chrome::HELP_SOURCE_KEYBOARD);
}

void ChromeNewWindowClient::RestoreTab() {
  if (tab_restore_helper_.get()) {
    DCHECK(!tab_restore_helper_->tab_restore_service()->IsLoaded());
    return;
  }

  Browser* browser = chrome::FindBrowserWithActiveWindow();
  Profile* profile = browser ? browser->profile() : nullptr;
  if (!profile)
    profile = ProfileManager::GetActiveUserProfile();
  if (profile->IsOffTheRecord())
    return;
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (!service)
    return;

  if (service->IsLoaded()) {
    RestoreTabUsingProfile(profile);
  } else {
    tab_restore_helper_ =
        std::make_unique<TabRestoreHelper>(this, profile, service);
    service->LoadTabsFromLastSession();
  }
}

void ChromeNewWindowClient::ShowShortcutCustomizationApp() {
  chrome::ShowShortcutCustomizationApp(ProfileManager::GetActiveUserProfile());
}

void ChromeNewWindowClient::ShowTaskManager() {
  chrome::OpenTaskManager(nullptr);
}

void ChromeNewWindowClient::OpenDiagnostics() {
  chrome::ShowDiagnosticsApp(ProfileManager::GetActiveUserProfile());
}

void ChromeNewWindowClient::OpenFeedbackPage(
    FeedbackSource source,
    const std::string& description_template) {
  chrome::OpenFeedbackDialog(chrome::FindBrowserWithActiveWindow(),
                             MapToChromeSource(source), description_template);
}

void ChromeNewWindowClient::OpenPersonalizationHub() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::PERSONALIZATION);
}

void ChromeNewWindowClient::OpenCaptivePortalSignin(const GURL& url) {
  chromeos::NetworkPortalSigninWindow::Get()->Show(url);
}

void ChromeNewWindowClient::OpenFile(const base::FilePath& file_path) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  platform_util::OpenItem(profile, file_path,
                          platform_util::OpenItemType::OPEN_FILE,
                          platform_util::OpenOperationCallback());
}

void ChromeNewWindowClient::LaunchCameraApp(const std::string& queries,
                                            int32_t task_id) {
  DCHECK(IsCameraAppEnabled());
  ChromeCameraAppUIDelegate::CameraAppDialog::ShowIntent(
      queries, arc::GetArcWindow(task_id));
  apps::RecordAppLaunch(web_app::kCameraAppId, apps::LaunchSource::kFromArc);
}

void ChromeNewWindowClient::CloseCameraApp() {
  const ash::ShelfID shelf_id(web_app::kCameraAppId);
  AppWindowShelfItemController* const app_controller =
      ChromeShelfController::instance()
          ->shelf_model()
          ->GetAppWindowShelfItemController(shelf_id);
  if (!app_controller)
    return;

  DCHECK_LE(app_controller->window_count(), 1lu);
  if (app_controller->window_count() > 0)
    app_controller->windows().front()->Close();
}

bool ChromeNewWindowClient::IsCameraAppEnabled() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  auto* swa_manager = ash::SystemWebAppManager::Get(profile);
  return swa_manager &&
         swa_manager->IsAppEnabled(ash::SystemWebAppType::CAMERA);
}

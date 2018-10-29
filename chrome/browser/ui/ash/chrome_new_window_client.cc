// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_client.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/url_formatter/url_fixer.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/was_activated_option.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_constants.h"

namespace {

ChromeNewWindowClient* g_chrome_new_window_client_instance = nullptr;

void RestoreTabUsingProfile(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  service->RestoreMostRecentEntry(nullptr);
}

bool IsIncognitoAllowed() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile && profile->GetProfileType() != Profile::GUEST_PROFILE &&
         IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
             IncognitoModePrefs::DISABLED;
}

}  // namespace

ChromeNewWindowClient::ChromeNewWindowClient() : binding_(this) {
  arc::ArcIntentHelperBridge::SetOpenUrlDelegate(this);

  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(ash::mojom::kServiceName, &new_window_controller_);

  // Register this object as the client interface implementation.
  ash::mojom::NewWindowClientAssociatedPtrInfo ptr_info;
  binding_.Bind(mojo::MakeRequest(&ptr_info));
  new_window_controller_->SetClient(std::move(ptr_info));

  DCHECK(!g_chrome_new_window_client_instance);
  g_chrome_new_window_client_instance = this;
}

ChromeNewWindowClient::~ChromeNewWindowClient() {
  DCHECK_EQ(g_chrome_new_window_client_instance, this);
  g_chrome_new_window_client_instance = nullptr;

  arc::ArcIntentHelperBridge::SetOpenUrlDelegate(nullptr);
}

// static
ChromeNewWindowClient* ChromeNewWindowClient::Get() {
  return g_chrome_new_window_client_instance;
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
  ChromeNewWindowClient* delegate_;
  Profile* profile_;
  sessions::TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreHelper);
};

void ChromeNewWindowClient::NewTab() {
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (browser && browser->is_type_tabbed()) {
    chrome::NewTab(browser);
    return;
  }

  // Display a browser, setting the focus to the location bar after it is shown.
  {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        ProfileManager::GetActiveUserProfile());
    browser = displayer.browser();
    chrome::NewTab(browser);
  }

  browser->SetFocusToLocationBar(false);
}

void ChromeNewWindowClient::NewTabWithUrl(const GURL& url,
                                          bool from_user_interaction) {
  OpenUrlImpl(url, from_user_interaction);
}

void ChromeNewWindowClient::NewWindow(bool is_incognito) {
  if (is_incognito && !IsIncognitoAllowed())
    return;

  Browser* browser = chrome::FindBrowserWithActiveWindow();
  Profile* profile = (browser && browser->profile())
                         ? browser->profile()->GetOriginalProfile()
                         : ProfileManager::GetActiveUserProfile();
  chrome::NewEmptyWindow(is_incognito ? profile->GetOffTheRecordProfile()
                                      : profile);
}

void ChromeNewWindowClient::OpenFileManager() {
  using file_manager::kFileManagerAppId;
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  const extensions::ExtensionService* const service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service || !extensions::util::IsAppLaunchableWithoutEnabling(
                      kFileManagerAppId, profile)) {
    return;
  }

  const extensions::Extension* const extension =
      service->GetInstalledExtension(kFileManagerAppId);
  OpenApplication(CreateAppLaunchParamsUserContainer(
      profile, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      extensions::SOURCE_KEYBOARD));
}

void ChromeNewWindowClient::OpenCrosh() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  GURL crosh_url =
      extensions::TerminalExtensionHelper::GetCroshExtensionURL(profile);
  if (!crosh_url.is_valid())
    return;
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  content::WebContents* page = browser->OpenURL(content::OpenURLParams(
      crosh_url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_GENERATED, false));
  browser->window()->Show();
  browser->window()->Activate();
  page->Focus();
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
    tab_restore_helper_.reset(new TabRestoreHelper(this, profile, service));
    service->LoadTabsFromLastSession();
  }
}

void ChromeNewWindowClient::ShowKeyboardShortcutViewer() {
  keyboard_shortcut_viewer_util::ToggleKeyboardShortcutViewer();
}

void ChromeNewWindowClient::ShowTaskManager() {
  chrome::OpenTaskManager(nullptr);
}

void ChromeNewWindowClient::OpenFeedbackPage() {
  chrome::OpenFeedbackDialog(chrome::FindBrowserWithActiveWindow(),
                             chrome::kFeedbackSourceAsh);
}

void ChromeNewWindowClient::OpenUrlFromArc(const GURL& url) {
  if (!url.is_valid())
    return;

  GURL url_to_open = url;
  if (url.SchemeIs(url::kFileScheme) || url.SchemeIs(url::kContentScheme)) {
    // Chrome cannot open this URL. Read the contents via ARC content file
    // system with an external file URL.
    url_to_open = arc::ArcUrlToExternalFileUrl(url_to_open);
  }

  content::WebContents* tab =
      OpenUrlImpl(url_to_open, false /* from_user_interaction */);
  if (!tab)
    return;

  // Add a flag to remember this tab originated in the ARC context.
  tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                   std::make_unique<arc::ArcWebContentsData>());
}

void ChromeNewWindowClient::OpenWebAppFromArc(const GURL& url) {
  DCHECK(url.is_valid() && url.SchemeIs(url::kHttpsScheme));

  // Fetch the profile associated with ARC. This method should only be called
  // for a |url| which was installed via ARC, and so we want the web app that is
  // opened through here to be installed in the profile associated with ARC.
  // |user| may be null if sign-in hasn't happened yet
  const auto* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return;

  // |profile| may be null if sign-in has happened but the profile isn't loaded
  // yet.
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return;

  const extensions::Extension* extension =
      extensions::util::GetInstalledPwaForUrl(
          profile, url, extensions::LAUNCH_CONTAINER_WINDOW);
  if (!extension) {
    OpenUrlFromArc(url);
    return;
  }

  AppLaunchParams params = CreateAppLaunchParamsUserContainer(
      profile, extension, WindowOpenDisposition::NEW_WINDOW,
      extensions::SOURCE_ARC);
  params.override_url = url;
  content::WebContents* tab = OpenApplication(params);
  if (!tab)
    return;

  // Add a flag to remember this tab originated in the ARC context.
  tab->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                   std::make_unique<arc::ArcWebContentsData>());
}

content::WebContents* ChromeNewWindowClient::OpenUrlImpl(
    const GURL& url,
    bool from_user_interaction) {
  // If the url is for system settings, show the settings in a window instead of
  // a browser tab.
  if (url.GetContent() == "settings" &&
      (url.SchemeIs(url::kAboutScheme) ||
       url.SchemeIs(content::kChromeUIScheme))) {
    chrome::ShowSettingsSubPageForProfile(
        ProfileManager::GetActiveUserProfile(), /*sub_page=*/std::string());
    return nullptr;
  }

  NavigateParams navigate_params(
      ProfileManager::GetActiveUserProfile(), url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));

  if (from_user_interaction)
    navigate_params.was_activated = content::WasActivatedOption::kYes;

  Navigate(&navigate_params);

  if (navigate_params.browser) {
    // The browser window might be on another user's desktop, and hence not
    // visible. Ensure the browser becomes visible on this user's desktop.
    multi_user_util::MoveWindowToCurrentDesktop(
        navigate_params.browser->window()->GetNativeWindow());
  }
  return navigate_params.navigated_or_inserted_contents;
}

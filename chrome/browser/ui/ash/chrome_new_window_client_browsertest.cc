// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_client.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

using arc::mojom::ChromePage;

namespace {

constexpr char kTestUserName1[] = "test1@test.com";
constexpr char kTestUser1GaiaId[] = "1111111111";
constexpr char kTestUserName2[] = "test2@test.com";
constexpr char kTestUser2GaiaId[] = "2222222222";

void CreateAndStartUserSession(const AccountId& account_id) {
  using chromeos::ProfileHelper;
  using session_manager::SessionManager;

  user_manager::known_user::SetProfileRequiresPolicy(
      account_id, user_manager::ProfileRequiresPolicy::kNoPolicyRequired);
  const std::string user_id_hash =
      ProfileHelper::GetUserIdHashByUserIdForTesting(account_id.GetUserEmail());
  SessionManager::Get()->CreateSession(account_id, user_id_hash, false);
  ProfileHelper::GetProfileByUserIdHashForTest(user_id_hash);
  SessionManager::Get()->SessionStarted();
}

// Return the number of windows that hosts OS Settings.
size_t GetNumberOfSettingsWindows() {
  auto* browser_list = BrowserList::GetInstance();
  return std::count_if(browser_list->begin(), browser_list->end(),
                       [](Browser* browser) {
                         return web_app::IsBrowserForSystemWebApp(
                             browser, web_app::SystemAppType::SETTINGS);
                       });
}

// Give the underlying function a clearer name.
Browser* GetLastActiveBrowser() {
  return chrome::FindLastActive();
}

}  // namespace

using ChromeNewWindowClientBrowserTest = InProcessBrowserTest;

using ChromeNewWindowClientWebAppBrowserTest =
    web_app::WebAppNavigationBrowserTest;

// Tests that when we open a new window by pressing 'Ctrl-N', we should use the
// current active window's profile to determine on which profile's desktop we
// should open a new window.
//
// Test is flaky. See https://crbug.com/884118
IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest,
                       DISABLED_NewWindowForActiveWindowProfileTest) {
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName1, kTestUser1GaiaId));
  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  Browser* browser1 = CreateBrowser(profile1);
  // The newly created window should be created for the current active profile.
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/false,
      /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Login another user and make sure the current active user changes.
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName2, kTestUser2GaiaId));
  Profile* profile2 = ProfileManager::GetActiveUserProfile();
  EXPECT_NE(profile1, profile2);

  Browser* browser2 = CreateBrowser(profile2);
  // The newly created window should be created for the current active window's
  // profile, which is |profile2|.
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/false,
      /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile2);

  // After activating |browser1|, the newly created window should be created
  // against |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/false,
      /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Test for incognito windows.
  // The newly created incognito window should be created against the current
  // active |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile1);

  // The newly created incognito window should be created against the current
  // active |browser2|'s profile.
  browser2->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile2);
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, IncognitoDisabled) {
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName1, kTestUser2GaiaId));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Disabling incognito mode disables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), IncognitoModePrefs::Availability::kDisabled);
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Enabling incognito mode enables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(
      profile->GetPrefs(), IncognitoModePrefs::Availability::kEnabled);
  ChromeNewWindowClient::Get()->NewWindow(
      /*incognito=*/true, /*should_trigger_session_restore=*/true);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(GetLastActiveBrowser()->profile()->IsIncognitoProfile());
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientWebAppBrowserTest, OpenWebApp) {
  InstallTestWebApp();
  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  const char* key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;

  {
    // Calling OpenWebAppFromArc for a not installed HTTPS URL should open in
    // an ordinary browser tab.
    const GURL url("https://www.google.com");
    auto observer = GetTestNavigationObserver(url);
    ChromeNewWindowClient::Get()->OpenWebAppFromArc(url);
    observer->WaitForNavigationFinished();

    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
    EXPECT_FALSE(GetLastActiveBrowser()->is_type_app());
    content::WebContents* contents =
        GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, contents->GetLastCommittedURL());
    EXPECT_NE(nullptr, contents->GetUserData(key));
  }

  {
    // Calling OpenWebAppFromArc for an installed web app URL should open in an
    // app window.
    auto observer = GetTestNavigationObserver(app_url);
    ChromeNewWindowClient::Get()->OpenWebAppFromArc(app_url);
    observer->WaitForNavigationFinished();

    EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
    EXPECT_TRUE(GetLastActiveBrowser()->is_type_app());
    content::WebContents* contents =
        GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(app_url, contents->GetLastCommittedURL());
    EXPECT_NE(nullptr, contents->GetUserData(key));
  }
}

void TestOpenSettingFromArc(Browser* browser,
                            ChromePage page,
                            const GURL& expected_url,
                            size_t expected_setting_window_count) {
  // Install the Settings App.
  web_app::WebAppProvider::GetForTest(browser->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  ChromeNewWindowClient::Get()->OpenChromePageFromArc(page);

  // The above OpenChromePageFromArc() should trigger an asynchronous call to
  // launch OS Settings SWA. Flush Mojo calls so the browser window is created.
  web_app::FlushSystemWebAppLaunchesForTesting(
      GetLastActiveBrowser()->profile());

  EXPECT_EQ(expected_setting_window_count, GetNumberOfSettingsWindows());

  // The right settings are loaded (not just the settings main page).
  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_url, contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest,
                       OpenOSSettingsAppFromArc) {
  // Opening a browser setting should not open the OS setting window.
  TestOpenSettingFromArc(
      browser(), ChromePage::AUTOFILL,
      GURL("chrome://settings/").Resolve(chrome::kAutofillSubPage),
      /*expected_setting_window_count=*/0u);

  // But opening an OS setting should open the OS setting window.
  TestOpenSettingFromArc(
      browser(), ChromePage::POWER,
      GURL("chrome://os-settings/")
          .Resolve(chromeos::settings::mojom::kPowerSubpagePath),
      /*expected_setting_window_count=*/1u);
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, OpenAboutChromePage) {
  // Opening an about: chrome page opens a new tab, and not the Settings window.
  ChromeNewWindowClient::Get()->OpenChromePageFromArc(ChromePage::ABOUTHISTORY);
  EXPECT_EQ(0u, GetNumberOfSettingsWindows());

  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryURL), contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientWebAppBrowserTest,
                       OpenAppWithIntent) {
  ASSERT_TRUE(https_server().Start());
  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());

  // InstallTestWebApp() but with a ShareTarget definition added.
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = app_url;
  web_app_info->scope =
      https_server().GetURL(GetAppUrlHost(), GetAppScopePath());
  web_app_info->title = base::UTF8ToUTF16(GetAppName());
  web_app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
  apps::ShareTarget share_target;
  share_target.method = apps::ShareTarget::Method::kGet;
  share_target.action = app_url;
  share_target.params.text = "text";
  web_app_info->share_target = share_target;
  std::string id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->FlushMojoCallsForTesting();

  const char* arc_transition_key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;

  {
    // Calling OpenAppWithIntent for a not installed HTTPS URL should open in
    // an ordinary browser tab.
    const GURL url("https://www.google.com");
    arc::mojom::LaunchIntentPtr intent = arc::mojom::LaunchIntent::New();
    intent->action = arc::kIntentActionView;
    intent->data = url;

    auto observer = GetTestNavigationObserver(url);
    ChromeNewWindowClient::Get()->OpenAppWithIntent(url, std::move(intent));
    observer->WaitForNavigationFinished();

    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
    EXPECT_FALSE(GetLastActiveBrowser()->is_type_app());
    content::WebContents* contents =
        GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, contents->GetLastCommittedURL());
    EXPECT_NE(nullptr, contents->GetUserData(arc_transition_key));
  }

  {
    // Calling OpenAppWithIntent for an installed web app URL should open the
    // intent in an app window.
    GURL launch_url =
        https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
    arc::mojom::LaunchIntentPtr intent = arc::mojom::LaunchIntent::New();
    intent->action = arc::kIntentActionView;
    intent->data = launch_url;

    auto observer = GetTestNavigationObserver(launch_url);
    ChromeNewWindowClient::Get()->OpenAppWithIntent(app_url, std::move(intent));
    observer->WaitForNavigationFinished();

    EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
    EXPECT_TRUE(GetLastActiveBrowser()->is_type_app());
    content::WebContents* contents =
        GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(launch_url, contents->GetLastCommittedURL());
    EXPECT_NE(nullptr, contents->GetUserData(arc_transition_key));
  }
  {
    // Calling OpenAppWithIntent for an installed web app URL with shared
    // content should open the app with the share data passed through.
    arc::mojom::LaunchIntentPtr intent = arc::mojom::LaunchIntent::New();
    intent->action = arc::kIntentActionSend;
    intent->extra_text = "shared_text";

    GURL::Replacements add_query;
    add_query.SetQueryStr("text=shared_text");
    GURL launch_url = app_url.ReplaceComponents(add_query);

    auto observer = GetTestNavigationObserver(launch_url);
    ChromeNewWindowClient::Get()->OpenAppWithIntent(app_url, std::move(intent));
    observer->WaitForNavigationFinished();

    EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
    EXPECT_TRUE(GetLastActiveBrowser()->is_type_app());
    content::WebContents* contents =
        GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(launch_url, contents->GetLastCommittedURL());
  }
}

void TestOpenChromePage(ChromePage page, const GURL& expected_url) {
  ChromeNewWindowClient::Get()->OpenChromePageFromArc(page);
  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_url, contents->GetVisibleURL());
}

class TestSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  void ShowChromePageForProfile(Profile* profile,
                                const GURL& gurl,
                                int64_t display_id) override {
    last_navigation_url_ = gurl;
    chrome::SettingsWindowManager::ShowChromePageForProfile(profile, gurl,
                                                            display_id);
  }
  const GURL& last_navigation_url() { return last_navigation_url_; }

 private:
  GURL last_navigation_url_;
};

void TestOpenOSSettingsChromePage(ChromePage page, const GURL& expected_url) {
  TestSettingsWindowManager test_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(&test_manager);

  ChromeNewWindowClient::Get()->OpenChromePageFromArc(page);
  web_app::FlushSystemWebAppLaunchesForTesting(
      ProfileManager::GetActiveUserProfile());

  EXPECT_EQ(expected_url, test_manager.last_navigation_url());

  chrome::SettingsWindowManager::SetInstanceForTesting(nullptr);
}

void TestAllOSSettingPages(const GURL& base_url) {
  TestOpenOSSettingsChromePage(ChromePage::MAIN, base_url);
  TestOpenOSSettingsChromePage(
      ChromePage::MULTIDEVICE,
      base_url.Resolve(chromeos::settings::mojom::kMultiDeviceSectionPath));
  TestOpenOSSettingsChromePage(
      ChromePage::WIFI,
      base_url.Resolve(chromeos::settings::mojom::kWifiNetworksSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::POWER,
      base_url.Resolve(chromeos::settings::mojom::kPowerSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::BLUETOOTH,
      base_url.Resolve(
          chromeos::settings::mojom::kBluetoothDevicesSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::DATETIME,
      base_url.Resolve(chromeos::settings::mojom::kDateAndTimeSectionPath));
  TestOpenOSSettingsChromePage(
      ChromePage::DISPLAY,
      base_url.Resolve(chromeos::settings::mojom::kDisplaySubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::HELP,
      base_url.Resolve(chromeos::settings::mojom::kAboutChromeOsSectionPath));
  TestOpenOSSettingsChromePage(
      ChromePage::ACCOUNTS,
      base_url.Resolve(
          chromeos::settings::mojom::kManageOtherPeopleSubpagePathV2));
  TestOpenOSSettingsChromePage(
      ChromePage::BLUETOOTHDEVICES,
      base_url.Resolve(
          chromeos::settings::mojom::kBluetoothDevicesSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::CHANGEPICTURE,
      base_url.Resolve(chromeos::settings::mojom::kChangePictureSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::CUPSPRINTERS,
      base_url.Resolve(chromeos::settings::mojom::kPrintingDetailsSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::KEYBOARDOVERLAY,
      base_url.Resolve(chromeos::settings::mojom::kKeyboardSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::OSLANGUAGESINPUT,
      base_url.Resolve(chromeos::settings::mojom::kInputSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::OSLANGUAGESLANGUAGES,
      base_url.Resolve(chromeos::settings::mojom::kLanguagesSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::LOCKSCREEN,
      base_url.Resolve(
          chromeos::settings::mojom::kSecurityAndSignInSubpagePathV2));
  TestOpenOSSettingsChromePage(
      ChromePage::MANAGEACCESSIBILITY,
      base_url.Resolve(
          chromeos::settings::mojom::kManageAccessibilitySubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::NETWORKSTYPEVPN,
      base_url.Resolve(chromeos::settings::mojom::kVpnDetailsSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::POINTEROVERLAY,
      base_url.Resolve(chromeos::settings::mojom::kPointersSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::SMARTPRIVACY,
      base_url.Resolve(chromeos::settings::mojom::kSmartPrivacySubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::STORAGE,
      base_url.Resolve(chromeos::settings::mojom::kStorageSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::MANAGEACCESSIBILITYTTS,
      base_url.Resolve(chromeos::settings::mojom::kTextToSpeechSubpagePath));
}

void TestAllBrowserSettingPages(const GURL& base_url) {
  TestOpenChromePage(ChromePage::PRIVACY,
                     base_url.Resolve(chrome::kPrivacySubPage));
  TestOpenChromePage(ChromePage::APPEARANCE,
                     base_url.Resolve(chrome::kAppearanceSubPage));
  TestOpenChromePage(ChromePage::AUTOFILL,
                     base_url.Resolve(chrome::kAutofillSubPage));
  TestOpenChromePage(ChromePage::CLEARBROWSERDATA,
                     base_url.Resolve(chrome::kClearBrowserDataSubPage));
  TestOpenChromePage(ChromePage::CLOUDPRINTERS,
                     base_url.Resolve(chrome::kCloudPrintersSubPage));
  TestOpenChromePage(ChromePage::DOWNLOADS,
                     base_url.Resolve(chrome::kDownloadsSubPage));
  TestOpenChromePage(ChromePage::ONSTARTUP,
                     base_url.Resolve(chrome::kOnStartupSubPage));
  TestOpenChromePage(ChromePage::PASSWORDS,
                     base_url.Resolve(chrome::kPasswordManagerSubPage));
  TestOpenChromePage(ChromePage::RESET,
                     base_url.Resolve(chrome::kResetSubPage));
  TestOpenChromePage(ChromePage::SEARCH,
                     base_url.Resolve(chrome::kSearchSubPage));
  TestOpenChromePage(ChromePage::SYNCSETUP,
                     base_url.Resolve(chrome::kSyncSetupSubPage));
  TestOpenChromePage(ChromePage::LANGUAGES,
                     base_url.Resolve(chrome::kLanguagesSubPage));
}

void TestAllAboutPages() {
  TestOpenChromePage(ChromePage::ABOUTDOWNLOADS,
                     GURL(chrome::kChromeUIDownloadsURL));
  TestOpenChromePage(ChromePage::ABOUTHISTORY,
                     GURL(chrome::kChromeUIHistoryURL));
  TestOpenChromePage(ChromePage::ABOUTBLANK, GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, TestOpenChromePage) {
  // Install the Settings App.
  web_app::WebAppProvider::GetForTest(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  TestAllOSSettingPages(GURL(chrome::kChromeUIOSSettingsURL));
  TestAllBrowserSettingPages(GURL(chrome::kChromeUISettingsURL));
  TestAllAboutPages();
}

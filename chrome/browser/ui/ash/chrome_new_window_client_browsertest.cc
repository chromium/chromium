// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_client.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/bookmark_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
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
      account_id,
      user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired);
  const std::string user_id_hash =
      ProfileHelper::GetUserIdHashByUserIdForTesting(account_id.GetUserEmail());
  SessionManager::Get()->CreateSession(account_id, user_id_hash, false);
  ProfileHelper::GetProfileByUserIdHashForTest(user_id_hash);
  SessionManager::Get()->SessionStarted();
}

class SettingsTestObserver : public chrome::SettingsWindowManagerObserver {
 public:
  void OnNewSettingsWindow(Browser* settings_browser) override {
    ++new_settings_count_;
  }
  int new_settings_count_ = 0;
};

// Give the underlying function a clearer name.
Browser* GetLastActiveBrowser() {
  return chrome::FindLastActive();
}

}  // namespace

using ChromeNewWindowClientBrowserTest = InProcessBrowserTest;

using ChromeNewWindowClientWebAppBrowserTest =
    extensions::test::BookmarkAppNavigationBrowserTest;

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
  ChromeNewWindowClient::Get()->NewWindow(/*incognito=*/false);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Login another user and make sure the current active user changes.
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName2, kTestUser2GaiaId));
  Profile* profile2 = ProfileManager::GetActiveUserProfile();
  EXPECT_NE(profile1, profile2);

  Browser* browser2 = CreateBrowser(profile2);
  // The newly created window should be created for the current active window's
  // profile, which is |profile2|.
  ChromeNewWindowClient::Get()->NewWindow(/*incognito=*/false);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile2);

  // After activating |browser1|, the newly created window should be created
  // against |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(/*incognito=*/false);
  EXPECT_EQ(GetLastActiveBrowser()->profile(), profile1);

  // Test for incognito windows.
  // The newly created incoginito window should be created against the current
  // active |browser1|'s profile.
  browser1->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(/*incognito=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile1);

  // The newly created incoginito window should be created against the current
  // active |browser2|'s profile.
  browser2->window()->Show();
  ChromeNewWindowClient::Get()->NewWindow(/*incognito=*/true);
  EXPECT_EQ(GetLastActiveBrowser()->profile()->GetOriginalProfile(), profile2);
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, IncognitoDisabled) {
  CreateAndStartUserSession(
      AccountId::FromUserEmailGaiaId(kTestUserName1, kTestUser2GaiaId));
  Profile* profile = ProfileManager::GetActiveUserProfile();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Disabling incognito mode disables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(profile->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  ChromeNewWindowClient::Get()->NewWindow(/*incognito=*/true);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Enabling incognito mode enables creation of new incognito windows.
  IncognitoModePrefs::SetAvailability(profile->GetPrefs(),
                                      IncognitoModePrefs::ENABLED);
  ChromeNewWindowClient::Get()->NewWindow(/*incognito=*/true);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_TRUE(GetLastActiveBrowser()->profile()->IsIncognitoProfile());
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientWebAppBrowserTest, OpenWebApp) {
  InstallTestBookmarkApp();
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
                            int expected_setting_window_count) {
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  SettingsTestObserver observer;
  auto* settings = chrome::SettingsWindowManager::GetInstance();
  settings->AddObserver(&observer);

  ChromeNewWindowClient::Get()->OpenChromePageFromArc(page);
  EXPECT_EQ(expected_setting_window_count, observer.new_settings_count_);

  // The right settings are loaded (not just the settings main page).
  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_url, contents->GetVisibleURL());

  settings->RemoveObserver(&observer);
}

class ChromeNewWindowClientBrowserTestWithSplitSettings
    : public ChromeNewWindowClientBrowserTest {
 public:
  ChromeNewWindowClientBrowserTestWithSplitSettings() {
    feature_list_.InitAndEnableFeature(chromeos::features::kSplitSettings);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTestWithSplitSettings,
                       OpenOSSettingsAppFromArc) {
  // When flag is on, opening a browser setting should not open the OS setting
  // window.
  TestOpenSettingFromArc(
      browser(), ChromePage::AUTOFILL,
      GURL("chrome://settings/").Resolve(chrome::kAutofillSubPage),
      /*expected_setting_window_count=*/0);

  // But opening an OS setting should open the OS setting window.
  TestOpenSettingFromArc(
      browser(), ChromePage::POWER,
      GURL("chrome://os-settings/").Resolve(chrome::kPowerSubPage),
      /*expected_setting_window_count=*/1);
}

class ChromeNewWindowClientBrowserTestWithoutSplitSettings
    : public ChromeNewWindowClientBrowserTest {
 public:
  ChromeNewWindowClientBrowserTestWithoutSplitSettings() {
    feature_list_.InitAndDisableFeature(chromeos::features::kSplitSettings);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug/950007): This should be removed when the split is complete.
IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTestWithoutSplitSettings,
                       OpenSettingsAppFromArc) {
  // When flag is off, opening a browser setting should open the setting window.
  TestOpenSettingFromArc(
      browser(), ChromePage::AUTOFILL,
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kAutofillSubPage),
      /*expected_setting_window_count=*/1);

  // And opening an OS setting should reuse that window.
  TestOpenSettingFromArc(
      browser(), ChromePage::POWER,
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kPowerSubPage),
      /*expected_setting_window_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTest, OpenAboutChromePage) {
  SettingsTestObserver observer;
  auto* settings = chrome::SettingsWindowManager::GetInstance();
  settings->AddObserver(&observer);

  // Opening an about: chrome page opens a new tab, and not the Settings window.
  ChromeNewWindowClient::Get()->OpenChromePageFromArc(ChromePage::ABOUTHISTORY);
  EXPECT_EQ(0, observer.new_settings_count_);

  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryURL), contents->GetVisibleURL());

  settings->RemoveObserver(&observer);
}

void TestOpenChromePage(ChromePage page, const GURL& expected_url) {
  ChromeNewWindowClient::Get()->OpenChromePageFromArc(page);
  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_url, contents->GetVisibleURL());
}

void TestAllOSSettingPages(const GURL& base_url) {
  TestOpenChromePage(ChromePage::MAIN, base_url);
  TestOpenChromePage(ChromePage::MULTIDEVICE,
                     base_url.Resolve(chrome::kMultideviceSubPage));
  TestOpenChromePage(ChromePage::WIFI,
                     base_url.Resolve(chrome::kWiFiSettingsSubPage));
  TestOpenChromePage(ChromePage::POWER,
                     base_url.Resolve(chrome::kPowerSubPage));
  TestOpenChromePage(ChromePage::BLUETOOTH,
                     base_url.Resolve(chrome::kBluetoothSubPage));
  TestOpenChromePage(ChromePage::DATETIME,
                     base_url.Resolve(chrome::kDateTimeSubPage));
  TestOpenChromePage(ChromePage::DISPLAY,
                     base_url.Resolve(chrome::kDisplaySubPage));
  TestOpenChromePage(ChromePage::HELP, base_url.Resolve(chrome::kHelpSubPage));
  TestOpenChromePage(ChromePage::ACCOUNTS,
                     base_url.Resolve(chrome::kAccountSubPage));
  TestOpenChromePage(ChromePage::BLUETOOTHDEVICES,
                     base_url.Resolve(chrome::kBluetoothSubPage));
  TestOpenChromePage(ChromePage::CHANGEPICTURE,
                     base_url.Resolve(chrome::kChangePictureSubPage));
  TestOpenChromePage(ChromePage::CUPSPRINTERS,
                     base_url.Resolve(chrome::kNativePrintingSettingsSubPage));
  TestOpenChromePage(ChromePage::KEYBOARDOVERLAY,
                     base_url.Resolve(chrome::kKeyboardOverlaySubPage));
  TestOpenChromePage(ChromePage::LANGUAGES,
                     base_url.Resolve(chrome::kLanguageSubPage));
  TestOpenChromePage(ChromePage::LOCKSCREEN,
                     base_url.Resolve(chrome::kLockScreenSubPage));
  TestOpenChromePage(ChromePage::MANAGEACCESSIBILITY,
                     base_url.Resolve(chrome::kManageAccessibilitySubPage));
  TestOpenChromePage(ChromePage::NETWORKSTYPEVPN,
                     base_url.Resolve(chrome::kVPNSettingsSubPage));
  TestOpenChromePage(ChromePage::POINTEROVERLAY,
                     base_url.Resolve(chrome::kPointerOverlaySubPage));
  TestOpenChromePage(ChromePage::RESET,
                     base_url.Resolve(chrome::kResetSubPage));
  TestOpenChromePage(ChromePage::STORAGE,
                     base_url.Resolve(chrome::kStorageSubPage));
  TestOpenChromePage(ChromePage::SYNCSETUP,
                     base_url.Resolve(chrome::kSyncSetupSubPage));
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
  TestOpenChromePage(ChromePage::SEARCH,
                     base_url.Resolve(chrome::kSearchSubPage));
}

void TestAllAboutPages() {
  TestOpenChromePage(ChromePage::ABOUTDOWNLOADS,
                     GURL(chrome::kChromeUIDownloadsURL));
  TestOpenChromePage(ChromePage::ABOUTHISTORY,
                     GURL(chrome::kChromeUIHistoryURL));
  TestOpenChromePage(ChromePage::ABOUTBLANK, GURL(url::kAboutBlankURL));
}

IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTestWithSplitSettings,
                       TestOpenChromePageWithSplitFlagOn) {
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  TestAllOSSettingPages(GURL(chrome::kChromeUIOSSettingsURL));
  TestAllBrowserSettingPages(GURL(chrome::kChromeUISettingsURL));
  TestAllAboutPages();
}

// TODO(crbug/950007): This should be removed when the split is complete.
IN_PROC_BROWSER_TEST_F(ChromeNewWindowClientBrowserTestWithoutSplitSettings,
                       TestOpenChromePageWithSplitFlagOff) {
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  TestAllOSSettingPages(GURL(chrome::kChromeUISettingsURL));
  TestAllBrowserSettingPages(GURL(chrome::kChromeUISettingsURL));
  TestAllAboutPages();
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/arc/arc_open_url_delegate_impl.h"

#include <memory>
#include <string>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using arc::mojom::ChromePage;

// Return the number of windows that hosts OS Settings.
size_t GetNumberOfSettingsWindows() {
  return base::ranges::count_if(*BrowserList::GetInstance(),
                                [](Browser* browser) {
                                  return ash::IsBrowserForSystemWebApp(
                                      browser, ash::SystemWebAppType::SETTINGS);
                                });
}

// Give the underlying function a clearer name.
Browser* GetLastActiveBrowser() {
  return chrome::FindLastActive();
}

using ArcOpenUrlDelegateImplBrowserTest = InProcessBrowserTest;

using ArcOpenUrlDelegateImplWebAppBrowserTest =
    web_app::WebAppNavigationBrowserTest;

IN_PROC_BROWSER_TEST_F(ArcOpenUrlDelegateImplWebAppBrowserTest, OpenWebApp) {
  InstallTestWebApp();
  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  const char* key =
      arc::ArcWebContentsData::ArcWebContentsData::kArcTransitionFlag;

  {
    // Calling OpenWebAppFromArc for a not installed HTTPS URL should open in
    // an ordinary browser tab.
    const GURL url("https://www.google.com");
    auto observer = GetTestNavigationObserver(url);
    ArcOpenUrlDelegateImpl::GetForTesting()->OpenWebAppFromArc(url);
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
    ArcOpenUrlDelegateImpl::GetForTesting()->OpenWebAppFromArc(app_url);
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
                            bool expected_setting_window) {
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser->profile())
      ->InstallSystemAppsForTesting();

  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  ArcOpenUrlDelegateImpl::GetForTesting()->OpenChromePageFromArc(page);
  if (expected_setting_window)
    browser_opened.Wait();

  EXPECT_EQ(expected_setting_window ? 1ul : 0ul, GetNumberOfSettingsWindows());

  // The right settings are loaded (not just the settings main page).
  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_url, contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ArcOpenUrlDelegateImplBrowserTest,
                       OpenOSSettingsAppFromArc) {
  // Opening a browser setting should not open the OS setting window.
  TestOpenSettingFromArc(
      browser(), ChromePage::AUTOFILL,
      GURL("chrome://settings/").Resolve(chrome::kAutofillSubPage),
      /*expected_setting_window=*/false);

  // But opening an OS setting should open the OS setting window.
  TestOpenSettingFromArc(
      browser(), ChromePage::POWER,
      GURL("chrome://os-settings/")
          .Resolve(chromeos::settings::mojom::kPowerSubpagePath),
      /*expected_setting_window=*/true);
}

IN_PROC_BROWSER_TEST_F(ArcOpenUrlDelegateImplBrowserTest, OpenAboutChromePage) {
  // Opening an about: chrome page opens a new tab, and not the Settings window.
  ArcOpenUrlDelegateImpl::GetForTesting()->OpenChromePageFromArc(
      ChromePage::ABOUTHISTORY);
  EXPECT_EQ(0u, GetNumberOfSettingsWindows());

  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GURL(chrome::kChromeUIHistoryURL), contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ArcOpenUrlDelegateImplWebAppBrowserTest,
                       OpenAppWithIntent) {
  ASSERT_TRUE(https_server().Start());
  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());

  // InstallTestWebApp() but with a ShareTarget definition added.
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope =
      https_server().GetURL(GetAppUrlHost(), GetAppScopePath());
  web_app_info->title = base::UTF8ToUTF16(GetAppName());
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  apps::ShareTarget share_target;
  share_target.method = apps::ShareTarget::Method::kGet;
  share_target.action = app_url;
  share_target.params.text = "text";
  web_app_info->share_target = share_target;
  std::string id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

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
    ArcOpenUrlDelegateImpl::GetForTesting()->OpenAppWithIntent(
        url, std::move(intent));
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
    ArcOpenUrlDelegateImpl::GetForTesting()->OpenAppWithIntent(
        app_url, std::move(intent));
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
    ArcOpenUrlDelegateImpl::GetForTesting()->OpenAppWithIntent(
        app_url, std::move(intent));
    observer->WaitForNavigationFinished();

    EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
    EXPECT_TRUE(GetLastActiveBrowser()->is_type_app());
    content::WebContents* contents =
        GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(launch_url, contents->GetLastCommittedURL());
  }
}

void TestOpenChromePage(ChromePage page, const GURL& expected_url) {
  // Note: It is impossible currently to 'wait until done' for this method, as
  // it doesn't guarantee a new browser, web contents, or even navigation.
  ArcOpenUrlDelegateImpl::GetForTesting()->OpenChromePageFromArc(page);
  content::WebContents* contents =
      GetLastActiveBrowser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(expected_url, contents->GetVisibleURL());
}

class TestSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  void ShowChromePageForProfile(Profile* profile,
                                const GURL& gurl,
                                int64_t display_id,
                                apps::LaunchCallback callback) override {
    last_navigation_url_ = gurl;
    chrome::SettingsWindowManager::ShowChromePageForProfile(
        profile, gurl, display_id, std::move(callback));
  }
  const GURL& last_navigation_url() { return last_navigation_url_; }

 private:
  GURL last_navigation_url_;
};

void TestOpenOSSettingsChromePage(ChromePage page, const GURL& expected_url) {
  TestSettingsWindowManager test_manager;
  chrome::SettingsWindowManager::SetInstanceForTesting(&test_manager);

  // Note: It is impossible currently to 'wait until done' for this method, as
  // it doesn't guarantee a new browser, web contents, or even navigation.
  ArcOpenUrlDelegateImpl::GetForTesting()->OpenChromePageFromArc(page);

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
      ChromePage::AUDIO,
      base_url.Resolve(chromeos::settings::mojom::kAudioSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::PERDEVICEMOUSE,
      base_url.Resolve(chromeos::settings::mojom::kPerDeviceMouseSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::PERDEVICETOUCHPAD,
      base_url.Resolve(
          chromeos::settings::mojom::kPerDeviceTouchpadSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::PERDEVICEPOINTINGSTICK,
      base_url.Resolve(
          chromeos::settings::mojom::kPerDevicePointingStickSubpagePath));
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
      base_url.Resolve(chromeos::settings::mojom::kAccessibilitySectionPath));
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
  TestOpenOSSettingsChromePage(
      ChromePage::PRIVACYHUB,
      base_url.Resolve(chromeos::settings::mojom::kPrivacyHubSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::PERDEVICEKEYBOARD,
      base_url.Resolve(
          chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::GRAPHICSTABLET,
      base_url.Resolve(chromeos::settings::mojom::kGraphicsTabletSubpagePath));
  TestOpenOSSettingsChromePage(
      ChromePage::NETWORKS,
      base_url.Resolve(chromeos::settings::mojom::kNetworkSectionPath));
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

IN_PROC_BROWSER_TEST_F(ArcOpenUrlDelegateImplBrowserTest, TestOpenChromePage) {
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  TestAllOSSettingPages(GURL(chrome::kChromeUIOSSettingsURL));
  TestAllBrowserSettingPages(GURL(chrome::kChromeUISettingsURL));
  TestAllAboutPages();
  // This is required to make sure that all pending launches are flushed through
  // the system. Ideally waiters could be added for each settings page, however
  // due to some settings page 'opens' not triggering any navigations, this ends
  // up being impossible currently. So this allows any pending launches to
  // complete.
  base::RunLoop().RunUntilIdle();
}

}  // namespace

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

namespace {

// Return the number of windows that hosts OS Settings.
size_t GetNumberOfSettingsWindows() {
  auto settings_browsers =
      ui_test_utils::FindMatchingBrowsers([](BrowserWindowInterface* browser) {
        return ash::IsBrowserForSystemWebApp(browser,
                                             ash::SystemWebAppType::SETTINGS);
      });
  return settings_browsers.size();
}

}  // namespace

class SettingsWindowManagerTest : public InProcessBrowserTest {
 public:
  SettingsWindowManagerTest() = default;
  SettingsWindowManagerTest(const SettingsWindowManagerTest&) = delete;
  SettingsWindowManagerTest& operator=(const SettingsWindowManagerTest&) =
      delete;

  ~SettingsWindowManagerTest() override = default;

  void SetUpOnMainThread() override {
    settings_manager_ = chrome::SettingsWindowManager::GetInstance();

    // Install the Settings App.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();

    base::test::TestFuture<void> synchronized;
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->on_apps_synchronized()
        .Post(FROM_HERE, synchronized.GetCallback());
    ASSERT_TRUE(synchronized.Wait());
  }

  void TearDownOnMainThread() override { settings_manager_ = nullptr; }

  void CloseNonDefaultBrowsers() {
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* browser_window_interface) {
          if (browser_window_interface != browser()) {
            CloseBrowserSynchronously(browser_window_interface);
          }
          return true;
        });
  }

  void ShowOSSettings() {
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    settings_manager_->ShowOSSettings(browser()->profile());
    browser_created_observer.Wait();
  }

 protected:
  raw_ptr<chrome::SettingsWindowManager> settings_manager_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SettingsWindowManagerTest, OpenSettingsWindow) {
  // Open a settings window.
  ShowOSSettings();

  Browser* settings_browser =
      settings_manager_->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser);
  EXPECT_EQ(1u, GetNumberOfSettingsWindows());

  // Open the settings again: no new window.
  settings_manager_->ShowOSSettings(browser()->profile());
  // TODO(crbug.com/41490117): Remove this once we can wait for the
  // ShowOSSettings call correctly.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(settings_browser,
            settings_manager_->FindBrowserForProfile(browser()->profile()));
  EXPECT_EQ(1u, GetNumberOfSettingsWindows());

  // Launching via LaunchService should also de-dupe to the same browser.
  webapps::AppId settings_app_id = *ash::GetAppIdForSystemWebApp(
      browser()->profile(), ash::SystemWebAppType::SETTINGS);
  content::WebContents* contents =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
              settings_app_id, apps::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::NEW_WINDOW,
              apps::LaunchSource::kFromCommandLine));
  EXPECT_EQ(contents,
            settings_browser->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(1u, GetNumberOfSettingsWindows());

  // Close the settings window.
  CloseBrowserSynchronously(settings_browser);
  EXPECT_FALSE(settings_manager_->FindBrowserForProfile(browser()->profile()));

  // Open a new settings window.
  ShowOSSettings();
  Browser* settings_browser2 =
      settings_manager_->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser2);
  EXPECT_EQ(1u, GetNumberOfSettingsWindows());

  CloseBrowserSynchronously(settings_browser2);
}

IN_PROC_BROWSER_TEST_F(SettingsWindowManagerTest, OpenChromePages) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // History should open in the existing browser window.
  chrome::ShowHistory(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Settings should open a new browser window.
  ShowOSSettings();
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // About should reuse the existing Settings window.
  chrome::ShowAboutChrome(browser());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // Extensions should open in an existing browser window.
  CloseNonDefaultBrowsers();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  std::string extension_to_highlight;  // none
  chrome::ShowExtensions(browser(), extension_to_highlight);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Downloads should open in an existing browser window.
  chrome::ShowDownloads(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(SettingsWindowManagerTest, OpenAboutPage) {
  // About should open settings window.
  chrome::ShowAboutChrome(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_F(SettingsWindowManagerTest, OpenSettings) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Browser settings opens in the existing browser window.
  chrome::ShowSettings(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // OS settings opens in a new window.
  ShowOSSettings();
  EXPECT_EQ(1u, GetNumberOfSettingsWindows());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // The opened Settings window should be the active browser.
  content::WebContents* web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(chrome::kChromeUIOSSettingsHost, web_contents->GetURL().GetHost());

  // Showing an OS sub-page reuses the OS settings window.
  settings_manager_->ShowOSSettings(
      browser()->profile(),
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath);
  EXPECT_EQ(1u, GetNumberOfSettingsWindows());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // Close the settings window.
  CloseNonDefaultBrowsers();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Showing a browser setting sub-page reuses the browser window.
  chrome::ShowSettingsSubPage(browser(), chrome::kAutofillSubPage);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

class SettingsWindowManagerLoginTest : public MixinBasedInProcessBrowserTest {
 public:
  SettingsWindowManagerLoginTest() = default;
  SettingsWindowManagerLoginTest(const SettingsWindowManagerLoginTest&) =
      delete;
  SettingsWindowManagerLoginTest& operator=(
      const SettingsWindowManagerLoginTest&) = delete;
  ~SettingsWindowManagerLoginTest() override = default;

 private:
  ash::LoginManagerMixin login_manager_{&mixin_host_, {}};
};

// Regression test for crash. https://crbug.com/1174525
IN_PROC_BROWSER_TEST_F(SettingsWindowManagerLoginTest, OpenBeforeLogin) {
  // Precondition: We're not signed in.
  ASSERT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // Try to open OS settings.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      ash::ProfileHelper::GetSigninProfile());

  // We didn't crash, and nothing opened.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0u, GetNumberOfSettingsWindows());
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"

#include <variant>

#include "ash/birch/birch_model.h"
#include "ash/birch/coral_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/test/run_until.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/birch/birch_test_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "gmock/gmock.h"

namespace ash {

class BirchCoralProviderTest : public extensions::PlatformAppBrowserTest {
 public:
  BirchCoralProviderTest() = default;
  BirchCoralProviderTest(const BirchCoralProviderTest&) = delete;
  BirchCoralProviderTest& operator=(const BirchCoralProviderTest&) = delete;
  ~BirchCoralProviderTest() override = default;

  // extensions::PlatformAppBrowserTest:
  void SetUpOnMainThread() override {
    // Enable coral service.
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
        prefs::kBirchUseCoral, true);

    ash::SystemWebAppManager::GetForTest(profile())
        ->InstallSystemAppsForTesting();

    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
  }

 protected:
  BirchCoralProvider* GetCoralProvider() const {
    return static_cast<BirchCoralProvider*>(
        Shell::Get()->birch_model()->GetCoralProviderForTest());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kCoralFeature};
};

MATCHER_P2(TabEq, title, url, "") {
  return arg.title == title && arg.url == url;
}

MATCHER_P2(AppEq, title, id, "") {
  return arg.title == title && arg.id == id;
}

// Tests that the coral provider collects correct in-session tab and app data.
IN_PROC_BROWSER_TEST_F(BirchCoralProviderTest, CollectInSessionData) {
  // Close existing browser windows.
  CloseAllBrowsers();

  // Create two browsers with different tabs and urls.
  test::CreateAndShowBrowser(profile(), {GURL("https://examples1.com"),
                                         GURL("https://examples2.com")});
  test::CreateAndShowBrowser(profile(), {GURL("https://examples3.com")});

  // Open some SWA windows.
  test::CreateSystemWebApp(profile(), SystemWebAppType::FILE_MANAGER);
  test::CreateSystemWebApp(profile(), SystemWebAppType::SETTINGS);
  test::CreateSystemWebApp(profile(), SystemWebAppType::HELP);

  // Open some PWA windows.
  test::InstallAndLaunchPWA(profile(), GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");
  test::InstallAndLaunchPWA(profile(), GURL("https://www.gmail.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"Gmail");

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  // Check if the collected data as expected.
  const coral_util::TabsAndApps tabs_and_apps = coral_util::SplitContentData(
      GetCoralProvider()->GetCoralRequestForTest().content());

  // Comparing the collected tab data with the expected tab data.
  EXPECT_THAT(tabs_and_apps.tabs,
              testing::UnorderedElementsAre(
                  TabEq("examples1.com", GURL("https://examples1.com/")),
                  TabEq("examples2.com", GURL("https://examples2.com/")),
                  TabEq("examples3.com", GURL("https://examples3.com/"))));

  // Comparing the collected app data with the expected app data in mru order.
  EXPECT_THAT(tabs_and_apps.apps,
              testing::UnorderedElementsAre(
                  AppEq("Gmail", "gdkbjbkdgeggmfkjbfohmimchmkikbid"),
                  AppEq("YouTube", "adnlfjpnmidfimlkaohpidplnoimahfh"),
                  AppEq("Explore", "nbljnnecbjbmifnoehiemkgefbnpoeak"),
                  AppEq("Settings", "odknhmnlageboeamepcngndbggdpaobj"),
                  AppEq("Files", "fkiggjmkendpmbegkagpmagjepfkpmeb")));
}

// Tests that the coral provider filters out duplicated tab and app data.
IN_PROC_BROWSER_TEST_F(BirchCoralProviderTest, NoDupInSessionData) {
  // Close existing browser windows.
  CloseAllBrowsers();

  // Create two browsers with duplicated urls.
  test::CreateAndShowBrowser(
      profile(), {GURL("https://examples1.com"), GURL("https://examples2.com"),
                  GURL("https://examples2.com")});
  test::CreateAndShowBrowser(profile(), {GURL("https://examples1.com"),
                                         GURL("https://examples3.com")});

  // Open some SWA windows with duplicated apps.
  test::CreateSystemWebApp(profile(), SystemWebAppType::FILE_MANAGER);
  test::CreateSystemWebApp(profile(), SystemWebAppType::FILE_MANAGER);
  test::CreateSystemWebApp(profile(), SystemWebAppType::SETTINGS);

  // Open some PWA windows with duplicated apps.
  test::InstallAndLaunchPWA(profile(), GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");
  test::InstallAndLaunchPWA(profile(), GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"Youtube");

  ash::ToggleOverview();
  ash::WaitForOverviewEnterAnimation();

  // Check if the collected data as expected.
  const coral_util::TabsAndApps tabs_and_apps = coral_util::SplitContentData(
      GetCoralProvider()->GetCoralRequestForTest().content());

  // Comparing the collected tab data with the expected tab data.
  EXPECT_THAT(tabs_and_apps.tabs,
              testing::UnorderedElementsAre(
                  TabEq("examples1.com", GURL("https://examples1.com/")),
                  TabEq("examples2.com", GURL("https://examples2.com/")),
                  TabEq("examples3.com", GURL("https://examples3.com/"))));

  // Comparing the collected app data with the expected app data in mru order.
  EXPECT_THAT(tabs_and_apps.apps,
              testing::UnorderedElementsAre(
                  AppEq("YouTube", "adnlfjpnmidfimlkaohpidplnoimahfh"),
                  AppEq("Settings", "odknhmnlageboeamepcngndbggdpaobj"),
                  AppEq("Files", "fkiggjmkendpmbegkagpmagjepfkpmeb")));
}

// Tests that the coral provider collects correct post-login tab and app data.
IN_PROC_BROWSER_TEST_F(BirchCoralProviderTest, PRE_CollectPostLoginData) {
  // Close existing browser windows.
  CloseAllBrowsers();

  // Create two browsers with different tabs and urls. Use a couple chrome urls
  // as those are guaranteed to have tab titles. Setting titles to regular urls
  // is possible, but does not get saved by session restore.
  test::CreateAndShowBrowser(profile(), {GURL("https://examples1.com"),
                                         GURL("https://examples2.com")});
  test::CreateAndShowBrowser(
      profile(),
      {GURL(chrome::kChromeUIBookmarksURL),
       GURL(chrome::kChromeUIChromeURLsURL), GURL(chrome::kChromeUICrashesUrl),
       GURL(chrome::kChromeUIDownloadsURL), GURL(chrome::kChromeUIHistoryURL),
       GURL(chrome::kChromeUISettingsURL)});

  // Open a SWA and a PWA.
  test::CreateSystemWebApp(profile(), SystemWebAppType::SETTINGS);
  test::InstallAndLaunchPWA(profile(), GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");

  // Immediate save to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

IN_PROC_BROWSER_TEST_F(BirchCoralProviderTest, CollectPostLoginData) {
  // Check if the collected data as expected.
  const coral_util::TabsAndApps tabs_and_apps = coral_util::SplitContentData(
      GetCoralProvider()->GetCoralRequestForTest().content());

  // Comparing the collected tab data with the expected tab data.
  EXPECT_THAT(tabs_and_apps.tabs,
              testing::UnorderedElementsAre(
                  TabEq("", GURL("https://examples1.com/")),
                  TabEq("", GURL("https://examples2.com/")),
                  TabEq("bookmarks", GURL(chrome::kChromeUIBookmarksURL)),
                  TabEq("chrome-urls", GURL(chrome::kChromeUIChromeURLsURL)),
                  TabEq("crashes", GURL(chrome::kChromeUICrashesUrl)),
                  TabEq("downloads", GURL(chrome::kChromeUIDownloadsURL)),
                  TabEq("history", GURL(chrome::kChromeUIHistoryURL)),
                  TabEq("settings", GURL(chrome::kChromeUISettingsURL))));

  // Comparing the collected app data with the expected app data in mru order.
  EXPECT_THAT(tabs_and_apps.apps,
              testing::UnorderedElementsAre(
                  AppEq("YouTube", "adnlfjpnmidfimlkaohpidplnoimahfh"),
                  AppEq("Settings", "odknhmnlageboeamepcngndbggdpaobj")));
}

}  // namespace ash

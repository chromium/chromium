// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/coral/coral_controller.h"
#include "ash/wm/coral/coral_test_util.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/birch/birch_test_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "components/app_restore/restore_data.h"
#include "content/public/test/browser_test.h"
#include "gmock/gmock.h"

namespace ash {
namespace {

// Collects the tab URLs from the given window list.
std::vector<GURL> CollectTabURLsFromWindows(
    const MruWindowTracker::WindowList& windows) {
  std::vector<GURL> tab_urls;
  for (aura::Window* window : windows) {
    Browser* browser =
        BrowserView::GetBrowserViewForNativeWindow(window)->browser();

    if (!browser) {
      continue;
    }

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip_model->count(); idx++) {
      tab_urls.push_back(
          tab_strip_model->GetWebContentsAt(idx)->GetVisibleURL());
    }
  }
  return tab_urls;
}

// Collects the app IDs from the given window list.
std::vector<std::string> CollectAppIDsFromWindows(
    const MruWindowTracker::WindowList& windows) {
  std::vector<std::string> app_ids;
  for (aura::Window* window : windows) {
    if (auto* app_id = window->GetProperty(kAppIDKey)) {
      app_ids.emplace_back(*app_id);
    }
  }
  return app_ids;
}

// Returns the native window associated with `swa_type`, if it exists.
aura::Window* GetNativeWindowForSwa(SystemWebAppType swa_type) {
  BrowserList* browsers = BrowserList::GetInstance();
  auto it = base::ranges::find_if(*browsers, [swa_type](Browser* browser) {
    return IsBrowserForSystemWebApp(browser, swa_type);
  });
  return it == browsers->end() ? nullptr : (*it)->window()->GetNativeWindow();
}

}  // namespace

class CoralBrowserTest : public InProcessBrowserTest {
 public:
  CoralBrowserTest() { set_launch_browser_for_testing(nullptr); }
  CoralBrowserTest(const CoralBrowserTest&) = delete;
  CoralBrowserTest& operator=(const CoralBrowserTest&) = delete;
  ~CoralBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Disable the prefs for data providers other than coral. This ensures
    // the data is fresh once the last active provider replies.
    DisableAllDataTypePrefsExcept({prefs::kBirchUseCoral});

    // Ensure the item remover is initialized, otherwise data fetches won't
    // complete.
    EnsureItemRemoverInitialized();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceBirchFakeCoralGroup);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kCoralFeature};
};

IN_PROC_BROWSER_TEST_F(CoralBrowserTest, PRE_PostLoginLaunch) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  test::InstallSystemAppsForTesting(profile);

  // Launch some SWA's. We will confirm their window bounds and state in the
  // real test. This will also ensure that post login overview shows up by
  // having at least one app open.
  test::CreateSystemWebApp(profile, SystemWebAppType::FILE_MANAGER);
  test::CreateSystemWebApp(profile, SystemWebAppType::SETTINGS);

  aura::Window* files_window =
      GetNativeWindowForSwa(SystemWebAppType::FILE_MANAGER);
  ASSERT_TRUE(files_window);
  files_window->SetBounds(gfx::Rect(600, 600));
  aura::Window* settings_window =
      GetNativeWindowForSwa(SystemWebAppType::SETTINGS);
  ASSERT_TRUE(settings_window);
  WindowState::Get(settings_window)->Maximize();

  test::InstallAndLaunchPWA(profile, GURL("https://www.nba.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"NBA");

  // Immediate save to full restore file to bypass the 2.5 second throttle.
  AppLaunchInfoSaveWaiter::Wait();
}

// Launches a browser with the expected tabs when the post login coral chip is
// clicked.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, PostLoginLaunch) {
  ASSERT_TRUE(BrowserList::GetInstance()->empty());

  Profile* profile = ProfileManager::GetActiveUserProfile();

  test::InstallSystemAppsForTesting(profile);

  // Wait until the chip is visible, it may not be visible while data fetch is
  // underway or the overview animation is still running.
  EXPECT_TRUE(base::test::RunUntil([]() {
    BirchChipButtonBase* coral_chip = GetBirchChipButton();
    return !!coral_chip;
  }));

  BirchChipButtonBase* coral_chip = GetBirchChipButton();
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);

  test::BrowsersWaiter waiter(/*expected_count=*/4);
  test::Click(coral_chip);
  waiter.Wait();

  // TODO(sammiequon): These tabs and apps are currently hardcoded in ash for
  // `switches::kForceBirchFakeCoral`. Update to use a test coral provider
  // instead.
  BrowserList* browsers = BrowserList::GetInstance();
  ASSERT_EQ(browsers->size(), 4u);
  // Verify the chrome browser.
  EXPECT_TRUE(
      base::ranges::any_of(*browsers, [](Browser* browser) {
        TabStripModel* tab_strip_model = browser->tab_strip_model();
        return tab_strip_model->count() == 3 &&
               tab_strip_model->GetWebContentsAt(0)->GetVisibleURL() ==
                   GURL("https://www.reddit.com/") &&
               tab_strip_model->GetWebContentsAt(1)->GetVisibleURL() ==
                   GURL("https://www.figma.com/") &&
               tab_strip_model->GetWebContentsAt(2)->GetVisibleURL() ==
                   GURL("https://www.notion.so/");
      }));

  // Verify the PWA.
  EXPECT_TRUE(base::ranges::any_of(*browsers, [](Browser* browser) {
    if (browser->type() != Browser::TYPE_APP) {
      return false;
    }
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    return tab_strip_model->count() == 1 &&
           tab_strip_model->GetWebContentsAt(0)->GetVisibleURL() ==
               GURL("https://www.nba.com/");
  }));

  // Tests that the files and settings SWAs are launched and have their previous
  // session window bounds.
  aura::Window* files_window =
      GetNativeWindowForSwa(SystemWebAppType::FILE_MANAGER);
  ASSERT_TRUE(files_window);
  EXPECT_EQ(gfx::Rect(600, 600), files_window->GetBoundsInScreen());

  aura::Window* settings_window =
      GetNativeWindowForSwa(SystemWebAppType::SETTINGS);
  ASSERT_TRUE(settings_window);
  EXPECT_TRUE(WindowState::Get(settings_window)->IsMaximized());
}

// Tests that clicking the in session coral button opens and activates a new
// desk.
// TODO(zxdan): Temporarily disable the test until the item uses the real group
// data.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, DISABLED_OpenNewDesk) {
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(1u, desks_controller->desks().size());

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Create a test coral group with no tabs and apps.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(CreateTestGroup({}, "Coral desk"));
  OverrideTestResponse(std::move(test_groups));

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButtonBase* coral_chip = GetBirchChipButton();
  ASSERT_TRUE(coral_chip);
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);

  DeskSwitchAnimationWaiter waiter;
  test::Click(coral_chip);
  waiter.Wait();

  // After clicking the coral chip, we have two desks and the new active desk
  // has the coral title.
  EXPECT_EQ(2u, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  EXPECT_EQ(u"Coral desk", desks_controller->GetDeskName(
                               desks_controller->GetActiveDeskIndex()));
}

// Tests that the Coral Delegate could create a new browser on the new desk by
// moving indicated tabs from the browser on the active desk.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, MoveTabsToNewDesk) {
  // Create two browsers with different tabs and urls.
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  test::CreateAndShowBrowser(primary_profile, {GURL("https://youtube.com"),
                                               GURL("https://google.com")});
  test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://maps.google.com"), GURL("https://mail.google.com")});

  // Cache the windows on current desk.
  const auto windows_on_last_active_desk =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  // Create a fake coral group which contains two tabs that are selected from
  // each of the two browsers created above.
  coral::mojom::GroupPtr fake_group =
      CreateTestGroup({{"Youtube", GURL("https://youtube.com")},
                       {"Google Maps", GURL("https://maps.google.com")}},
                      "Coral desk");

  DeskSwitchAnimationWaiter waiter;
  Shell::Get()->coral_controller()->OpenNewDeskWithGroup(std::move(fake_group));
  waiter.Wait();

  // We should have two desks and the new active desk has the coral title.
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(2u, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  EXPECT_EQ(u"Coral desk", desks_controller->GetDeskName(
                               desks_controller->GetActiveDeskIndex()));

  // The active desk should have a browser window which has the two tabs in the
  // fake group.
  std::vector<GURL> tab_urls_on_active_desk = CollectTabURLsFromWindows(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_THAT(tab_urls_on_active_desk,
              testing::UnorderedElementsAre(GURL("https://youtube.com"),
                                            GURL("https://maps.google.com")));

  // The last active desk should not have the moved tabs.
  std::vector<GURL> tab_urls_on_last_active_desk =
      CollectTabURLsFromWindows(windows_on_last_active_desk);
  EXPECT_THAT(tab_urls_on_last_active_desk,
              testing::UnorderedElementsAre(GURL("https://google.com"),
                                            GURL("https://mail.google.com")));
}

// Tests that the Coral controller could create a new desk and move the apps in
// the group to the new desk.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, MoveAppsToNewDesk) {
  // Create two browsers with different tabs and urls.
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  test::InstallSystemAppsForTesting(primary_profile);

  // Open some SWA windows.
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::SETTINGS);
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::HELP);

  // Create a browser window.
  CreateBrowser(primary_profile);

  // Open some PWA windows.
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.gmail.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"Gmail");

  // Create a fake coral group which contains four apps except the Files app and
  // the browser.
  coral::mojom::GroupPtr fake_group =
      CreateTestGroup({{"Youtube", "adnlfjpnmidfimlkaohpidplnoimahfh"},
                       {"Gmail", "gdkbjbkdgeggmfkjbfohmimchmkikbid"},
                       {"Explore", "nbljnnecbjbmifnoehiemkgefbnpoeak"},
                       {"Settings", "odknhmnlageboeamepcngndbggdpaobj"}},
                      "Coral desk");

  DeskSwitchAnimationWaiter waiter;
  Shell::Get()->coral_controller()->OpenNewDeskWithGroup(std::move(fake_group));
  waiter.Wait();

  // We should have two desks and the new active desk has the coral title.
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(2u, desks_controller->desks().size());
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
  EXPECT_EQ(u"Coral desk", desks_controller->GetDeskName(
                               desks_controller->GetActiveDeskIndex()));

  // The active desk should have the four apps in the group.
  std::vector<std::string> app_ids_on_active_desk = CollectAppIDsFromWindows(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_THAT(app_ids_on_active_desk, testing::UnorderedElementsAre(
                                          "adnlfjpnmidfimlkaohpidplnoimahfh",
                                          "gdkbjbkdgeggmfkjbfohmimchmkikbid",
                                          "nbljnnecbjbmifnoehiemkgefbnpoeak",
                                          "odknhmnlageboeamepcngndbggdpaobj"));
}

// Tests that the chip will get updated when the title is loaded by the backend.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, AsyncGroupTitle) {
  // Create a test coral group with a pending title.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(CreateTestGroup({}));
  OverrideTestResponse(std::move(test_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButton* coral_chip =
      static_cast<BirchChipButton*>(GetBirchChipButton());
  ASSERT_TRUE(coral_chip);

  // The chip should hide title with title pending.
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);
  ASSERT_FALSE(coral_chip->title_->GetVisible());

  // When the group title gets updated, the chip title will be shown with
  // updated title.
  BirchCoralProvider::Get()->TitleUpdated(base::Token(), "Updated Title");
  ASSERT_TRUE(coral_chip->title_->GetVisible());
  EXPECT_EQ(coral_chip->title_->GetText(), u"Updated Title");
}

// Tests that the chip will show placeholder title when corresponding group
// title loading fails.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, GroupTitleLoadingFail) {
  // Create a test coral group with a pending title.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(CreateTestGroup({}));
  OverrideTestResponse(std::move(test_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a single chip.
  BirchChipButton* coral_chip =
      static_cast<BirchChipButton*>(GetBirchChipButton());
  ASSERT_TRUE(coral_chip);

  // The chip should show placeholder title when receiving an empty title.
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);
  ASSERT_FALSE(coral_chip->title_->GetVisible());

  // When the group title gets updated, the chip title will be shown with
  // updated title.
  BirchCoralProvider::Get()->TitleUpdated(base::Token(), "");
  ASSERT_TRUE(coral_chip->title_->GetVisible());
  EXPECT_EQ(coral_chip->title_->GetText(), u"Suggested Group");
}

// Tests that the coral chip gets updated while corresponding tab/app items are
// closed.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, CloseTabAppUpdateChip) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  // Create two browsers. A url appears in both browsers.
  test::CreateAndShowBrowser(primary_profile, {GURL("https://youtube.com")});
  test::CreateAndShowBrowser(primary_profile, {GURL("https://youtube.com"),
                                               GURL("https://google.com")});

  test::InstallSystemAppsForTesting(primary_profile);

  // Open two File windows and two PWA windows.
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.gmail.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"Gmail");

  // Create a fake coral group which contains non-duplicated tabs and apps.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"google.com", "https://google.com/"},
                       {"youtube.com", "https://youtube.com/"},
                       {"YouTube", "adnlfjpnmidfimlkaohpidplnoimahfh"},
                       {"Files", "fkiggjmkendpmbegkagpmagjepfkpmeb"}},
                      "Coral desk"));

  OverrideTestResponse(std::move(test_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with a coral chip.
  ASSERT_TRUE(GetBirchChipButton());

  const auto& group = BirchCoralProvider::Get()->GetGroupById(base::Token());

  EXPECT_EQ(4u, group->entities.size());

  // Closing the first browser with the duplicated tab (https://youtube.com)
  // will not change the group.
  SelectFirstBrowser();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(4u, group->entities.size());

  // Closing the next browser will decrease the items in the group.
  SelectFirstBrowser();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(2u, group->entities.size());

  // Closing a duplicated window (file manager) will not change the group.
  SelectFirstBrowser();
  EXPECT_EQ(u"Files", browser()->window()->GetNativeWindow()->GetTitle());
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(2u, group->entities.size());

  // Closing a non-duplicated window will decrease the items in the group.
  SelectFirstBrowser();
  EXPECT_EQ(u"Files", browser()->window()->GetNativeWindow()->GetTitle());
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, group->entities.size());

  // Closing the last app window in group will remove the chip.
  SelectFirstBrowser();
  EXPECT_EQ(u"YouTube", browser()->window()->GetNativeWindow()->GetTitle());
  CloseBrowserSynchronously(browser());

  EXPECT_FALSE(GetBirchChipButton());
}

}  // namespace ash

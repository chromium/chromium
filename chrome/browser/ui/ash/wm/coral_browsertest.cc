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
#include "ash/wm/coral/coral_test_util.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/birch/coral_chip_button.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/command_line.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/birch/birch_test_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "components/app_restore/restore_data.h"
#include "content/public/test/browser_test.h"
#include "gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

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
  aura::Window* found_window = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [swa_type, &found_window](BrowserWindowInterface* browser) {
        if (IsBrowserForSystemWebApp(browser, swa_type)) {
          found_window = browser->GetWindow()->GetNativeWindow();
        }
        return !found_window;
      });
  return found_window;
}

class WindowDestroyedObserver : public aura::WindowObserver {
 public:
  explicit WindowDestroyedObserver(aura::Window* window) {
    CHECK(window);
    window_observation_.Observe(window);
  }

  void Wait() {
    if (window_observation_.IsObserving()) {
      run_loop_.Run();
    }
  }

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    window_observation_.Reset();
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

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

  void CloseBrowserAndNativeWindowSynchronously(
      BrowserWindowInterface* browser) {
    WindowDestroyedObserver window_destroyed_observer(
        browser->GetWindow()->GetNativeWindow());
    CloseBrowserSynchronously(browser);
    window_destroyed_observer.Wait();
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
  ASSERT_EQ(chrome::GetTotalBrowserCount(), 0u);

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

  // TODO(zxdan): These tabs and apps are currently hardcoded in ash for
  // `switches::kForceBirchFakeCoral`. Update to use a test coral provider
  // instead.
  ASSERT_EQ(chrome::GetTotalBrowserCount(), 4u);
  const std::vector<BrowserWindowInterface*> browsers =
      ui_test_utils::FindMatchingBrowsers(
          [](BrowserWindowInterface* browser) { return true; });
  // Verify the chrome browser.
  EXPECT_TRUE(std::ranges::any_of(browsers, [](BrowserWindowInterface*
                                                   browser) {
    TabStripModel* const tab_strip_model = browser->GetTabStripModel();
    return tab_strip_model->count() == 3 &&
           tab_strip_model->GetWebContentsAt(0)->GetVisibleURL() ==
               GURL("https://www.reddit.com/") &&
           tab_strip_model->GetWebContentsAt(1)->GetVisibleURL() ==
               GURL("https://www.figma.com/") &&
           tab_strip_model->GetWebContentsAt(2)->GetVisibleURL() ==
               GURL("https://www.notion.so/");
  }));

  // Verify the PWA.
  EXPECT_TRUE(
      std::ranges::any_of(browsers, [](BrowserWindowInterface* browser) {
        if (browser->GetType() != BrowserWindowInterface::TYPE_APP) {
          return false;
        }
        TabStripModel* const tab_strip_model = browser->GetTabStripModel();
        return tab_strip_model->count() == 1 &&
               tab_strip_model->GetWebContentsAt(0)->GetVisibleURL() ==
                   GURL("https://www.nba.com/");
      }));

  // Tests that the files and settings SWAs are launched and have their previous
  // session window bounds.
  aura::Window* files_window =
      GetNativeWindowForSwa(SystemWebAppType::FILE_MANAGER);
  ASSERT_TRUE(files_window);
  EXPECT_EQ(files_window->GetBoundsInScreen(), gfx::Rect(600, 600));

  aura::Window* settings_window =
      GetNativeWindowForSwa(SystemWebAppType::SETTINGS);
  ASSERT_TRUE(settings_window);
  EXPECT_TRUE(WindowState::Get(settings_window)->IsMaximized());
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
  std::vector<coral::mojom::GroupPtr> fake_groups;
  fake_groups.push_back(
      CreateTestGroup({{"Youtube", GURL("https://youtube.com")},
                       {"Google Maps", GURL("https://maps.google.com")}},
                      "Coral desk"));
  OverrideTestResponse(std::move(fake_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  BirchChipButton* coral_chip = GetFirstCoralButton();
  ASSERT_TRUE(coral_chip);
  test::Click(coral_chip);

  // We should have two desks and the new active desk has the coral title.
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), 2u);
  EXPECT_EQ(desks_controller->GetActiveDeskIndex(), 1);
  EXPECT_EQ(
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex()),
      u"Coral desk");

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
  std::vector<coral::mojom::GroupPtr> fake_groups;
  fake_groups.push_back(
      CreateTestGroup({{"Youtube", "adnlfjpnmidfimlkaohpidplnoimahfh"},
                       {"Gmail", "gdkbjbkdgeggmfkjbfohmimchmkikbid"},
                       {"Explore", "nbljnnecbjbmifnoehiemkgefbnpoeak"},
                       {"Settings", "odknhmnlageboeamepcngndbggdpaobj"}},
                      "Coral desk"));
  OverrideTestResponse(std::move(fake_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  BirchChipButton* coral_chip = GetFirstCoralButton();
  ASSERT_TRUE(coral_chip);
  test::Click(coral_chip);

  // We should have two desks and the new active desk has the coral title.
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), 2u);
  EXPECT_EQ(desks_controller->GetActiveDeskIndex(), 1);
  EXPECT_EQ(
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex()),
      u"Coral desk");

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
  test_groups.push_back(
      CreateTestGroup({{"example", GURL("www.example.com")}}));
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
  CoralChipButton* coral_chip =
      views::AsViewClass<CoralChipButton>(GetBirchChipButton());
  ASSERT_TRUE(coral_chip);

  // The chip should hide title with title pending.
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);
  ASSERT_FALSE(coral_chip->title()->GetVisible());

  // When the group title gets updated, the chip title will be shown with
  // updated title.
  BirchCoralProvider::Get()->TitleUpdated(base::Token(), "Updated Title");
  ASSERT_TRUE(coral_chip->title()->GetVisible());
  EXPECT_EQ(coral_chip->title()->GetText(), u"Updated Title");
  EXPECT_EQ(coral_chip->GetAccessibleName(),
            u"Updated Title Organize in a new desk");
}

// Tests that the chip will show placeholder title when corresponding group
// title loading fails.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, GroupTitleLoadingFail) {
  // Create a test coral group with a pending title.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"example", GURL("www.example.com")}}));
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
  CoralChipButton* coral_chip =
      views::AsViewClass<CoralChipButton>(GetBirchChipButton());
  ASSERT_TRUE(coral_chip);

  // The chip should show placeholder title when receiving an empty title.
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);
  ASSERT_FALSE(coral_chip->title()->GetVisible());

  // When the group title gets updated, the chip title will be shown with
  // updated title.
  BirchCoralProvider::Get()->TitleUpdated(base::Token(), "");
  ASSERT_TRUE(coral_chip->title()->GetVisible());
  EXPECT_EQ(coral_chip->title()->GetText(), u"Suggested group");
  EXPECT_EQ(coral_chip->GetAccessibleName(),
            u"Suggested group Organize in a new desk");
}

// Tests that the coral chip gets updated while corresponding tab/app items are
// closed.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, CloseTabAppUpdateChip) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  // Create two browsers. A url appears in both browsers.
  BrowserWindowInterface* const normal_browser1 = test::CreateAndShowBrowser(
      primary_profile, {GURL("https://youtube.com")});
  BrowserWindowInterface* const normal_browser2 = test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://youtube.com"), GURL("https://google.com")});

  test::InstallSystemAppsForTesting(primary_profile);

  // Open two File windows.
  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  BrowserWindowInterface* const file_browser1 =
      browser_created_observer->Wait();
  browser_created_observer.emplace();
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  BrowserWindowInterface* const file_browser2 =
      browser_created_observer->Wait();

  // Open two PWA windows.
  BrowserWindowInterface* const pwa_browser1 = test::InstallAndLaunchPWA(
      primary_profile, GURL("https://www.youtube.com/"),
      /*launch_in_browser=*/false,
      /*app_title=*/u"YouTube");
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.gmail.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"Gmail");

  // Create a fake coral group which contains non-duplicated tabs and apps.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"google.com", GURL("https://google.com/")},
                       {"youtube.com", GURL("https://youtube.com/")},
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

  EXPECT_EQ(group->entities.size(), 4u);

  // Closing the first browser with the duplicated tab (https://youtube.com)
  // will not change the group.
  CloseBrowserAndNativeWindowSynchronously(normal_browser1);
  EXPECT_EQ(group->entities.size(), 4u);

  // Closing the next browser will decrease the items in the group.
  CloseBrowserAndNativeWindowSynchronously(normal_browser2);
  EXPECT_EQ(group->entities.size(), 2u);

  // Closing a duplicated window (file manager) will not change the group.
  CloseBrowserAndNativeWindowSynchronously(file_browser1);
  EXPECT_EQ(group->entities.size(), 2u);

  // Closing a non-duplicated window will decrease the items in the group.
  CloseBrowserAndNativeWindowSynchronously(file_browser2);
  EXPECT_EQ(group->entities.size(), 1u);

  // Closing the last app window in group will remove the chip.
  CloseBrowserAndNativeWindowSynchronously(pwa_browser1);

  EXPECT_FALSE(GetBirchChipButton());
}

// Tests that closing a window which contains all the items in two groups would
// remove corresponding coral chips.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, CloseWindowRemoveTwoChips) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  // Create a browser containing 8 tabs.
  BrowserWindowInterface* const browser = test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://mail.google.com"), GURL("https://youtube.com"),
       GURL("https://google.com"), GURL("https://earth.google.com"),
       GURL("https://maps.google.com"), GURL("https://docs.google.com"),
       GURL("https://calendar.google.com"), GURL("https://chat.google.com")});
  // Create another browser to keep staying in Overview after removing the first
  // one.
  test::CreateAndShowBrowser(primary_profile,
                             {GURL("https://meet.google.com")});

  // Create a fake coral group which contains non-duplicated tabs and apps.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"mail.google.com", GURL("https://mail.google.com")},
                       {"youtube.com", GURL("https://youtube.com")},
                       {"google.com", GURL("https://google.com")},
                       {"earth.google.com", GURL("https://earth.google.com")}},
                      "Coral desk 1", /*id=*/base::Token(1, 2)));
  test_groups.push_back(CreateTestGroup(
      {{"maps.google.com", GURL("https://maps.google.com")},
       {"docs.google.com", GURL("https://docs.google.com")},
       {"calendar.google.com", GURL("https://calendar.google.com")},
       {"chat.google.com", GURL("https://chat.google.com")}},
      "Coral desk 2", /*id=*/base::Token(2, 3)));

  OverrideTestResponse(std::move(test_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with two coral chips.
  ASSERT_EQ(GetBirchChipsNum(), 2u);

  // Closing the first browser with all items in groups.
  EXPECT_EQ(8, browser->GetTabStripModel()->GetTabCount());
  CloseBrowserAndNativeWindowSynchronously(browser);

  // Two chips are removed.
  EXPECT_EQ(0u, GetBirchChipsNum());
}

// Tests that closing a desk removes all coral chips.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, CloseDeskRemoveAllChips) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  // Create a browser containing 8 tabs.
  test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://mail.google.com"), GURL("https://youtube.com"),
       GURL("https://google.com"), GURL("https://earth.google.com"),
       GURL("https://maps.google.com"), GURL("https://docs.google.com"),
       GURL("https://calendar.google.com"), GURL("https://chat.google.com")});

  test::InstallSystemAppsForTesting(primary_profile);

  // Open a File window and a PWA window.
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");

  // Create two fake coral groups.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"mail.google.com", GURL("https://mail.google.com")},
                       {"youtube.com", GURL("https://youtube.com")},
                       {"google.com", GURL("https://google.com")},
                       {"YouTube", "adnlfjpnmidfimlkaohpidplnoimahfh"}},
                      "Coral desk 1", /*id=*/base::Token(1, 2)));
  test_groups.push_back(CreateTestGroup(
      {{"maps.google.com", GURL("https://maps.google.com")},
       {"docs.google.com", GURL("https://docs.google.com")},
       {"calendar.google.com", GURL("https://calendar.google.com")},
       {"Files", "fkiggjmkendpmbegkagpmagjepfkpmeb"}},
      "Coral desk 2", /*id=*/base::Token(2, 3)));

  OverrideTestResponse(std::move(test_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  NewDesk();

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with two coral chips.
  ASSERT_EQ(GetBirchChipsNum(), 2u);

  // Closing the active desk removes all chips.
  RemoveDesk(GetActiveDesk(), DeskCloseType::kCloseAllWindows);
  SimulateWaitForCloseAll();

  // Two chips are removed.
  EXPECT_EQ(0u, GetBirchChipsNum());
}

// Tests that merging a desk removes all coral chips.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, MergeDeskRemoveAllChips) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  // Create a browser containing 8 tabs.
  test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://mail.google.com"), GURL("https://youtube.com"),
       GURL("https://google.com"), GURL("https://earth.google.com"),
       GURL("https://maps.google.com"), GURL("https://docs.google.com"),
       GURL("https://calendar.google.com"), GURL("https://chat.google.com")});

  test::InstallSystemAppsForTesting(primary_profile);

  // Open a File window and a PWA window.
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");

  // Create two fake coral groups.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"mail.google.com", GURL("https://mail.google.com")},
                       {"youtube.com", GURL("https://youtube.com")},
                       {"google.com", GURL("https://google.com")},
                       {"YouTube", "adnlfjpnmidfimlkaohpidplnoimahfh"}},
                      "Coral desk 1", /*id=*/base::Token(1, 2)));
  test_groups.push_back(CreateTestGroup(
      {{"maps.google.com", GURL("https://maps.google.com")},
       {"docs.google.com", GURL("https://docs.google.com")},
       {"calendar.google.com", GURL("https://calendar.google.com")},
       {"Files", "fkiggjmkendpmbegkagpmagjepfkpmeb"}},
      "Coral desk 2", /*id=*/base::Token(2, 3)));

  OverrideTestResponse(std::move(test_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Create a new desk to merge the active desk.
  NewDesk();

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with two coral chips.
  ASSERT_EQ(GetBirchChipsNum(), 2u);

  // There are two desks before removing.
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(desks_controller->GetNumberOfDesks(), 2);

  // Merging the active desk removes all chips.
  RemoveDesk(GetActiveDesk(), DeskCloseType::kCombineDesks);
  // There should be only one desk after merging.
  ASSERT_EQ(desks_controller->GetNumberOfDesks(), 1);

  // Two chips are removed.
  EXPECT_EQ(0u, GetBirchChipsNum());
}

// Tests that moving a window to another desk would update the groups and chips.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, MoveWindowToOtherDeskUpdateChip) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  // TODO(crbug.com/378159705): move this to a test helper.
  // Create a browser containing 8 tabs.
  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://mail.google.com"), GURL("https://youtube.com"),
       GURL("https://google.com"), GURL("https://earth.google.com"),
       GURL("https://maps.google.com"), GURL("https://docs.google.com"),
       GURL("https://calendar.google.com"), GURL("https://chat.google.com")});
  BrowserWindowInterface* const regular_browser =
      browser_created_observer->Wait();

  test::InstallSystemAppsForTesting(primary_profile);

  // Open a File window.
  browser_created_observer.emplace();
  test::CreateSystemWebApp(primary_profile, SystemWebAppType::FILE_MANAGER);
  BrowserWindowInterface* const file_browser = browser_created_observer->Wait();

  // Open a PWA window.
  test::InstallAndLaunchPWA(primary_profile, GURL("https://www.youtube.com/"),
                            /*launch_in_browser=*/false,
                            /*app_title=*/u"YouTube");

  // Create two fake coral groups.
  std::vector<coral::mojom::GroupPtr> test_groups;
  test_groups.push_back(
      CreateTestGroup({{"mail.google.com", GURL("https://mail.google.com")},
                       {"youtube.com", GURL("https://youtube.com")},
                       {"google.com", GURL("https://google.com")},
                       {"YouTube", "adnlfjpnmidfimlkaohpidplnoimahfh"}},
                      "Coral desk 1", /*id=*/base::Token(1, 2)));
  test_groups.push_back(CreateTestGroup(
      {{"maps.google.com", GURL("https://maps.google.com")},
       {"docs.google.com", GURL("https://docs.google.com")},
       {"calendar.google.com", GURL("https://calendar.google.com")},
       {"Files", "fkiggjmkendpmbegkagpmagjepfkpmeb"}},
      "Coral desk 2", /*id=*/base::Token(2, 3)));

  OverrideTestResponse(std::move(test_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  // Create another desk.
  NewDesk();

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // The birch bar is created with two coral chips.
  ASSERT_EQ(GetBirchChipsNum(), 2u);

  // Both groups initially have 4 entities.
  const auto& group_1 =
      BirchCoralProvider::Get()->GetGroupById(base::Token(1, 2));
  EXPECT_EQ(group_1->entities.size(), 4u);

  const auto& group_2 =
      BirchCoralProvider::Get()->GetGroupById(base::Token(2, 3));
  EXPECT_EQ(group_2->entities.size(), 4u);

  auto* desks_controller = DesksController::Get();

  auto* new_desk = desks_controller->GetDeskAtIndex(1);

  // Move the browser window to another desk.
  ASSERT_EQ(8, regular_browser->GetTabStripModel()->GetTabCount());
  auto* browser_window = regular_browser->GetWindow()->GetNativeWindow();
  desks_controller->MoveWindowFromActiveDeskTo(
      browser_window, new_desk, browser_window->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kSendToDesk);
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(browser_window));

  // Both groups are reduced to 1 entity.
  EXPECT_EQ(group_1->entities.size(), 1u);
  EXPECT_EQ(group_2->entities.size(), 1u);

  // Move the Files app to another desk.
  auto* file_window = file_browser->GetWindow()->GetNativeWindow();
  ASSERT_EQ(file_window->GetTitle(), u"Files");
  desks_controller->MoveWindowFromActiveDeskTo(
      file_window, new_desk, file_window->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kSendToDesk);
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(file_window));

  // The first chip is removed.
  EXPECT_EQ(GetBirchChipsNum(), 1u);
}

// Tests that consecutively launching groups to new desks works.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, ConsecutiveLaunchGroups) {
  // Create two browsers with different tabs and urls.
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://youtube.com"), GURL("https://google.com"),
       GURL("https://docs.google.com"), GURL("https://drive.google.com")});
  test::CreateAndShowBrowser(
      primary_profile,
      {GURL("https://maps.google.com"), GURL("https://mail.google.com"),
       GURL("https://calendar.google.com"), GURL("https://meet.google.com")});

  // Create two fake coral groups which contains tabs that are selected from
  // each of the two browsers created above.
  std::vector<coral::mojom::GroupPtr> fake_groups;
  fake_groups.push_back(
      CreateTestGroup({{"Youtube", GURL("https://youtube.com")},
                       {"Docs", GURL("https://docs.google.com")},
                       {"Google Maps", GURL("https://maps.google.com")},
                       {"Calendar", GURL("https://calendar.google.com")}},
                      "Coral 1"));
  fake_groups.push_back(
      CreateTestGroup({{"Google", GURL("https://google.com")},
                       {"Drive", GURL("https://drive.google.com")},
                       {"Gmail", GURL("https://mail.google.com")},
                       {"Meet", GURL("https://meet.google.com")}},
                      "Coral 2"));
  OverrideTestResponse(std::move(fake_groups));

  // Set up a callback for a birch data fetch.
  base::RunLoop birch_data_fetch_waiter;
  Shell::Get()->birch_model()->SetDataFetchCallbackForTest(
      birch_data_fetch_waiter.QuitClosure());

  ToggleOverview();
  WaitForOverviewEntered();

  // Wait for fetch callback to complete.
  birch_data_fetch_waiter.Run();

  // There should be two chips on the bar.
  ASSERT_EQ(GetBirchChipsNum(), 2u);

  // Launch the first group.
  test::Click(GetFirstCoralButton());

  // We should have two desks and the new active desk is the second one.
  DesksController* desks_controller = DesksController::Get();
  EXPECT_EQ(desks_controller->desks().size(), 2u);
  EXPECT_EQ(desks_controller->GetActiveDeskIndex(), 1);
  EXPECT_EQ(
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex()),
      u"Coral 1");

  // The active desk should have a browser window which has the tabs in the
  // first group.
  std::vector<GURL> tab_urls_on_active_desk = CollectTabURLsFromWindows(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_THAT(tab_urls_on_active_desk,
              testing::UnorderedElementsAre(
                  GURL("https://youtube.com"), GURL("https://docs.google.com"),
                  GURL("https://maps.google.com"),
                  GURL("https://calendar.google.com")));

  // Launch the second group.
  ASSERT_EQ(GetBirchChipsNum(), 1u);
  test::Click(GetFirstCoralButton());

  // We should have three desks and the new active desk is the third one.
  EXPECT_EQ(desks_controller->desks().size(), 3u);
  EXPECT_EQ(desks_controller->GetActiveDeskIndex(), 2);
  EXPECT_EQ(
      desks_controller->GetDeskName(desks_controller->GetActiveDeskIndex()),
      u"Coral 2");

  // The active desk should have a browser window which has the tabs in the
  // second group.
  tab_urls_on_active_desk = CollectTabURLsFromWindows(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_THAT(tab_urls_on_active_desk,
              testing::UnorderedElementsAre(GURL("https://google.com"),
                                            GURL("https://drive.google.com"),
                                            GURL("https://mail.google.com"),
                                            GURL("https://meet.google.com")));
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/mru_window_tracker.h"
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

}  // namespace

class CoralBrowserTest : public InProcessBrowserTest {
 public:
  CoralBrowserTest() {
    set_launch_browser_for_testing(nullptr);
  }
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
    command_line->AppendSwitch(switches::kForceBirchFakeCoral);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kBirchCoral};
};

IN_PROC_BROWSER_TEST_F(CoralBrowserTest, PRE_PostLoginBrowser) {
  // Ensure that post login overview shows up by having at least one app open
  // and immediate saving to bypass the 2.5 second throttle.
  CreateBrowser(ProfileManager::GetActiveUserProfile());
  AppLaunchInfoSaveWaiter::Wait();
}

// Launches a browser with the expected tabs when the post login coral chip is
// clicked.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, PostLoginBrowser) {
  test::InstallSystemAppsForTesting(ProfileManager::GetActiveUserProfile());

  // Wait until the chip is visible, it may not be visible while data fetch is
  // underway or the overview animation is still running.
  EXPECT_TRUE(base::test::RunUntil([]() {
    BirchChipButtonBase* coral_chip = GetBirchChipButton();
    return !!coral_chip;
  }));

  BirchChipButtonBase* coral_chip = GetBirchChipButton();
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);

  test::BrowsersWaiter waiter(/*expected_count=*/3);
  test::Click(coral_chip);
  waiter.Wait();

  // TODO(sammiequon): These tabs and apps are currently hardcoded in ash for
  // `switches::kForceBirchFakeCoral`. Update to use a test coral provider
  // instead.
  EXPECT_TRUE(
      base::ranges::any_of(*BrowserList::GetInstance(), [](Browser* browser) {
        TabStripModel* tab_strip_model = browser->tab_strip_model();
        return tab_strip_model->count() == 4 &&
               tab_strip_model->GetWebContentsAt(0)->GetVisibleURL() ==
                   GURL("https://www.ikea.com/") &&
               tab_strip_model->GetWebContentsAt(1)->GetVisibleURL() ==
                   GURL("https://www.figma.com/") &&
               tab_strip_model->GetWebContentsAt(2)->GetVisibleURL() ==
                   GURL("https://www.notion.so/") &&
               tab_strip_model->GetWebContentsAt(3)->GetVisibleURL() ==
                   GURL("https://www.nhl.com/");
      }));
  EXPECT_TRUE(
      base::ranges::any_of(*BrowserList::GetInstance(), [](Browser* browser) {
        return IsBrowserForSystemWebApp(browser, SystemWebAppType::SETTINGS);
      }));
  EXPECT_TRUE(
      base::ranges::any_of(*BrowserList::GetInstance(), [](Browser* browser) {
        return IsBrowserForSystemWebApp(browser,
                                        SystemWebAppType::FILE_MANAGER);
      }));
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

  // TODO(sammiequon): This title is currently hardcoded in ash for
  // `switches::kForceBirchFakeCoral`. Update to use a test coral provider
  // instead.
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
  coral::mojom::GroupPtr fake_group = coral::mojom::Group::New();
  fake_group->title = "Coral desk";

  fake_group->entities.push_back(
      coral::mojom::EntityKey::NewTabUrl(GURL("https://youtube.com")));
  fake_group->entities.push_back(
      coral::mojom::EntityKey::NewTabUrl(GURL("https://maps.google.com")));

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

}  // namespace ash

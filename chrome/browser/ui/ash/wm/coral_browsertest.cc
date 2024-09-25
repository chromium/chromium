// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/birch/birch_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/ash/util/ash_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace ash {

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
  // Wait until the chip is visible, it may not be visible while data fetch is
  // underway or the overview animation is still running.
  EXPECT_TRUE(base::test::RunUntil([]() {
    BirchChipButtonBase* coral_chip = GetBirchChipButton();
    return !!coral_chip;
  }));

  BirchChipButtonBase* coral_chip = GetBirchChipButton();
  ASSERT_EQ(coral_chip->GetItem()->GetType(), BirchItemType::kCoral);
  ui_test_utils::BrowserChangeObserver observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  test::Click(coral_chip);
  Browser* coral_browser = observer.Wait();

  // TODO(sammiequon): This tabs are currently hardcoded in ash for
  // `switches::kForceBirchFakeCoral`. Update to use a test coral provider
  // instead.
  TabStripModel* tab_strip_model = coral_browser->tab_strip_model();
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(GURL("https://www.ikea.com/"),
            tab_strip_model->GetWebContentsAt(0)->GetVisibleURL());
  EXPECT_EQ(GURL("https://www.nhl.com/"),
            tab_strip_model->GetWebContentsAt(1)->GetVisibleURL());
}

// Tests that clicking the in session coral button opens and activates a new
// desk.
IN_PROC_BROWSER_TEST_F(CoralBrowserTest, OpenNewDesk) {
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

}  // namespace ash

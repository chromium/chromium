// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace {

class BrowserFinderWithDesksTest : public InProcessBrowserTest {
 public:
  BrowserFinderWithDesksTest() = default;
  ~BrowserFinderWithDesksTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kVirtualDesks);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Create three desks (two other than the default).
    auto* desks_controller = ash::DesksController::Get();
    desks_controller->NewDesk(ash::DesksCreationRemovalSource::kButton);
    desks_controller->NewDesk(ash::DesksCreationRemovalSource::kButton);
  }

  void ActivateBrowser(Browser* browser) { browser->window()->Activate(); }

  Browser* CreateTestBrowser() {
    Browser* new_browser = CreateBrowser(browser()->profile());
    new_browser->window()->Show();
    ActivateBrowser(new_browser);
    return new_browser;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFinderWithDesksTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserFinderWithDesksTest, FindAnyBrowser) {
  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(3u, desks_controller->desks().size());
  auto* desk_1 = desks_controller->desks()[0].get();
  auto* desk_2 = desks_controller->desks()[1].get();
  auto* desk_3 = desks_controller->desks()[2].get();

  Browser* browser_1 = CreateTestBrowser();
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();
  auto* window_1 = browser_1->window()->GetNativeWindow();
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_EQ(browser_1, chrome::FindAnyBrowser(browser()->profile(), true));

  // Switch to desk_2 and create a browser there.
  ash::ActivateDesk(desk_2);
  EXPECT_TRUE(desk_2->is_active());
  Browser* browser_2 = CreateTestBrowser();
  auto* window_2 = browser_2->window()->GetNativeWindow();
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(window_2));

  // FindAnyBrowser should return the MRU browser, which is browser_2 in this
  // case.
  EXPECT_EQ(browser_2, chrome::FindAnyBrowser(browser()->profile(), true));

  // Switch to desk_3, no browsers on this desk, however, FindAnyBrowser should
  // still return browser_2.
  ash::ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_FALSE(desks_controller->BelongsToActiveDesk(window_2));
  EXPECT_EQ(browser_2, chrome::FindAnyBrowser(browser()->profile(), true));

  // Switch to desk_1 by activating browser_1. When we switch back to desk_3,
  // FindAnyBrowser() will return browser_1 as the MRU browser.
  ash::DeskSwitchAnimationWaiter waiter;
  ActivateBrowser(browser_1);
  waiter.Wait();

  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_EQ(browser_1, chrome::FindAnyBrowser(browser()->profile(), true));

  ash::ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_EQ(browser_1, chrome::FindAnyBrowser(browser()->profile(), true));
}

IN_PROC_BROWSER_TEST_F(BrowserFinderWithDesksTest, FindTabbedBrowser) {
  auto* desks_controller = ash::DesksController::Get();
  ASSERT_EQ(3u, desks_controller->desks().size());
  auto* desk_1 = desks_controller->desks()[0].get();
  auto* desk_2 = desks_controller->desks()[1].get();
  auto* desk_3 = desks_controller->desks()[2].get();

  Browser* browser_1 = CreateTestBrowser();
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();
  auto* window_1 = browser_1->window()->GetNativeWindow();
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_TRUE(desk_1->is_active());
  EXPECT_TRUE(desks_controller->BelongsToActiveDesk(window_1));
  EXPECT_EQ(browser_1, chrome::FindTabbedBrowser(browser()->profile(), true));

  // Switch to desk_2, expect that FindTabbedBrowser() favors the current desk.
  ash::ActivateDesk(desk_2);
  EXPECT_TRUE(desk_2->is_active());
  EXPECT_FALSE(chrome::FindTabbedBrowser(browser()->profile(), true));

  // Create a browser on desk_2, and expect that FindTabbedBrowser() to find it.
  Browser* browser_2 = CreateTestBrowser();
  EXPECT_EQ(browser_2, chrome::FindTabbedBrowser(browser()->profile(), true));

  ash::ActivateDesk(desk_3);
  EXPECT_TRUE(desk_3->is_active());
  EXPECT_FALSE(chrome::FindTabbedBrowser(browser()->profile(), true));
}

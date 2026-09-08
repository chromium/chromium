// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_test_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/base/page_transition_types.h"

using PinnedTabServiceBrowserTest = InProcessBrowserTest;

// Makes sure pinned tabs are updated when tabstrip is empty.
// http://crbug.com/40519327
IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest, TabStripEmpty) {
  Profile* profile = browser()->GetProfile();
  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  PinnedTabCodec::WritePinnedTabs(profile);
  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("https://www.google.com/:pinned", result);

  // When tab strip is empty, browser window will be closed and PinnedTabService
  // must update data on this event.
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  tab_strip_model->SetTabPinned(0, false);
  int previous_tab_count = tab_strip_model->count();
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(previous_tab_count - 1, tab_strip_model->count());
  observer.Wait();

  // Let's see it's cleared out properly.
  result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest, CloseWindow) {
  Profile* profile = browser()->GetProfile();
  EXPECT_TRUE(PinnedTabServiceFactory::GetForProfile(profile));
  EXPECT_TRUE(profile->GetPrefs());

  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  browser()->GetWindow()->Close();
  observer.Wait();

  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("https://www.google.com/:pinned", result);
}

// Makes sure closing a popup triggers writing pinned tabs.
IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest, Popup) {
  Profile* profile = browser()->GetProfile();
  EXPECT_TRUE(PinnedTabServiceFactory::GetForProfile(profile));
  EXPECT_TRUE(profile->GetPrefs());

  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  // Create a popup browser.
  BrowserWindowInterface* popup_browser =
      CreateBrowserWindow(BrowserWindowCreateParams(
          BrowserWindowInterface::TYPE_POPUP, browser()->GetProfile(),
          /*from_user_gesture=*/true));
  ASSERT_EQ(popup_browser->GetType(), BrowserWindowInterface::Type::TYPE_POPUP);

  // Close the browser. This should trigger saving the tabs. No need to destroy
  // the browser (this happens automatically in the test destructor).
  UnloadController::From(browser())->OnWindowClosing();

  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("https://www.google.com/:pinned", result);

  // Close the popup browser. This shouldn't reset the saved state.
  popup_browser->GetTabStripModel()->CloseAllTabs();

  // Check the state to make sure it hasn't changed.
  result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("https://www.google.com/:pinned", result);
}

// Makes sure pinned tabs are not restored when the last pinned tab is closed
// directly (e.g. via keyboard shortcut or close button), rather than closing
// the window.
IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest, ClosePinnedTab) {
  Profile* profile = browser()->GetProfile();
  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  PinnedTabCodec::WritePinnedTabs(profile);
  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("https://www.google.com/:pinned", result);

  // Close the pinned tab without unpinning it first.
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  observer.Wait();

  // The pinned tab should not be restored.
  result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest, CloseSelectedPinnedTab) {
  Profile* profile = browser()->GetProfile();
  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  PinnedTabCodec::WritePinnedTabs(profile);
  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("https://www.google.com/:pinned", result);

  // Close the selected pinned tab (as would happen with Ctrl+W).
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  tab_strip_model->CloseSelectedTabs();
  observer.Wait();

  // The pinned tab should not be restored.
  result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_TRUE(result.empty());
}

// Tests the exact scenario from the bug report:
// 1. Open and pin a tab.
// 2. Press Ctrl+W twice (CloseTab accelerator) to close the pinned tab.
// 3. Confirm that the pinned tab is not restored upon relaunch.
IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest,
                       ClosePinnedTabViaAccelerator) {
  Profile* profile = browser()->GetProfile();
  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->SetTabPinned(0, true);

  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  ui_test_utils::BrowserDestroyedObserver observer(browser());

  // First accelerator press triggers confirmation toast.
  chrome::CloseTab(browser());
  EXPECT_EQ(1, tab_strip_model->count());

  // Second accelerator press closes the pinned tab and closes the window.
  chrome::CloseTab(browser());
  observer.Wait();

  // The pinned tab should not be restored.
  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest,
                       MultipleWindowsCloseFirstPinnedTabViaAccelerator) {
  Profile* profile = browser()->GetProfile();
  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);
  browser()->tab_strip_model()->SetTabPinned(0, true);

  BrowserWindowInterface* browser2 = CreateBrowser(profile);
  browser2->GetTabStripModel()->SetTabPinned(0, true);

  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Close browser's pinned tab via accelerator.
  {
    ui_test_utils::BrowserDestroyedObserver observer(browser());
    chrome::CloseTab(browser());
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    chrome::CloseTab(browser());
    observer.Wait();
  }

  // Now close browser2 by closing the window.
  {
    ui_test_utils::BrowserDestroyedObserver observer(browser2);
    browser2->GetWindow()->Close();
    observer.Wait();
  }

  // Only browser2's pinned tab (about:blank) should be saved.
  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_EQ("about:blank:pinned", result);
}

IN_PROC_BROWSER_TEST_F(PinnedTabServiceBrowserTest,
                       MultipleWindowsCloseLastPinnedTabViaAccelerator) {
  Profile* profile = browser()->GetProfile();
  GURL url("https://www.google.com");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);
  browser()->tab_strip_model()->SetTabPinned(0, true);

  BrowserWindowInterface* browser2 = CreateBrowser(profile);
  browser2->GetTabStripModel()->SetTabPinned(0, true);

  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Close browser by closing the window first.
  {
    ui_test_utils::BrowserDestroyedObserver observer(browser());
    browser()->GetWindow()->Close();
    observer.Wait();
  }

  // Now close browser2's pinned tab via accelerator.
  {
    ui_test_utils::BrowserDestroyedObserver observer(browser2);
    chrome::CloseTab(browser2);
    EXPECT_EQ(1, browser2->GetTabStripModel()->count());
    chrome::CloseTab(browser2);
    observer.Wait();
  }

  // Since browser2 was the last window and its pinned tab was closed by user,
  // no pinned tabs should be restored.
  std::string result =
      PinnedTabTestUtils::TabsToString(PinnedTabCodec::ReadPinnedTabs(profile));
  EXPECT_TRUE(result.empty());
}

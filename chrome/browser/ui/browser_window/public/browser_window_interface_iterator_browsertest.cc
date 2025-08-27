// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BrowserWindowInterfaceIteratorBrowserTest = InProcessBrowserTest;

// Test that GetLastActiveBrowserWindowInterfaceWithAnyProfile returns the most
// recently activated browser.
// TODO(crbug.com/431671448): Disable test on Linux until passing.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_GetLastActiveBrowserWindowInterface_ReturnsLastActive \
  DISABLED_GetLastActiveBrowserWindowInterface_ReturnsLastActive
#else
#define MAYBE_GetLastActiveBrowserWindowInterface_ReturnsLastActive \
  GetLastActiveBrowserWindowInterface_ReturnsLastActive
#endif
IN_PROC_BROWSER_TEST_F(
    BrowserWindowInterfaceIteratorBrowserTest,
    MAYBE_GetLastActiveBrowserWindowInterface_ReturnsLastActive) {
  // Start with the default browser created by the test framework.
  Browser* const browser1 = browser();

  // Verify initial state - the default browser should be the last active.
  EXPECT_EQ(GetLastActiveBrowserWindowInterfaceWithAnyProfile(), browser1);

  // Create a second browser window.
  Browser* const browser2 =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  ASSERT_NE(browser2, nullptr);
  browser2->window()->Show();

  // Activate the second browser.
  browser2->window()->Activate();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return GetLastActiveBrowserWindowInterfaceWithAnyProfile() == browser2;
  }));

  // Activate the first browser again.
  browser1->window()->Activate();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return GetLastActiveBrowserWindowInterfaceWithAnyProfile() == browser1;
  }));
}

IN_PROC_BROWSER_TEST_F(
    BrowserWindowInterfaceIteratorBrowserTest,
    ForEachCurrentBrowserWindowInterfaceOrderedByActivationEmpty) {
  // We start with one browser window already open, so we have to close it to
  // test the empty case.
  CloseBrowserSynchronously(browser());

  bool was_called = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        was_called = true;
        return true;
      });
  EXPECT_FALSE(was_called);
}

IN_PROC_BROWSER_TEST_F(
    BrowserWindowInterfaceIteratorBrowserTest,
    ForEachCurrentBrowserWindowInterfaceOrderedByActivationStopsIteratingWhenFalseIsReturned) {
  // We start with one Browser already
  CreateBrowser(GetProfile());
  CreateBrowser(GetProfile());

  int i = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        i++;
        return i == 1;
      });

  EXPECT_EQ(i, 2);
}

IN_PROC_BROWSER_TEST_F(
    BrowserWindowInterfaceIteratorBrowserTest,
    ForEachCurrentBrowserWindowInterfaceOrderedByActivationAddRemove) {
  Browser* browser_window_1 = browser();
  Browser* browser_window_2 = CreateBrowser(GetProfile());
  Browser* browser_window_3 = CreateBrowser(GetProfile());

  std::vector<BrowserWindowInterface*> visited;
  int i = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        visited.push_back(browser_window);

        if (i == 0) {
          // Create a browser while the list is scanned.
          CreateBrowser(GetProfile());

          // Remove a browser while the list is scanned.
          CloseBrowserSynchronously(browser_window_2);
        }

        i++;
        return true;
      });

  // In this test, windows are activated when they are created. Therefore
  // browser_window_3 is the most recently activated, followed by
  // browser_window_2, then browser_window_1. However, browser_window_2 was
  // removed during iteration, so it won't appear in the output. The would-be
  // browser_window_4 is not added because this is
  // ForEachCurrentBrowserWindowInterfaceOrderedByActivation(), not
  // ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation().
  EXPECT_THAT(visited,
              testing::ElementsAre(browser_window_3, browser_window_1));
}

IN_PROC_BROWSER_TEST_F(
    BrowserWindowInterfaceIteratorBrowserTest,
    ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivationEmpty) {
  // We start with one browser window already open, so we have to close it to
  // test the empty case.
  CloseBrowserSynchronously(browser());

  bool was_called = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        was_called = true;
        return true;
      });
  EXPECT_FALSE(was_called);
}

IN_PROC_BROWSER_TEST_F(
    BrowserWindowInterfaceIteratorBrowserTest,
    ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivationStopsIteratingWhenFalseIsReturned) {
  // We start with one Browser already
  CreateBrowser(GetProfile());
  CreateBrowser(GetProfile());

  int i = 0;
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        i++;
        return i == 1;
      });

  EXPECT_EQ(i, 2);
}

IN_PROC_BROWSER_TEST_F(
    BrowserWindowInterfaceIteratorBrowserTest,
    ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivationAddRemove) {
  Browser* browser_window_1 = browser();
  Browser* browser_window_2 = CreateBrowser(GetProfile());
  Browser* browser_window_3 = CreateBrowser(GetProfile());
  Browser* browser_window_4 = nullptr;

  std::vector<BrowserWindowInterface*> visited;
  int i = 0;
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser_window) {
        visited.push_back(browser_window);

        if (i == 0) {
          // Create a browser while the list is scanned.
          browser_window_4 = CreateBrowser(GetProfile());

          // Remove a browser while the list is scanned.
          CloseBrowserSynchronously(browser_window_2);
        }

        i++;
        return true;
      });

  // In this test, windows are activated when they are created. Therefore
  // browser_window_3 is the most recently activated, followed by
  // browser_window_2, then browser_window_1. However, browser_window_2 was
  // removed during iteration, so it won't appear in the output, and
  // browser_window_4 is appended to the end of the iteration since it was added
  // in the middle.
  EXPECT_THAT(visited, testing::ElementsAre(browser_window_3, browser_window_1,
                                            browser_window_4));
}

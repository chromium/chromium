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

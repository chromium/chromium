// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_codec.h"

#include <string>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/tabs/pinned_tab_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using PinnedTabCodecBrowserTest = InProcessBrowserTest;

// Make sure nothing is restored when the browser has no pinned tabs.
IN_PROC_BROWSER_TEST_F(PinnedTabCodecBrowserTest, NoPinnedTabs) {
  GURL url1("https://www.google.com");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  PinnedTabCodec::WritePinnedTabs(browser()->profile());

  std::string result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(browser()->profile()));
  EXPECT_EQ("", result);
}

// Creates a browser with one pinned tab and one normal tab, does restore and
// makes sure we get back another pinned tab.
IN_PROC_BROWSER_TEST_F(PinnedTabCodecBrowserTest, PinnedAndNonPinned) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.google.com/2");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close about:blank since we don't need it.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);

  browser()->tab_strip_model()->SetTabPinned(0, true);

  PinnedTabCodec::WritePinnedTabs(browser()->profile());

  StartupTabs pinned_tabs =
      PinnedTabCodec::ReadPinnedTabs(browser()->profile());
  std::string result = PinnedTabTestUtils::TabsToString(pinned_tabs);
  EXPECT_EQ("https://www.google.com/:pinned", result);

  // Update pinned tabs and restore back the old value directly.
  browser()->tab_strip_model()->SetTabPinned(1, true);

  PinnedTabCodec::WritePinnedTabs(browser()->profile());
  result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(browser()->profile()));
  EXPECT_EQ("https://www.google.com/:pinned https://www.google.com/2:pinned",
            result);

  PinnedTabCodec::WritePinnedTabs(browser()->profile(), pinned_tabs);
  result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(browser()->profile()));
  EXPECT_EQ("https://www.google.com/:pinned", result);
}

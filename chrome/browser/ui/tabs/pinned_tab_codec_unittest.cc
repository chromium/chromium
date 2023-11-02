// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_codec.h"

#include <string>
#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/tabs/pinned_tab_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using PinnedTabCodecTest = BrowserWithTestWindowTest;

// Make sure nothing is restored when the browser has no pinned tabs.
TEST_F(PinnedTabCodecTest, NoPinnedTabs) {
  GURL url1("http://www.google.com");
  AddTab(browser(), url1);

  PinnedTabCodec::WritePinnedTabs(profile());

  std::string result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("", result);
}

// Creates a browser with one pinned tab and one normal tab, does restore and
// makes sure we get back another pinned tab.
TEST_F(PinnedTabCodecTest, PinnedAndNonPinned) {
  GURL url1("http://www.google.com");
  GURL url2("http://www.google.com/2");
  AddTab(browser(), url2);

  // AddTab inserts at index 0, so order after this is url1, url2.
  AddTab(browser(), url1);

  browser()->tab_strip_model()->SetTabPinned(0, true);

  PinnedTabCodec::WritePinnedTabs(profile());

  StartupTabs pinned_tabs = PinnedTabCodec::ReadPinnedTabs(profile());
  std::string result = PinnedTabTestUtils::TabsToString(pinned_tabs);
  EXPECT_EQ("http://www.google.com/:pinned", result);

  // Update pinned tabs and restore back the old value directly.
  browser()->tab_strip_model()->SetTabPinned(1, true);

  PinnedTabCodec::WritePinnedTabs(profile());
  result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("http://www.google.com/:pinned http://www.google.com/2:pinned",
            result);

  PinnedTabCodec::WritePinnedTabs(profile(), pinned_tabs);
  result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("http://www.google.com/:pinned", result);
}

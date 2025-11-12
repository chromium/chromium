// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"

using BrowserFinderTest = BrowserWithTestWindowTest;

TEST_F(BrowserFinderTest, ScheduledForDeletion) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(browser(), chrome::FindBrowserWithProfile(profile()));
  // Add a tab as the tabstrip starts empty and CloseAllTabs() effectively
  // does nothing if there are no tabs (meaning Browser deletion isn't
  // scheduled).
  AddTab(browser(), GURL("http://foo.chromium.org"));
  std::unique_ptr<Browser> browser = release_browser();
  browser->tab_strip_model()->CloseAllTabs();
  // This is normally invoked when the tab strip is empty (specifically from
  // BrowserView::OnWindowCloseRequested).
  browser->OnWindowClosing();
  EXPECT_TRUE(browser->is_delete_scheduled());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(nullptr, chrome::FindBrowserWithProfile(profile()));
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using BrowserFinderBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserFinderBrowserTest, ScheduledForDeletion) {
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());

  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(new_browser);

  EXPECT_EQ(2u, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(new_browser,
            ProfileBrowserCollection::GetForProfile(browser()->profile())
                ->GetLastActiveBrowser());

  // Close all tabs. The tabstrip starts with one blank tab (created by
  // CreateBrowser), and CloseAllTabs() is required to schedule browser deletion
  // during OnWindowClosing().
  new_browser->tab_strip_model()->CloseAllTabs();

  new_browser->OnWindowClosing();

  EXPECT_TRUE(new_browser->IsDeleteScheduled());
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_NE(new_browser,
            ProfileBrowserCollection::GetForProfile(browser()->profile())
                ->GetLastActiveBrowser());
}

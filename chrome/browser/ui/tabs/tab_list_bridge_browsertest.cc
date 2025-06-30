// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_list_bridge.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/window_open_disposition.h"

// TODO(devlin): Would it make sense to port this to instead be a
// TabListInterface browsertest, and use it on all relevant platforms?
using TabListBridgeBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TabListBridgeBrowserTest, GetTab) {
  const GURL url1("http://one.example");
  const GURL url2("http://two.example");

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabListInterface* tab_list_interface = TabListBridge::From(browser());
  ASSERT_TRUE(tab_list_interface);

  tabs::TabInterface* tab1 = tab_list_interface->GetTab(0);
  ASSERT_TRUE(tab1);
  EXPECT_EQ(url1, tab1->GetContents()->GetLastCommittedURL());

  tabs::TabInterface* tab2 = tab_list_interface->GetTab(1);
  ASSERT_TRUE(tab2);
  EXPECT_EQ(url2, tab2->GetContents()->GetLastCommittedURL());
}

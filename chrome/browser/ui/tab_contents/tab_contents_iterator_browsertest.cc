// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"

using TabContentsIteratorBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TabContentsIteratorBrowserTest, ForEachTabInterface) {
  // By default, there is one browser with one tab.
  int tab_count = 0;
  tabs::ForEachTabInterface([&](tabs::TabInterface* tab) {
    tab_count++;
    return true;
  });
  EXPECT_EQ(1, tab_count);

  // Add another tab.
  std::ignore = AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
  tab_count = 0;
  tabs::ForEachTabInterface([&](tabs::TabInterface* tab) {
    tab_count++;
    return true;
  });
  EXPECT_EQ(2, tab_count);

  // Create a new browser.
  CreateBrowser(browser()->GetProfile());
  tab_count = 0;
  tabs::ForEachTabInterface([&](tabs::TabInterface* tab) {
    tab_count++;
    return true;
  });

  // The new browser has one tab, so 2 + 1 = 3.
  EXPECT_EQ(3, tab_count);
}

IN_PROC_BROWSER_TEST_F(TabContentsIteratorBrowserTest,
                       ForEachTabInterface_EarlyExit) {
  // Add a couple of tabs.
  std::ignore = AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
  std::ignore = AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);

  int tab_count = 0;
  tabs::ForEachTabInterface([&](tabs::TabInterface* tab) {
    tab_count++;
    return false;  // Stop after the first tab.
  });

  EXPECT_EQ(1, tab_count);
}

IN_PROC_BROWSER_TEST_F(TabContentsIteratorBrowserTest,
                       ForEachTabInterface_TabDestructionDuringIteration) {
  // Add a couple of tabs.
  std::ignore = AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
  std::ignore = AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK);
  TabStripModel* const tab_strip_model = browser()->GetTabStripModel();
  EXPECT_EQ(3, tab_strip_model->count());

  int tab_count = 0;
  tabs::ForEachTabInterface([&](tabs::TabInterface* tab) {
    if (tab_count == 0) {
      // Remove the next tab in the current Browser.
      EXPECT_EQ(browser(), tab->GetBrowserWindowInterface());
      EXPECT_EQ(0, tab_strip_model->GetIndexOfTab(tab));

      tab_strip_model->DetachAndDeleteWebContentsAt(1);
      EXPECT_EQ(2, tab_strip_model->count());
    }

    tab_count++;
    return true;  // Stop after the first tab.
  });

  EXPECT_EQ(2, tab_count);
}

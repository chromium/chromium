// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_attachment_tracker.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tabs {

class TabAttachmentTrackerBrowserTest : public InProcessBrowserTest {
 protected:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }
};

IN_PROC_BROWSER_TEST_F(TabAttachmentTrackerBrowserTest,
                       TracksInitialAndMovedAttachments) {
  // 1. Verify the initial tab has an attachment count of 1.
  TabInterface* first_tab = tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(first_tab);

  TabAttachmentTracker* tracker = TabAttachmentTracker::From(first_tab);
  ASSERT_TRUE(tracker);
  EXPECT_EQ(tracker->attachment_count(), 1);

  // 2. Create a second browser window.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(browser2);

  // 3. Move the tab from the first browser to the second browser.
  std::unique_ptr<TabModel> tab_model =
      tab_strip_model()->DetachTabAtForInsertion(0);
  ASSERT_TRUE(tab_model);

  browser2->tab_strip_model()->InsertDetachedTabAt(0, std::move(tab_model),
                                                   AddTabTypes::ADD_ACTIVE);

  // 4. Verify that the tracker's attachment count has incremented to 2.
  EXPECT_EQ(tracker->attachment_count(), 2);
}

}  // namespace tabs

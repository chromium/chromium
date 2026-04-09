// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_data.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace tabs {

class TabGroupDataBrowserTest : public InProcessBrowserTest {
 public:
  TabGroupDataBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void UpdateTitle(content::WebContents* contents,
                   const std::u16string& title) {
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry);
    contents->UpdateTitleForEntry(entry, title);
  }
};

IN_PROC_BROWSER_TEST_F(TabGroupDataBrowserTest, NotifyOnTabAdded) {
  // Create tab group
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  TabGroup* tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  // create the TabGroupDataObserver to the group
  TabGroupDataObserver observer(tab_group);

  bool was_notified = false;
  base::CallbackListSubscription subscription =
      observer.RegisterTabGroupDataChangedCallback(base::BindRepeating(
          [](bool* was_notified) { *was_notified = true; }, &was_notified));

  // add a tab to the group
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->AddToExistingGroup({1}, group_id);
  EXPECT_TRUE(was_notified);
}

IN_PROC_BROWSER_TEST_F(TabGroupDataBrowserTest, NotifyOnTabDataChanged) {
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  TabGroup* tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  TabGroupDataObserver observer(tab_group);

  bool was_notified = false;
  base::CallbackListSubscription subscription =
      observer.RegisterTabGroupDataChangedCallback(base::BindRepeating(
          [](bool* was_notified) { *was_notified = true; }, &was_notified));

  // update the title for the tab in the group
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  UpdateTitle(web_contents, u"New Title");
  EXPECT_TRUE(was_notified);
}

IN_PROC_BROWSER_TEST_F(TabGroupDataBrowserTest, NotifyOnTabRemovedFromGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 2);
  const tab_groups::TabGroupId group_id =
      tab_strip_model->AddToNewGroup({0, 1});
  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(group_id);

  TabGroupDataObserver observer(tab_group);
  bool was_notified = false;
  base::CallbackListSubscription subscription =
      observer.RegisterTabGroupDataChangedCallback(base::BindRepeating(
          [](bool* was_notified) { *was_notified = true; }, &was_notified));
  tab_strip_model->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  EXPECT_TRUE(was_notified);
}

IN_PROC_BROWSER_TEST_F(TabGroupDataBrowserTest, NotifyOnTabMovedInGroup) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("example.com", "/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->count(), 2);
  const tab_groups::TabGroupId group_id =
      tab_strip_model->AddToNewGroup({0, 1});
  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(group_id);

  const GURL first_tab_url =
      tab_strip_model->GetWebContentsAt(0)->GetLastCommittedURL();
  const GURL second_tab_url =
      tab_strip_model->GetWebContentsAt(1)->GetLastCommittedURL();
  EXPECT_NE(first_tab_url, second_tab_url);

  TabGroupDataObserver observer(tab_group);
  const tabs::TabGroupData tab_group_data = observer.tab_group_data();
  EXPECT_EQ(tab_group_data.tab_data[0].last_committed_url, first_tab_url);
  EXPECT_EQ(tab_group_data.tab_data[1].last_committed_url, second_tab_url);

  bool was_notified = false;
  base::CallbackListSubscription subscription =
      observer.RegisterTabGroupDataChangedCallback(base::BindRepeating(
          [](bool* was_notified) { *was_notified = true; }, &was_notified));

  tab_strip_model->MoveWebContentsAt(0, 1, false);
  EXPECT_TRUE(was_notified);

  tabs::TabGroupData updated_tab_group_data = observer.tab_group_data();
  EXPECT_EQ(updated_tab_group_data.tab_data[0].last_committed_url,
            second_tab_url);
  EXPECT_EQ(updated_tab_group_data.tab_data[1].last_committed_url,
            first_tab_url);
}

}  // namespace tabs

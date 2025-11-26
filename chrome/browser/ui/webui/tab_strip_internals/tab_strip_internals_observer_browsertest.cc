// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_observer.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// TODO(crbug.com/427204855): Revisit the flaky tests that currently use
// `Times(AtLeast())` expectations. The number of callback invocations should
// ideally be deterministic and predictable for each test case. Consider
// replacing them with exact expectations once the underlying cause of
// nondeterminism is understood. Investigate whether using a host resolver
// helps stabilize tests that internally trigger navigation related events.
// For example, tests triggering a navigation to "chrome://newtab/" or
// "chrome://about:blank"
using ::testing::AtLeast;

// Tests that the TabStripInternalsObserver correctly forwards callbacks
// from TabStripModelObserver and BrowserListObserver.
using TabStripInternalsObserverBrowserTest = InProcessBrowserTest;

// BrowserAdded: Observe when a browser is added to BrowserList.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest, BrowserAdded) {
  BrowserWindowInterface* extra_browser = nullptr;
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  // Local scope to isolate OnBrowserAdded and avoid capturing OnBrowserRemoved.
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(AtLeast(1));

    extra_browser = CreateBrowser(browser()->profile());
  }
  ASSERT_TRUE(extra_browser);
  CloseBrowserSynchronously(extra_browser);
}

// BrowserRemoved: Observe when a browser is removed from the BrowserList.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest, BrowserRemoved) {
  BrowserWindowInterface* extra_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(extra_browser);
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  // Local scope to isolate OnBrowserRemoved triggered due to removal of
  // `extra_browser`. Prevents capturing OnBrowserRemoved for the main test
  // browser.
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(AtLeast(1));

    CloseBrowserSynchronously(extra_browser);
  }
}

// TabStripModelChanged: Observe when a tab is added.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabStripModelChanged) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(AtLeast(1));

    ASSERT_TRUE(
        AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  }
}

// TabGroupChanged: Observe when a tab group is created i.e. trigger
// OnTabGroupChanged with kCreated.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabGroupChanged_GroupCreated) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(4);

    model->AddToNewGroup({0, 1});
  }
}

// TabGroupChanged: Observe when a tab group is closed i.e. trigger
// OnTabGroupChanged with kClosed.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabGroupChanged_GroupClosed) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_id));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(AtLeast(4));

    model->CloseAllTabsInGroup(group_id);
  }
  EXPECT_FALSE(model->group_model()->ContainsTabGroup(group_id));
}

// TabGroupChanged: Observe when a tab group's collapsed state changes,
// triggering OnTabGroupChanged(TabGroupChange::kVisualsChanged).
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabGroupChanged_CollapsedStateChanged) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_id));
  const auto* group = model->group_model()->GetTabGroup(group_id);
  ASSERT_TRUE(group);
  const tab_groups::TabGroupVisualData* old_visuals = group->visual_data();
  bool old_collapsed = old_visuals->is_collapsed();
  auto old_title = old_visuals->title();
  auto old_color = old_visuals->color();
  tab_groups::TabGroupVisualData new_visuals(
      old_visuals->title(), old_visuals->color(), !old_visuals->is_collapsed());
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(1);

    model->ChangeTabGroupVisuals(group_id, new_visuals);
  }
  const tab_groups::TabGroupVisualData* updated_visuals =
      model->group_model()->GetTabGroup(group_id)->visual_data();
  EXPECT_NE(updated_visuals->is_collapsed(), old_collapsed);
  EXPECT_EQ(updated_visuals->title(), old_title);
  EXPECT_EQ(updated_visuals->color(), old_color);
}

// TODO(crbug.com/454725279): This and SplitRemoved are failing. Re-enable these
// tests.
// SplitTabChanged: Observe when a split tab is added, triggering
// OnSplitTabChanged(SplitTabChange::kAdded).
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       DISABLED_SplitTabChanged_SplitAdded) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(3, model->count());
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(2);

    split_tabs::SplitTabId split_id =
        model->AddToNewSplit({0}, split_tabs::SplitTabVisualData(),
                             split_tabs::SplitTabCreatedSource::kToolbarButton);
    EXPECT_TRUE(model->ContainsSplit(split_id));
  }
  ASSERT_TRUE(model->GetTabAtIndex(0)->GetSplit().has_value());
  ASSERT_TRUE(model->GetTabAtIndex(1)->GetSplit().has_value());
  EXPECT_EQ(model->GetTabAtIndex(0)->GetSplit().value(),
            model->GetTabAtIndex(1)->GetSplit().value());
}

// SplitTabChanged: Observe when a split tab is removed, triggering
// OnSplitTabChanged(SplitTabChange::kRemoved).
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       DISABLED_SplitTabChanged_SplitRemoved) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  split_tabs::SplitTabId split_id =
      model->AddToNewSplit({0}, split_tabs::SplitTabVisualData(),
                           split_tabs::SplitTabCreatedSource::kToolbarButton);
  ASSERT_TRUE(model->ContainsSplit(split_id));
  ASSERT_TRUE(model->GetTabAtIndex(0)->GetSplit().has_value());
  ASSERT_TRUE(model->GetTabAtIndex(1)->GetSplit().has_value());
  EXPECT_EQ(model->GetTabAtIndex(0)->GetSplit().value(),
            model->GetTabAtIndex(1)->GetSplit().value());
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(2);

    model->RemoveSplit(split_id);
  }
  EXPECT_FALSE(model->ContainsSplit(split_id));
  EXPECT_FALSE(model->GetTabAtIndex(0)->GetSplit().has_value());
  EXPECT_FALSE(model->GetTabAtIndex(1)->GetSplit().has_value());
}

// TabChangedAt: Observe when a tab's content changes.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest, TabChangedAt) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(AtLeast(1));

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
  }
}

// TabPinnedStateChanged: Observe when a tab's pinned state changes.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabPinnedStateChanged) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(1);

    browser()->tab_strip_model()->SetTabPinned(0, true);
  }
}

// TabBlockedStateChanged: Observe when a tab's blocked state changes.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabBlockedStateChanged) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(1);

    browser()->tab_strip_model()->SetTabBlocked(0, true);
  }
}

// TabGroupedStateChanged: Observe when a tab is added to an existing group.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabGroupedStateChanged_TabAddedToGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({0});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_id));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(1);

    model->AddToExistingGroup({1}, group_id);
  }
  EXPECT_EQ(model->GetTabGroupForTab(1), group_id);
}

// TabGroupedStateChanged: Observe when a tab is being removed from a group.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabGroupedStateChanged_TabRemovedFromGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_id));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(1);

    model->RemoveFromGroup({1});
  }
  EXPECT_FALSE(model->GetTabGroupForTab(1).has_value());
}

// TabGroupedStateChanged: Observe when a tab moves from one group to another.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabGroupedStateChanged_TabMovedBetweenGroups) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(2, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(3, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));

  TabStripModel* model = browser()->tab_strip_model();
  tab_groups::TabGroupId group_a = model->AddToNewGroup({0, 1});
  tab_groups::TabGroupId group_b = model->AddToNewGroup({2, 3});
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_a));
  ASSERT_TRUE(model->group_model()->ContainsTabGroup(group_b));
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(2);

    // Move tab at index 1 from group A to group B.
    model->RemoveFromGroup({1});
    model->AddToExistingGroup({1}, group_b);
  }
  EXPECT_EQ(model->GetTabGroupForTab(1), group_b);
}

// TabRestoreServiceChanged: Observe when a tab moves to TabRestoreService.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabRestoreServiceChanged_TabAdded) {
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(AtLeast(1));

    // Close an existing tab.
    browser()->tab_strip_model()->CloseWebContentsAt(
        1, TabCloseTypes::CLOSE_USER_GESTURE);
  }
}

// TabRestoreServiceDestroyed: Callback should not be invoked when service is
// destroyed.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabRestoreServiceDestroyed_NoCallback) {
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(browser()->profile(),
                                       mock_callback.Get());
    EXPECT_CALL(mock_callback, Run()).Times(0);

    sessions::TabRestoreService* service =
        TabRestoreServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(service);
    // Invoke for code coverage.
    observer.TabRestoreServiceDestroyed(service);
  }
}

// TabRestoreService_OTR_Profile: Callback should not be invoked for an OTR
// profile browser by the TabRestoreService.
IN_PROC_BROWSER_TEST_F(TabStripInternalsObserverBrowserTest,
                       TabRestoreService_OTR_Profile_NoCallback) {
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(otr_browser);
  Profile* otr_profile = otr_browser->profile();
  ASSERT_TRUE(otr_profile->IsOffTheRecord());
  base::MockCallback<base::RepeatingCallback<void()>> mock_callback;
  {
    TabStripInternalsObserver observer(otr_profile, mock_callback.Get());
    // Use the expected callback count triggered by the TabStripModel to confirm
    // that the TabRestoreService did not trigger any additional callbacks.
    EXPECT_CALL(mock_callback, Run()).Times(1);

    // Close an incognito browser tab.
    otr_browser->tab_strip_model()->CloseWebContentsAt(
        0, TabCloseTypes::CLOSE_USER_GESTURE);
  }
  CloseBrowserSynchronously(otr_browser);
}

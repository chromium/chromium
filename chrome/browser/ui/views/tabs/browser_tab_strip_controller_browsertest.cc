// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/test/views_test_utils.h"

namespace {
ui::MouseEvent dummy_event_ = ui::MouseEvent(ui::EventType::kMousePressed,
                                             gfx::PointF(),
                                             gfx::PointF(),
                                             base::TimeTicks::Now(),
                                             0,
                                             0);
}

class BrowserTabStripControllerTestBase : public InProcessBrowserTest {
 public:
  TabStripController* controller() {
    return browser()->GetBrowserView().tabstrip()->controller();
  }
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }
  TabStrip* tabstrip() {
    return browser()->GetBrowserView().tab_strip_region_view()->tab_strip();
  }
};

class BrowserTabStripControllerTestAddTabActiveGroupEnabled
    : public BrowserTabStripControllerTestBase {
 public:
  BrowserTabStripControllerTestAddTabActiveGroupEnabled() {
    scoped_feature_list_.InitWithFeatures({features::kNewTabAddsToActiveGroup},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class BrowserTabStripControllerTestAddTabActiveGroupDisabled
    : public BrowserTabStripControllerTestBase {
 public:
  BrowserTabStripControllerTestAddTabActiveGroupDisabled() {
    scoped_feature_list_.InitWithFeatures({},
                                          {features::kNewTabAddsToActiveGroup});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserTabStripControllerTestAddTabActiveGroupEnabled,
                       AddTabsWithActiveTabGroup) {
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  EXPECT_EQ(tab_strip_model()->count(), 4);

  tab_groups::TabGroupId group_id = tab_strip_model()->AddToNewGroup({1, 2});

  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));

  // Select a tab in the group.
  controller()->SelectTab(1, dummy_event_);
  controller()->CreateNewTab();

  // Create a new tab, it should be at position 3 because
  // there is an active tab group
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(0));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(4));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(5));

  // Switch to the first tab, which is not in the group and then make a new
  // tab, make sure it is at the end of the tab strip and it is not in the
  // group.
  controller()->SelectTab(0, dummy_event_);
  controller()->CreateNewTab();

  EXPECT_EQ(tab_strip_model()->count(), 6);
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(0));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(4));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(5));
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripControllerTestAddTabActiveGroupDisabled,
                       AddTabsWithActiveTabGroupFeatureDisabled) {
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  EXPECT_EQ(tab_strip_model()->count(), 4);

  tab_groups::TabGroupId group_id = tab_strip_model()->AddToNewGroup({1, 2});

  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));

  // Select a tab in the group.
  controller()->SelectTab(1, dummy_event_);
  ASSERT_TRUE(tabstrip()->tab_at(1)->IsActive());
  controller()->CreateNewTab();

  // Create a new tab, it should not have been added to the group even
  // though a tab in the group is selected.
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(0));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(4));
}

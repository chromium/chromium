// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/run_until.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view_utils.h"

class VerticalTabStripLinkDragTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  void EnsureTabCount(int count) {
    while (tab_strip_model()->count() < count) {
      AppendTab();
    }
    while (tab_strip_model()->count() > count) {
      tab_strip_model()->DetachAndDeleteWebContentsAt(0);
    }
    ASSERT_EQ(tab_strip_model()->count(), count);
    RunScheduledLayouts();
  }

  VerticalTabStripRegionView* region_view() {
    return browser()
        ->GetBrowserView()
        .vertical_tab_strip_region_view_for_testing();
  }

  VerticalUnpinnedTabContainerView* unpinned_container() {
    return region_view()->GetUnpinnedTabsContainer();
  }

  VerticalPinnedTabContainerView* pinned_container() {
    return region_view()->GetPinnedTabsContainer();
  }

  std::optional<BrowserRootView::DropIndex> GetDropIndexAt(
      views::View* view,
      gfx::Point loc_in_view) {
    gfx::Point loc_in_region = loc_in_view;
    views::View::ConvertPointToTarget(view, region_view(), &loc_in_region);
    ui::OSExchangeData data;
    ui::DropTargetEvent event(data, gfx::PointF(loc_in_region),
                              gfx::PointF(loc_in_region),
                              ui::DragDropTypes::DRAG_COPY);
    return region_view()->GetDropIndex(event);
  }

  std::vector<VerticalTabView*> WaitForTabs(size_t count) {
    std::vector<VerticalTabView*> tab_views;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      tab_views.clear();
      for (views::View* child : unpinned_container()->children()) {
        if (auto* tab_view = views::AsViewClass<VerticalTabView>(child)) {
          tab_views.push_back(tab_view);
        }
      }
      return tab_views.size() == count &&
             (count == 0 || tab_views[0]->height() > 20);
    }));
    return tab_views;
  }

  std::vector<VerticalTabView*> WaitForPinnedTabs(size_t count) {
    std::vector<VerticalTabView*> tab_views;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      tab_views.clear();
      for (views::View* child : pinned_container()->children()) {
        if (auto* tab_view = views::AsViewClass<VerticalTabView>(child)) {
          tab_views.push_back(tab_view);
        }
      }
      return tab_views.size() == count &&
             (count == 0 || tab_views[0]->height() > 20);
    }));
    return tab_views;
  }

  VerticalSplitTabView* WaitForSplitView() {
    VerticalSplitTabView* split_view = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      for (views::View* child : unpinned_container()->children()) {
        if (auto* v = views::AsViewClass<VerticalSplitTabView>(child)) {
          split_view = v;
          return v->height() > 20;
        }
      }
      return false;
    }));
    return split_view;
  }

  VerticalTabGroupView* WaitForGroupView(bool collapsed = false) {
    VerticalTabGroupView* group_view = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      for (views::View* child : unpinned_container()->children()) {
        if (auto* v = views::AsViewClass<VerticalTabGroupView>(child)) {
          if (v->IsCollapsed() == collapsed &&
              v->group_header()->height() >= 26 &&
              unpinned_container()->height() > 40) {
            group_view = v;
            return true;
          }
        }
      }
      return false;
    }));
    return group_view;
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripLinkDragTest, DropBeforeAndAfterTabs) {
  EnsureTabCount(3);
  auto tab_views = WaitForTabs(3);

  // Drop at the top of the first tab.
  {
    gfx::Point location(tab_views[0]->width() / 2, 2);
    auto drop_index = GetDropIndexAt(tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
  }

  // Drop at the bottom of the first tab.
  {
    gfx::Point location(tab_views[0]->width() / 2, tab_views[0]->height() - 2);
    auto drop_index = GetDropIndexAt(tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
  }

  // Drop at the bottom of the last tab.
  {
    gfx::Point location(tab_views[2]->width() / 2, tab_views[2]->height() - 2);
    auto drop_index = GetDropIndexAt(tab_views[2], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 3);
  }
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripLinkDragTest, DropInMiddleToReplace) {
  EnsureTabCount(3);
  auto tab_views = WaitForTabs(3);

  // Drop in the middle of the first tab.
  {
    gfx::Point location(tab_views[0]->width() / 2, tab_views[0]->height() / 2);
    auto drop_index = GetDropIndexAt(tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }

  // Drop in the middle of the second tab.
  {
    gfx::Point location(tab_views[1]->width() / 2, tab_views[1]->height() / 2);
    auto drop_index = GetDropIndexAt(tab_views[1], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripLinkDragTest, DropInSplitTabs) {
  AppendSplitTab();
  while (tab_strip_model()->count() > 2) {
    tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  }
  ASSERT_EQ(tab_strip_model()->count(), 2);
  RunScheduledLayouts();

  auto* split_view = WaitForSplitView();

  // Drop at the top of the split tab -> before split.
  {
    gfx::Point location(split_view->width() / 2, 2);
    auto drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex);
  }

  // Drop at the bottom of the split tab -> after split.
  {
    gfx::Point location(split_view->width() / 2, split_view->height() - 2);
    auto drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex);
  }

  // Drop in the middle of the first tab in the split -> replace tab 0.
  {
    gfx::Point location(split_view->width() / 4, split_view->height() / 2);
    auto drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }

  // Drop in the middle of the second tab in the split -> replace tab 1.
  {
    gfx::Point location(3 * split_view->width() / 4, split_view->height() / 2);
    auto drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }

  // Drop in the middle of the split view, but between tabs -> before/after
  // split depending on vertical position.
  {
    // Middle vertically, but very left edge of the first tab.
    gfx::Point location(2, split_view->height() / 2 - 1);
    auto drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex);

    location.set_y(split_view->height() / 2 + 1);
    drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex);
  }
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripLinkDragTest, DropInGroups) {
  EnsureTabCount(3);
  auto tab_views = WaitForTabs(3);
  tab_strip_model()->AddToNewGroup({0, 1});
  RunScheduledLayouts();

  auto* group_view = WaitForGroupView();

  // Drop at the top of the group header -> before group.
  {
    gfx::Point location(group_view->group_header()->width() / 2, 2);
    auto drop_index = GetDropIndexAt(group_view->group_header(), location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->group_inclusion,
              BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup);
  }

  // Drop at the bottom of the group header -> inside group at start.
  {
    gfx::Point location(group_view->group_header()->width() / 2,
                        group_view->group_header()->height() - 2);
    auto drop_index = GetDropIndexAt(group_view->group_header(), location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->group_inclusion,
              BrowserRootView::DropIndex::GroupInclusion::kIncludeInGroup);
  }

  // Drop at the bottom of the last tab in the group -> ungrouped.
  {
    gfx::Point location(tab_views[1]->width() / 2, tab_views[1]->height() - 2);
    auto drop_index = GetDropIndexAt(tab_views[1], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
    EXPECT_EQ(drop_index->group_inclusion,
              BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup);
  }
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripLinkDragTest, DropInCollapsedGroups) {
  EnsureTabCount(3);
  tab_strip_model()->ActivateTabAt(
      2, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_groups::TabGroupId group_id = tab_strip_model()->AddToNewGroup({0, 1});
  const TabGroup* group =
      tab_strip_model()->group_model()->GetTabGroup(group_id);
  vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  RunScheduledLayouts();

  auto* group_view = WaitForGroupView(/*collapsed=*/true);

  // Drop at the top of the collapsed group header -> before group.
  {
    gfx::Point location(group_view->group_header()->width() / 2, 2);
    auto drop_index = GetDropIndexAt(group_view->group_header(), location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
  }

  // Drop at the bottom of the collapsed group header -> after group.
  {
    gfx::Point location(group_view->group_header()->width() / 2,
                        group_view->group_header()->height() - 2);
    auto drop_index = GetDropIndexAt(group_view->group_header(), location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
  }
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripLinkDragTest, DropInPinnedTabs) {
  tab_strip_model()->AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      /*foreground=*/true);
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  RunScheduledLayouts();

  auto pinned_tab_views = WaitForPinnedTabs(2);

  // Drop at the left of the first pinned tab.
  {
    gfx::Point location(2, pinned_tab_views[0]->height() / 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
  }

  // Drop at the right of the first pinned tab.
  {
    gfx::Point location(pinned_tab_views[0]->width() - 2,
                        pinned_tab_views[0]->height() / 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
  }

  // Drop at the right of the last pinned tab.
  {
    gfx::Point location(pinned_tab_views[1]->width() - 2,
                        pinned_tab_views[1]->height() / 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[1], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
  }

  // Drop in the middle of the first pinned tab -> replace.
  {
    gfx::Point location(pinned_tab_views[0]->width() / 2,
                        pinned_tab_views[0]->height() / 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripLinkDragTest,
                       DropInPinnedTabsCollapsed) {
  tab_strip_model()->AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      /*foreground=*/true);
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  region_view()->OnResize(-1000, true);
  RunScheduledLayouts();

  auto pinned_tab_views = WaitForPinnedTabs(2);

  // Drop at the top of the first pinned tab.
  {
    gfx::Point location(pinned_tab_views[0]->width() / 2, 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
  }

  // Drop at the bottom of the first pinned tab.
  {
    gfx::Point location(pinned_tab_views[0]->width() / 2,
                        pinned_tab_views[0]->height() - 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
  }
}

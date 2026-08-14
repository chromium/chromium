// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_bottom_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view_utils.h"

class TabStripLinkDragTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{tabs::kVerticalTabs, {}},
            {tabs::kVerticalTabsExpandOnHover, {}},
            {tabs::kTabStripUnification, {}}};
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin<InProcessBrowserTest>::SetUpOnMainThread();
    Tab::SetShowHoverCardOnMouseHoverForTesting(false);

    if (GetParam() == TabStripOrientation::kHorizontal) {
      ExitVerticalTabsMode();
    } else {
      EnterVerticalTabsMode();
    }
  }

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

  BaseTabStripRegionView* region_view() {
    return views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  UnpinnedTabContainerView* unpinned_container() {
    return region_view()->GetUnpinnedTabsContainer();
  }

  PinnedTabContainerView* pinned_container() {
    return region_view()->GetPinnedTabsContainer();
  }

  std::optional<BrowserRootView::DropIndex> GetDropIndexAt(
      views::View* view,
      gfx::Point loc_in_view) {
    const gfx::Point loc_in_region =
        views::View::ConvertPointToTarget(view, region_view(), loc_in_view);
    ui::OSExchangeData data;
    ui::DropTargetEvent event(data, gfx::PointF(loc_in_region),
                              gfx::PointF(loc_in_region),
                              ui::DragDropTypes::DRAG_COPY);
    return region_view()->GetDropIndex(event);
  }

  gfx::Point GetStartEdgeLocation(views::View* view) {
    return GetParam() == TabStripOrientation::kHorizontal
               ? gfx::Point(2, view->height() / 2)
               : gfx::Point(view->width() / 2, 2);
  }

  gfx::Point GetEndEdgeLocation(views::View* view) {
    return GetParam() == TabStripOrientation::kHorizontal
               ? gfx::Point(view->width() - 2, view->height() / 2)
               : gfx::Point(view->width() / 2, view->height() - 2);
  }

  gfx::Point GetCenterLocation(views::View* view) {
    return gfx::Point(view->width() / 2, view->height() / 2);
  }

  std::vector<TabView*> WaitForTabs(size_t count) {
    std::vector<TabView*> tab_views;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      tab_views.clear();
      for (views::View* child : unpinned_container()->children()) {
        if (auto* tab_view = views::AsViewClass<TabView>(child)) {
          tab_views.push_back(tab_view);
        }
      }
      return tab_views.size() == count &&
             (count == 0 || tab_views[0]->height() > 20);
    }));
    return tab_views;
  }

  std::vector<TabView*> WaitForPinnedTabs(size_t count) {
    std::vector<TabView*> tab_views;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      tab_views.clear();
      for (views::View* child : pinned_container()->children()) {
        if (auto* tab_view = views::AsViewClass<TabView>(child)) {
          tab_views.push_back(tab_view);
        }
      }
      return tab_views.size() == count &&
             (count == 0 || tab_views[0]->height() > 20);
    }));
    return tab_views;
  }

  SplitTabView* WaitForSplitView() {
    SplitTabView* split_view = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      for (views::View* child : unpinned_container()->children()) {
        if (auto* v = views::AsViewClass<SplitTabView>(child)) {
          split_view = v;
          return v->height() > 20;
        }
      }
      return false;
    }));
    return split_view;
  }

  TabGroupView* WaitForGroupView(bool collapsed = false) {
    TabGroupView* group_view = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      for (views::View* child : unpinned_container()->children()) {
        if (auto* v = views::AsViewClass<TabGroupView>(child)) {
          if (v->IsCollapsed() == collapsed &&
              v->group_header()->height() >= 20 &&
              unpinned_container()->height() > 20) {
            group_view = v;
            return true;
          }
        }
      }
      return false;
    }));
    return group_view;
  }

 private:
  gfx::AnimationTestApi::RenderModeResetter disable_animation_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripLinkDragTest,
                         testing::Values(TabStripOrientation::kHorizontal,
                                         TabStripOrientation::kVertical));

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, DropBeforeAndAfterTabs) {
  EnsureTabCount(3);
  auto tab_views = WaitForTabs(3);

  // Drop at start edge of first tab.
  {
    auto drop_index =
        GetDropIndexAt(tab_views[0], GetStartEdgeLocation(tab_views[0]));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
  }

  // Drop at end edge of first tab.
  {
    auto drop_index =
        GetDropIndexAt(tab_views[0], GetEndEdgeLocation(tab_views[0]));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
  }

  // Drop at end edge of last tab.
  {
    auto drop_index =
        GetDropIndexAt(tab_views[2], GetEndEdgeLocation(tab_views[2]));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 3);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, DropInMiddleToReplace) {
  EnsureTabCount(3);
  auto tab_views = WaitForTabs(3);

  // Drop in the middle of first tab.
  {
    auto drop_index =
        GetDropIndexAt(tab_views[0], GetCenterLocation(tab_views[0]));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }

  // Drop in the middle of second tab.
  {
    auto drop_index =
        GetDropIndexAt(tab_views[1], GetCenterLocation(tab_views[1]));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, DropInSplitTabs) {
  AppendSplitTab();
  while (tab_strip_model()->count() > 2) {
    tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  }
  ASSERT_EQ(tab_strip_model()->count(), 2);
  RunScheduledLayouts();

  auto* split_view = WaitForSplitView();

  // Drop at start edge of split tab -> before split.
  {
    auto drop_index =
        GetDropIndexAt(split_view, GetStartEdgeLocation(split_view));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex);
  }

  // Drop at end edge of split tab -> after split.
  {
    auto drop_index =
        GetDropIndexAt(split_view, GetEndEdgeLocation(split_view));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex);
  }

  // Drop in the middle of the first tab in split -> replace tab 0.
  {
    gfx::Point location(split_view->width() / 4, split_view->height() / 2);
    auto drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }

  // Drop in the middle of the second tab in split -> replace tab 1.
  {
    gfx::Point location(3 * split_view->width() / 4, split_view->height() / 2);
    auto drop_index = GetDropIndexAt(split_view, location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, DropInGroups) {
  EnsureTabCount(3);
  auto tab_views = WaitForTabs(3);
  tab_strip_model()->AddToNewGroup({0, 1});
  RunScheduledLayouts();

  auto* group_view = WaitForGroupView();

  // Drop at start edge of group header -> before group.
  {
    auto drop_index =
        GetDropIndexAt(group_view->group_header(),
                       GetStartEdgeLocation(group_view->group_header()));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->group_inclusion,
              BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup);
  }

  // Drop at end edge of group header -> inside group at start.
  {
    auto drop_index =
        GetDropIndexAt(group_view->group_header(),
                       GetEndEdgeLocation(group_view->group_header()));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->group_inclusion,
              BrowserRootView::DropIndex::GroupInclusion::kIncludeInGroup);
  }

  // Drop at end edge of last tab in group -> ungrouped.
  {
    auto drop_index =
        GetDropIndexAt(tab_views[1], GetEndEdgeLocation(tab_views[1]));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
    EXPECT_EQ(drop_index->group_inclusion,
              BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, DropInCollapsedGroups) {
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

  // Drop at start edge of collapsed group header -> before group.
  {
    auto drop_index =
        GetDropIndexAt(group_view->group_header(),
                       GetStartEdgeLocation(group_view->group_header()));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
  }

  // Drop at end edge of collapsed group header -> after group.
  {
    auto drop_index =
        GetDropIndexAt(group_view->group_header(),
                       GetEndEdgeLocation(group_view->group_header()));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 2);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, DropInPinnedTabs) {
  tab_strip_model()->AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile())),
      /*foreground=*/true);
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  RunScheduledLayouts();

  auto pinned_tab_views = WaitForPinnedTabs(2);

  // Drop at left edge of first pinned tab.
  {
    gfx::Point location(2, pinned_tab_views[0]->height() / 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
  }

  // Drop at right edge of first pinned tab.
  {
    gfx::Point location(pinned_tab_views[0]->width() - 2,
                        pinned_tab_views[0]->height() / 2);
    auto drop_index = GetDropIndexAt(pinned_tab_views[0], location);
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 1);
  }

  // Drop in the middle of first pinned tab -> replace.
  {
    auto drop_index = GetDropIndexAt(pinned_tab_views[0],
                                     GetCenterLocation(pinned_tab_views[0]));
    ASSERT_TRUE(drop_index.has_value());
    EXPECT_EQ(drop_index->index, 0);
    EXPECT_EQ(drop_index->relative_to_index,
              BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, GetLinkDropBoundsNoShift) {
  AppendTab();
  AppendTab();

  display::Screen* screen = display::Screen::Get();
  gfx::Rect display_bounds =
      screen->GetDisplayNearestView(region_view()->GetWidget()->GetNativeView())
          .bounds();
  DropArrow::MaybeAdjustDisplayBounds(display_bounds);

  browser()->GetWindow()->SetBounds(
      gfx::Rect(display_bounds.x() + 100, display_bounds.y() + 100, 800, 600));

  BrowserRootView::DropIndex index;
  index.index = 0;
  index.relative_to_index =
      BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex;

  DropArrow::Direction direction;
  gfx::Rect bounds =
      region_view()->GetLinkDropBoundsForTesting(index, &direction);
  EXPECT_FALSE(bounds.IsEmpty());

  if (GetParam() == TabStripOrientation::kHorizontal) {
    EXPECT_EQ(direction, DropArrow::Direction::kDown);
  } else {
    EXPECT_EQ(direction, DropArrow::Direction::kRight);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, GetLinkDropBoundsReplaceTab) {
  AppendTab();
  AppendTab();

  display::Screen* screen = display::Screen::Get();
  gfx::Rect display_bounds =
      screen->GetDisplayNearestView(region_view()->GetWidget()->GetNativeView())
          .bounds();
  DropArrow::MaybeAdjustDisplayBounds(display_bounds);

  browser()->GetWindow()->SetBounds(
      gfx::Rect(display_bounds.x() + 100, display_bounds.y() + 100, 800, 600));

  BrowserRootView::DropIndex index;
  index.index = 0;
  index.relative_to_index =
      BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex;

  DropArrow::Direction direction;
  gfx::Rect bounds =
      region_view()->GetLinkDropBoundsForTesting(index, &direction);
  EXPECT_FALSE(bounds.IsEmpty());

  if (GetParam() == TabStripOrientation::kHorizontal) {
    EXPECT_EQ(direction, DropArrow::Direction::kDown);
  } else {
    EXPECT_EQ(direction, DropArrow::Direction::kRight);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, DropInRegionViewOutsideTabStrip) {
  if (GetParam() != TabStripOrientation::kVertical) {
    return;
  }
  EnsureTabCount(3);
  RunScheduledLayouts();

  auto* v_region_view =
      views::AsViewClass<VerticalTabStripRegionView>(region_view());
  ASSERT_TRUE(v_region_view);

  {
    auto* top_container = v_region_view->GetTopContainer();
    const gfx::Point loc_in_top(top_container->width() / 2,
                                top_container->height() / 2);
    const gfx::Point loc_in_region = views::View::ConvertPointToTarget(
        top_container, region_view(), loc_in_top);
    EXPECT_EQ(region_view()->GetDropTarget(loc_in_region), region_view());
    EXPECT_EQ(GetDropIndexAt(top_container, loc_in_top)->index, 0);
  }

  {
    auto* bottom_container = v_region_view->GetBottomContainer();
    const gfx::Point loc_in_bottom(bottom_container->width() / 2,
                                   bottom_container->height() / 2);
    const gfx::Point loc_in_region = views::View::ConvertPointToTarget(
        bottom_container, region_view(), loc_in_bottom);
    EXPECT_EQ(region_view()->GetDropTarget(loc_in_region), region_view());
    EXPECT_EQ(GetDropIndexAt(bottom_container, loc_in_bottom)->index, 3);
  }
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragTest, ExpandOnHoverOnLinkDrag) {
  if (GetParam() != TabStripOrientation::kVertical) {
    return;
  }
  browser()->GetWindow()->Show();
  browser()->GetWindow()->Activate();

  auto* v_region_view =
      views::AsViewClass<VerticalTabStripRegionView>(region_view());
  ASSERT_TRUE(v_region_view);

  tabs::VerticalTabStripStateController::From(browser())
      ->SetExpandOnHoverEnabled(true);
  tabs::VerticalTabStripStateController::From(browser())->RequestCollapse(true);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tabs::VerticalTabStripStateController::From(browser())
        ->IsCollapsed();
  }));

  auto lock =
      v_region_view->GetExpandOnHoverLock(ExpandOnHoverLockType::kKeepExpanded);
  ASSERT_TRUE(v_region_view->is_expanded_on_hover());

  BrowserRootView::DropIndex index;
  index.index = 0;
  index.relative_to_index =
      BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex;
  v_region_view->HandleDragUpdate(index);

  lock.reset();
  ASSERT_TRUE(v_region_view->is_expanded_on_hover());

  v_region_view->HandleDragExited();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !v_region_view->is_expanded_on_hover(); }));
}

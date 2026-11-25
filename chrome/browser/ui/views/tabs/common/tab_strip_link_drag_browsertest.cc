// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
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
    return {{tabs::kTabStripUnification, {}}};
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
               ? gfx::Point(view->GetMirroredXInView(2), view->height() / 2)
               : gfx::Point(view->width() / 2, 2);
  }

  gfx::Point GetEndEdgeLocation(views::View* view) {
    return GetParam() == TabStripOrientation::kHorizontal
               ? gfx::Point(view->GetMirroredXInView(view->width() - 2),
                            view->height() / 2)
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
    return WaitForSplitView(*unpinned_container());
  }

  SplitTabView* WaitForSplitView(views::View& container) {
    SplitTabView* split_view = nullptr;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      for (views::View* child : container.children()) {
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

  // Pinned tabs are laid out horizontally in both orientations (as a grid when
  // the vertical tab strip is expanded), so the drop index is determined by the
  // x-coordinate regardless of GetParam().

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

class TabStripLinkDragRTLTest : public TabStripLinkDragTest {
 public:
  using DropIndex = BrowserRootView::DropIndex;
  using enum DropIndex::GroupInclusion;
  using enum DropIndex::RelativeToIndex;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    TabStripLinkDragTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                    switches::kForceDirectionRTL);
    command_line->AppendSwitchASCII(switches::kForceTextDirection,
                                    switches::kForceDirectionRTL);
  }

  gfx::Point GetSplitQuarterLocation(const SplitTabView* split_view,
                                     int quarter) const {
    return gfx::Point(quarter * split_view->width() / 4,
                      split_view->height() / 2);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripLinkDragRTLTest,
                         testing::Values(TabStripOrientation::kHorizontal,
                                         TabStripOrientation::kVertical));

IN_PROC_BROWSER_TEST_P(TabStripLinkDragRTLTest, DropInPinnedAndUnpinnedTabs) {
  EnsureTabCount(4);
  tab_strip_model()->SetTabPinned(0, true);
  tab_strip_model()->SetTabPinned(1, true);
  RunScheduledLayouts();
  auto pinned_views = WaitForPinnedTabs(2);
  auto unpinned_views = WaitForTabs(2);
  ASSERT_EQ(pinned_views.size(), 2u);
  ASSERT_EQ(unpinned_views.size(), 2u);
  const bool is_horizontal = GetParam() == TabStripOrientation::kHorizontal;
  const gfx::Point before_pinned_loc =
      is_horizontal ? gfx::Point(pinned_container()->width() + 5,
                                 pinned_container()->height() / 2)
                    : gfx::Point(pinned_container()->width() / 2, -5);
  const gfx::Point unpinned_start_loc =
      is_horizontal ? gfx::Point(unpinned_views[0]->GetMirroredXInView(
                                     unpinned_views[0]->width() / 8),
                                 unpinned_views[0]->height() / 2)
                    : GetStartEdgeLocation(unpinned_views[0]);
  const gfx::Point after_unpinned_loc =
      is_horizontal ? gfx::Point(-10, unpinned_container()->height() / 2)
                    : gfx::Point(unpinned_container()->width() / 2,
                                 unpinned_container()->height() + 10);

  auto drop_before_pinned =
      GetDropIndexAt(pinned_container(), before_pinned_loc);
  auto drop_pinned_replace =
      GetDropIndexAt(pinned_views[1], GetCenterLocation(pinned_views[1]));
  auto drop_unpinned_start =
      GetDropIndexAt(unpinned_views[0], unpinned_start_loc);
  auto drop_unpinned_end =
      GetDropIndexAt(unpinned_views[0], GetEndEdgeLocation(unpinned_views[0]));
  auto drop_unpinned_replace =
      GetDropIndexAt(unpinned_views[1], GetCenterLocation(unpinned_views[1]));
  auto drop_after_unpinned =
      GetDropIndexAt(unpinned_container(), after_unpinned_loc);

  EXPECT_EQ(drop_before_pinned, (DropIndex{0, kInsertBeforeIndex}));
  EXPECT_EQ(drop_pinned_replace, (DropIndex{1, kReplaceIndex}));
  EXPECT_EQ(drop_unpinned_start, (DropIndex{2, kInsertBeforeIndex}));
  EXPECT_EQ(drop_unpinned_end, (DropIndex{3, kInsertBeforeIndex}));
  EXPECT_EQ(drop_unpinned_replace, (DropIndex{3, kReplaceIndex}));
  EXPECT_EQ(drop_after_unpinned, (DropIndex{4, kInsertBeforeIndex}));
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragRTLTest, DropInGroups) {
  EnsureTabCount(3);
  auto tab_views = WaitForTabs(3);
  ASSERT_EQ(tab_views.size(), 3u);
  tab_strip_model()->AddToNewGroup({0, 1});
  RunScheduledLayouts();
  auto* group_view = WaitForGroupView();
  ASSERT_TRUE(group_view);
  auto* header = group_view->group_header();

  auto drop_header_start = GetDropIndexAt(header, GetStartEdgeLocation(header));
  auto drop_header_end = GetDropIndexAt(header, GetEndEdgeLocation(header));
  auto drop_first_end =
      GetDropIndexAt(tab_views[0], GetEndEdgeLocation(tab_views[0]));
  auto drop_last_end =
      GetDropIndexAt(tab_views[1], GetEndEdgeLocation(tab_views[1]));

  EXPECT_EQ(drop_header_start,
            (DropIndex{0, kInsertBeforeIndex, kDontIncludeInGroup}));
  EXPECT_EQ(drop_header_end,
            (DropIndex{0, kInsertBeforeIndex, kIncludeInGroup}));
  EXPECT_EQ(drop_first_end,
            (DropIndex{1, kInsertBeforeIndex, kIncludeInGroup}));
  EXPECT_EQ(drop_last_end,
            (DropIndex{2, kInsertBeforeIndex, kDontIncludeInGroup}));
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragRTLTest, DropInCollapsedGroups) {
  EnsureTabCount(3);
  tab_strip_model()->ActivateTabAt(
      2, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  const TabGroup* group = tab_strip_model()->group_model()->GetTabGroup(
      tab_strip_model()->AddToNewGroup({0, 1}));
  vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  RunScheduledLayouts();
  auto* collapsed_group = WaitForGroupView(/*collapsed=*/true);
  ASSERT_TRUE(collapsed_group);
  auto* header = collapsed_group->group_header();

  auto drop_start = GetDropIndexAt(header, GetStartEdgeLocation(header));
  auto drop_end = GetDropIndexAt(header, GetEndEdgeLocation(header));

  EXPECT_EQ(drop_start,
            (DropIndex{0, kInsertBeforeIndex, kDontIncludeInGroup}));
  EXPECT_EQ(drop_end, (DropIndex{2, kInsertBeforeIndex, kDontIncludeInGroup}));
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragRTLTest, DropInSplitTabs) {
  EnsureTabCount(1);
  AppendSplitTab();
  ASSERT_EQ(tab_strip_model()->count(), 3);
  RunScheduledLayouts();
  auto* split = WaitForSplitView();
  ASSERT_TRUE(split);

  auto drop_start = GetDropIndexAt(split, GetStartEdgeLocation(split));
  auto drop_end = GetDropIndexAt(split, GetEndEdgeLocation(split));
  auto drop_first = GetDropIndexAt(split, GetSplitQuarterLocation(split, 3));
  auto drop_second = GetDropIndexAt(split, GetSplitQuarterLocation(split, 1));

  EXPECT_EQ(drop_start, (DropIndex{1, kInsertBeforeIndex}));
  EXPECT_EQ(drop_end, (DropIndex{3, kInsertBeforeIndex}));
  EXPECT_EQ(drop_first, (DropIndex{1, kReplaceIndex}));
  EXPECT_EQ(drop_second, (DropIndex{2, kReplaceIndex}));
}

IN_PROC_BROWSER_TEST_P(TabStripLinkDragRTLTest, DropInGroupedSplitTabs) {
  EnsureTabCount(1);
  AppendSplitTab();
  ASSERT_EQ(tab_strip_model()->count(), 3);
  tab_strip_model()->AddToNewGroup({1, 2});
  RunScheduledLayouts();
  auto* group_view = WaitForGroupView();
  ASSERT_TRUE(group_view);
  auto* split = WaitForSplitView(*group_view);
  ASSERT_TRUE(split);

  auto drop_start = GetDropIndexAt(split, GetStartEdgeLocation(split));
  auto drop_end = GetDropIndexAt(split, GetEndEdgeLocation(split));
  auto drop_first = GetDropIndexAt(split, GetSplitQuarterLocation(split, 3));
  auto drop_second = GetDropIndexAt(split, GetSplitQuarterLocation(split, 1));

  EXPECT_EQ(drop_start, (DropIndex{1, kInsertBeforeIndex}));
  EXPECT_EQ(drop_end, (DropIndex{3, kInsertBeforeIndex}));
  EXPECT_EQ(drop_first, (DropIndex{1, kReplaceIndex}));
  EXPECT_EQ(drop_second, (DropIndex{2, kReplaceIndex}));
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"

#include "base/check.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/view_utils.h"

class SplitTabViewTestBase
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsBrowserTestMixin<
        InProcessBrowserTest>::GetEnabledFeatures();
    enabled.push_back({tabs::kTabStripUnification, {}});
    return enabled;
  }

 protected:
  SplitTabView* CreateActivePinnedSplitTab() {
    AppendPinnedTab();
    AppendPinnedTab();
    tab_strip_model()->ActivateTabAt(0);
    tab_strip_model()->AddToNewSplit(
        {1}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
    RunScheduledLayouts();
    TabCollectionNode* split_node =
        pinned_collection_node()->GetChildNodeOfType(
            TabCollectionNode::Type::SPLIT);
    return split_node ? views::AsViewClass<SplitTabView>(split_node->view())
                      : nullptr;
  }

  SkBitmap PaintView(views::View* view) {
    CHECK(view);
    CHECK(!view->size().IsEmpty());
    CHECK(view->layer() || view->GetMirroredPosition().IsOrigin());
    SkBitmap bitmap;
    {
      ui::CanvasPainter painter(&bitmap, view->size(), 1.0f,
                                SK_ColorTRANSPARENT, true);
      view->Paint(views::PaintInfo::CreateRootPaintInfo(painter.context(),
                                                        view->size()));
    }
    return bitmap;
  }
};

class SplitTabViewTest
    : public SplitTabViewTestBase,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  TabStripOrientation orientation() const { return GetParam(); }
  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  void SetUpOnMainThread() override {
    SplitTabViewTestBase::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }
};

IN_PROC_BROWSER_TEST_P(SplitTabViewTest, SplitTabsInGroup_LineViewPaint) {
  // Create a split tab (2 tabs) and an additional tab grouped together.
  AppendSplitTab();
  AppendTab();

  TabStripModel* model = tab_strip_model();
  model->AddToNewGroup({1, 2, 3});

  auto* unpinned_node = unpinned_collection_node();
  auto* group_node =
      unpinned_node->GetChildNodeOfType(TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_TRUE(group_view);
  ASSERT_TRUE(group_view->group_line());

  SkBitmap bitmap;
  const gfx::Size size = group_view->group_line()->size();
  ui::CanvasPainter canvas_painter(&bitmap, size, 1.0f, SK_ColorTRANSPARENT,
                                   false);

  model->ActivateTabAt(1, TabStripUserGestureDetails(
                              TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();
  EXPECT_EQ(model->active_index(), 1);
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  group_view->group_line()->Paint(views::PaintInfo::CreateRootPaintInfo(
      canvas_painter.context(), group_view->group_line()->size()));

  model->ActivateTabAt(2, TabStripUserGestureDetails(
                              TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();
  EXPECT_EQ(model->active_index(), 2);
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  group_view->group_line()->Paint(views::PaintInfo::CreateRootPaintInfo(
      canvas_painter.context(), group_view->group_line()->size()));

  model->ActivateTabAt(3, TabStripUserGestureDetails(
                              TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();
  EXPECT_EQ(model->active_index(), 3);
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  group_view->group_line()->Paint(views::PaintInfo::CreateRootPaintInfo(
      canvas_painter.context(), group_view->group_line()->size()));
}

IN_PROC_BROWSER_TEST_P(SplitTabViewTest, CreateSplitWithCollapsedTabGroup) {
  // Append two tabs that will form the split, then group and collapse the
  // initial tab.
  AppendTab();
  AppendTab();
  tab_strip_model()->AddToNewGroup({0});
  auto* group_node = unpinned_collection_node()->GetChildNodeOfType(
      TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_TRUE(group_view);
  group_view->ToggleCollapsedState(
      ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  RunScheduledLayouts();
  ASSERT_TRUE(group_view->IsCollapsed());

  // Create a split tab from the two ungrouped tabs.
  tab_strip_model()->AddToNewSplit(
      {1}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
  RunScheduledLayouts();

  auto* split_node = unpinned_collection_node()->GetChildNodeOfType(
      TabCollectionNode::Type::SPLIT);
  ASSERT_TRUE(split_node);
  auto* split_view = views::AsViewClass<SplitTabView>(split_node->view());
  ASSERT_TRUE(split_view);
  EXPECT_LE(split_view->GetMinimumSize().width(),
            split_view->GetPreferredSize().width());
}

IN_PROC_BROWSER_TEST_P(SplitTabViewTest,
                       AdjacentTabsTargetableInHorizontalOverlapRegion) {
  if (!is_horizontal()) {
    GTEST_SKIP();
  }

  const auto animation_mode_reset =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

  // Tabs layout:
  // Tab 0: Leading unsplit tab
  // Tab 1, 2: Split tab (active so SplitTabView has highest Z-order)
  // Tab 3: Trailing unsplit tab
  AppendSplitTab();
  AppendTab();
  tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  RunScheduledLayouts();

  auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  auto* unpinned_container =
      views::AsViewClass<UnpinnedTabContainerView>(unpinned_node->view());

  auto* leading_tab =
      views::AsViewClass<TabView>(unpinned_node->children()[0]->view());
  auto* split_view =
      views::AsViewClass<SplitTabView>(unpinned_node->children()[1]->view());
  auto* trailing_tab =
      views::AsViewClass<TabView>(unpinned_node->children()[2]->view());

  split_view->GetWidget()->LayoutRootViewIfNecessary();

  auto* leading_split_tab =
      views::AsViewClass<TabView>(split_view->children()[0]);
  auto* trailing_split_tab =
      views::AsViewClass<TabView>(split_view->children()[1]);

  gfx::RectF leading_hit_bounds = gfx::SkRectToRectF(
      leading_split_tab->tab_styling()
          ->GetPath(TabStyle::PathType::kHitTest, /*scale=*/1.0f,
                    {.render_units = TabStyle::RenderUnits::kDips})
          .getBounds());
  views::View::ConvertRectToTarget(leading_split_tab, unpinned_container,
                                   &leading_hit_bounds);
  const int leading_threshold_x = gfx::ToEnclosingRect(leading_hit_bounds).x();

  gfx::RectF trailing_hit_bounds = gfx::SkRectToRectF(
      trailing_split_tab->tab_styling()
          ->GetPath(TabStyle::PathType::kHitTest, /*scale=*/1.0f,
                    {.render_units = TabStyle::RenderUnits::kDips})
          .getBounds());
  views::View::ConvertRectToTarget(trailing_split_tab, unpinned_container,
                                   &trailing_hit_bounds);
  const int trailing_threshold_x =
      gfx::ToEnclosingRect(trailing_hit_bounds).right();

  // Points 1 DIP outside the leading and trailing hit-test thresholds of the
  // split tabs (still within `split_view->bounds()`) should target the adjacent
  // tabs rather than `split_view`.
  const gfx::Point point_in_leading_overlap(leading_threshold_x - 1,
                                            split_view->height() / 2);
  ASSERT_TRUE(split_view->bounds().Contains(point_in_leading_overlap));
  EXPECT_EQ(
      unpinned_container->GetEventHandlerForPoint(point_in_leading_overlap),
      leading_tab);

  const gfx::Point point_in_trailing_overlap(trailing_threshold_x + 1,
                                             split_view->height() / 2);
  ASSERT_TRUE(split_view->bounds().Contains(point_in_trailing_overlap));
  EXPECT_EQ(
      unpinned_container->GetEventHandlerForPoint(point_in_trailing_overlap),
      trailing_tab);

  // Points 1 DIP inside the left and right split tabs' hit-test thresholds
  // should still target the respective child TabViews.
  const gfx::Point point_in_leading_split_tab(leading_threshold_x + 1,
                                              split_view->height() / 2);
  EXPECT_EQ(
      unpinned_container->GetEventHandlerForPoint(point_in_leading_split_tab),
      leading_split_tab);

  const gfx::Point point_in_trailing_split_tab(trailing_threshold_x - 1,
                                               split_view->height() / 2);
  EXPECT_EQ(
      unpinned_container->GetEventHandlerForPoint(point_in_trailing_split_tab),
      trailing_split_tab);
}

IN_PROC_BROWSER_TEST_P(SplitTabViewTest, HoverSynchronizesAcrossSplitTabs) {
  AppendSplitTab();
  RunScheduledLayouts();

  auto* split_node = unpinned_collection_node()->GetChildNodeOfType(
      TabCollectionNode::Type::SPLIT);
  ASSERT_TRUE(split_node);
  auto* split_view = views::AsViewClass<SplitTabView>(split_node->view());
  ASSERT_TRUE(split_view);
  ASSERT_EQ(split_view->children().size(), 2u);

  auto* tab1 = views::AsViewClass<TabView>(split_view->children()[0]);
  auto* tab2 = views::AsViewClass<TabView>(split_view->children()[1]);
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  EXPECT_FALSE(tab1->IsHoverAnimationActive());
  EXPECT_FALSE(tab2->IsHoverAnimationActive());

  ui::MouseEvent enter_event(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  split_view->OnMouseEntered(enter_event);
  EXPECT_TRUE(tab1->IsHoverAnimationActive());
  EXPECT_TRUE(tab2->IsHoverAnimationActive());

  ui::MouseEvent exit_event(ui::EventType::kMouseExited, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(), 0, 0);
  split_view->OnMouseExited(exit_event);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !tab1->IsHoverAnimationActive() && !tab2->IsHoverAnimationActive();
  }));
}

IN_PROC_BROWSER_TEST_P(SplitTabViewTest,
                       RemoveTabFromSplitReparentsRemainingTab) {
  AppendSplitTab();
  RunScheduledLayouts();

  auto* split_node = unpinned_collection_node()->GetChildNodeOfType(
      TabCollectionNode::Type::SPLIT);
  ASSERT_TRUE(split_node);
  std::optional<split_tabs::SplitTabId> split_id =
      tab_strip_model()->GetSplitForTab(1);
  ASSERT_TRUE(split_id.has_value());

  tab_strip_model()->RemoveSplit(split_id.value());
  RunScheduledLayouts();

  EXPECT_EQ(unpinned_collection_node()->GetChildNodeOfType(
                TabCollectionNode::Type::SPLIT),
            nullptr);
  EXPECT_EQ(tab_strip_model()->count(), 3);
}

IN_PROC_BROWSER_TEST_P(SplitTabViewTest, CreateSplitUKMLogged) {
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder =
      std::make_unique<ukm::TestAutoSetUkmRecorder>();

  // Create two tabs with the first two being split.
  GURL a_url = GURL("https://a.com");
  GURL b_url = GURL("https://b.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  AppendTab();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));
  tab_strip_model()->ActivateTabAt(0);
  tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);

  // Ensure UKM is recorded.
  auto entries = ukm_recorder->GetEntriesByName(
      ukm::builders::SplitView_Created::kEntryName);
  EXPECT_EQ(2u, entries.size());
  ukm_recorder->ExpectEntrySourceHasUrl(entries[0], a_url);
  ukm_recorder->ExpectEntrySourceHasUrl(entries[1], b_url);
  EXPECT_EQ(
      *ukm_recorder->GetEntryMetric(
          entries[0], ukm::builders::SplitView_Created::kSplitEventIdName),
      *ukm_recorder->GetEntryMetric(
          entries[1], ukm::builders::SplitView_Created::kSplitEventIdName));
}

IN_PROC_BROWSER_TEST_P(SplitTabViewTest, SwapTabIntoSplitUKMLogged) {
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder =
      std::make_unique<ukm::TestAutoSetUkmRecorder>();

  // Create three tabs with the first two being split.
  GURL a_url = GURL("https://a.com");
  GURL b_url = GURL("https://b.com");
  GURL c_url = GURL("https://c.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  AppendTab();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));
  AppendTab();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), c_url));
  tab_strip_model()->ActivateTabAt(0);
  tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);

  // Swap the first tab with the last.
  tab_strip_model()->UpdateTabInSplit(
      tab_strip_model()->GetTabAtIndex(0)->GetHandle(),
      tab_strip_model()->GetTabAtIndex(2)->GetHandle(),
      TabStripModel::SplitUpdateType::kSwap);

  // Ensure UKM is recorded.
  auto entries = ukm_recorder->GetEntriesByName(
      ukm::builders::SplitView_Updated::kEntryName);
  EXPECT_EQ(2u, entries.size());
  ukm_recorder->ExpectEntrySourceHasUrl(entries[0], c_url);
  ukm_recorder->ExpectEntrySourceHasUrl(entries[1], b_url);
  EXPECT_EQ(
      *ukm_recorder->GetEntryMetric(
          entries[0], ukm::builders::SplitView_Updated::kSplitEventIdName),
      *ukm_recorder->GetEntryMetric(
          entries[1], ukm::builders::SplitView_Updated::kSplitEventIdName));
}

IN_PROC_BROWSER_TEST_P(SplitTabViewTest, NavigateSplitTabUKMLogged) {
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder =
      std::make_unique<ukm::TestAutoSetUkmRecorder>();

  // Create two tabs with the first two being split.
  GURL a_url = GURL("https://a.com");
  GURL b_url = GURL("https://b.com");
  GURL c_url = GURL("https://c.com");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  AppendTab();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));
  tab_strip_model()->ActivateTabAt(0);
  tab_strip_model()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);

  // Navigate the first tab to a new URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), c_url));

  // Ensure UKM is recorded.
  auto entries = ukm_recorder->GetEntriesByName(
      ukm::builders::SplitView_Updated::kEntryName);
  EXPECT_EQ(2u, entries.size());
  ukm_recorder->ExpectEntrySourceHasUrl(entries[0], c_url);
  ukm_recorder->ExpectEntrySourceHasUrl(entries[1], b_url);
  EXPECT_EQ(
      *ukm_recorder->GetEntryMetric(
          entries[0], ukm::builders::SplitView_Updated::kSplitEventIdName),
      *ukm_recorder->GetEntryMetric(
          entries[1], ukm::builders::SplitView_Updated::kSplitEventIdName));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SplitTabViewTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });

class VerticalSplitTabViewBrowserTest : public SplitTabViewTestBase {};

IN_PROC_BROWSER_TEST_F(VerticalSplitTabViewBrowserTest,
                       ProposedLayout_Unbounded) {
  AppendSplitTab();
  auto* split = unpinned_collection_node()
                    ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                    ->view();
  EXPECT_TRUE(views::IsViewClass<SplitTabView>(split));
  SplitTabView* split_tab_view = views::AsViewClass<SplitTabView>(split);

  auto children = split_tab_view->children();
  EXPECT_EQ(children.size(), 2u);
  auto child1 = children[0];
  auto child2 = children[1];

  auto proposed_layout =
      split_tab_view->CalculateProposedLayout(views::SizeBounds());
  auto* child1_layout = proposed_layout.GetLayoutFor(child1);
  auto* child2_layout = proposed_layout.GetLayoutFor(child2);
  // Expect children to be on the same row.
  EXPECT_EQ(child1_layout->bounds.y(), child2_layout->bounds.y());
  // Expect children to be next to each other with set gap.
  EXPECT_EQ(child1_layout->bounds.right() + SplitTabView::kSplitViewGap,
            child2_layout->bounds.x());
  // Expect total width to just hold the two children
  EXPECT_EQ(proposed_layout.host_size.width(), child2_layout->bounds.right());
  EXPECT_EQ(proposed_layout.host_size.width(),
            child1_layout->bounds.width() + SplitTabView::kSplitViewGap +
                child2_layout->bounds.width());
}

IN_PROC_BROWSER_TEST_F(VerticalSplitTabViewBrowserTest,
                       ProposedLayout_LargeBounds) {
  AppendSplitTab();
  auto* split = unpinned_collection_node()
                    ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                    ->view();
  EXPECT_TRUE(views::IsViewClass<SplitTabView>(split));
  SplitTabView* split_tab_view = views::AsViewClass<SplitTabView>(split);

  auto children = split_tab_view->children();
  EXPECT_EQ(children.size(), 2u);
  auto child1 = children[0];
  auto child2 = children[1];

  // Needs to be larger than 2 * kVerticalTabExpandedMinWidth.
  int available_width = 200;
  auto proposed_layout = split_tab_view->CalculateProposedLayout(
      views::SizeBounds(available_width, {}));
  auto* child1_layout = proposed_layout.GetLayoutFor(child1);
  auto* child2_layout = proposed_layout.GetLayoutFor(child2);
  // Expect children to be on the same row.
  EXPECT_EQ(child1_layout->bounds.y(), child2_layout->bounds.y());
  // Expect children to be next to each other with set gap.
  EXPECT_EQ(child1_layout->bounds.right() + SplitTabView::kSplitViewGap,
            child2_layout->bounds.x());
  // Expect total width to just hold the two children.
  EXPECT_EQ(proposed_layout.host_size.width(), child2_layout->bounds.right());
  EXPECT_EQ(proposed_layout.host_size.width(),
            child1_layout->bounds.width() + SplitTabView::kSplitViewGap +
                child2_layout->bounds.width());
  // Expect children to share total width.
  EXPECT_EQ((available_width - SplitTabView::kSplitViewGap) / 2,
            child1_layout->bounds.width());
  EXPECT_EQ((available_width - SplitTabView::kSplitViewGap) / 2,
            child2_layout->bounds.width());
}

IN_PROC_BROWSER_TEST_F(VerticalSplitTabViewBrowserTest,
                       ProposedLayout_LimitedBounds) {
  AppendSplitTab();
  auto* split = unpinned_collection_node()
                    ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                    ->view();
  EXPECT_TRUE(views::IsViewClass<SplitTabView>(split));
  SplitTabView* split_tab_view = views::AsViewClass<SplitTabView>(split);

  auto children = split_tab_view->children();
  EXPECT_EQ(children.size(), 2u);
  auto child1 = children[0];
  auto child2 = children[1];

  // Needs to be smaller than the minimum size of a split view.
  int available_width = 60;
  auto proposed_layout = split_tab_view->CalculateProposedLayout(
      views::SizeBounds(available_width, {}));
  auto* child1_layout = proposed_layout.GetLayoutFor(child1);
  auto* child2_layout = proposed_layout.GetLayoutFor(child2);
  // Expect children to be on different rows.
  EXPECT_NE(child1_layout->bounds.y(), child2_layout->bounds.y());
  // Expect children to be next to each other vertically with set gap.
  EXPECT_EQ(child1_layout->bounds.bottom() + SplitTabView::kSplitViewGap,
            child2_layout->bounds.y());
  // Expect total height to just hold the two children.
  EXPECT_EQ(proposed_layout.host_size.height(), child2_layout->bounds.bottom());
  EXPECT_EQ(proposed_layout.host_size.height(),
            child1_layout->bounds.height() + SplitTabView::kSplitViewGap +
                child2_layout->bounds.height());
  // Expect children to fill width.
  EXPECT_EQ(available_width, child1_layout->bounds.width());
  EXPECT_EQ(available_width, child2_layout->bounds.width());
}

IN_PROC_BROWSER_TEST_F(VerticalSplitTabViewBrowserTest,
                       PinnedSplitTabPaintsMergedBackgroundAndBorder) {
  SplitTabView* split_view = CreateActivePinnedSplitTab();
  ASSERT_TRUE(split_view);
  ASSERT_EQ(split_view->children().size(), 2u);
  auto* tab_0 = views::AsViewClass<TabView>(split_view->children()[0]);
  ASSERT_TRUE(tab_0);
  ASSERT_TRUE(tab_0->IsActive());
  ASSERT_TRUE(tab_0->GetBackgroundColor().has_value());
  const SkColor active_bg = tab_0->GetBackgroundColor().value();

  const SkBitmap tab_0_bitmap = PaintView(tab_0);
  const SkBitmap split_bitmap = PaintView(split_view);

  EXPECT_NE(split_view->GetBorder(), nullptr);
  EXPECT_EQ(tab_0_bitmap.getColor(2, tab_0->height() / 2), SK_ColorTRANSPARENT);
  EXPECT_EQ(
      split_bitmap.getColor(split_view->width() / 2, split_view->height() / 2),
      active_bg);
}

class HorizontalSplitTabViewBrowserTest : public SplitTabViewTestBase {
 public:
  void SetUpOnMainThread() override {
    SplitTabViewTestBase::SetUpOnMainThread();
    ExitVerticalTabsMode();
  }
};

IN_PROC_BROWSER_TEST_F(HorizontalSplitTabViewBrowserTest,
                       ProposedLayout_Unbounded) {
  AppendSplitTab();
  auto* split = unpinned_collection_node()
                    ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                    ->view();
  ASSERT_TRUE(views::IsViewClass<SplitTabView>(split));
  SplitTabView* split_tab_view = views::AsViewClass<SplitTabView>(split);

  auto children = split_tab_view->children();
  ASSERT_EQ(children.size(), 2u);
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();

  auto proposed_layout =
      split_tab_view->CalculateProposedLayout(views::SizeBounds());
  auto* child1_layout = proposed_layout.GetLayoutFor(children[0]);
  auto* child2_layout = proposed_layout.GetLayoutFor(children[1]);

  EXPECT_EQ(child1_layout->bounds.y(), child2_layout->bounds.y());
  EXPECT_EQ(child1_layout->bounds.right() - tab_overlap,
            child2_layout->bounds.x());
  EXPECT_EQ(proposed_layout.host_size.width(), child2_layout->bounds.right());
  EXPECT_EQ(proposed_layout.host_size.width(),
            child1_layout->bounds.width() + child2_layout->bounds.width() -
                tab_overlap);
  EXPECT_EQ(proposed_layout.host_size.height(),
            TabStyle::Get()->GetStandardHeight());
}

IN_PROC_BROWSER_TEST_F(HorizontalSplitTabViewBrowserTest,
                       ProposedLayout_Bounded) {
  AppendSplitTab();
  auto* split = unpinned_collection_node()
                    ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                    ->view();
  ASSERT_TRUE(views::IsViewClass<SplitTabView>(split));
  SplitTabView* split_tab_view = views::AsViewClass<SplitTabView>(split);

  auto children = split_tab_view->children();
  ASSERT_EQ(children.size(), 2u);
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int available_width = 200;

  auto proposed_layout = split_tab_view->CalculateProposedLayout(
      views::SizeBounds(available_width, {}));
  auto* child1_layout = proposed_layout.GetLayoutFor(children[0]);
  auto* child2_layout = proposed_layout.GetLayoutFor(children[1]);

  EXPECT_EQ(child1_layout->bounds.y(), child2_layout->bounds.y());
  EXPECT_EQ(child1_layout->bounds.right() - tab_overlap,
            child2_layout->bounds.x());
  EXPECT_EQ(proposed_layout.host_size.width(), available_width);
  EXPECT_EQ(child1_layout->bounds.width() + child2_layout->bounds.width() -
                tab_overlap,
            available_width);
}

IN_PROC_BROWSER_TEST_F(HorizontalSplitTabViewBrowserTest, PinnedSplitTabWidth) {
  AppendTab();
  AppendTab();
  tab_strip_model()->SetTabPinned(1, true);
  tab_strip_model()->SetTabPinned(2, true);
  tab_strip_model()->AddToNewSplit(
      {1, 2}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
  RunScheduledLayouts();

  auto* split = pinned_collection_node()
                    ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                    ->view();
  ASSERT_TRUE(views::IsViewClass<SplitTabView>(split));
  SplitTabView* split_tab_view = views::AsViewClass<SplitTabView>(split);

  const int expected_child_width =
      TabStyle::Get()->GetPinnedWidth(/*is_split=*/true);
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  EXPECT_EQ(split_tab_view->GetPreferredSize().width(),
            2 * expected_child_width - tab_overlap);
  ASSERT_EQ(split_tab_view->children().size(), 2u);
  EXPECT_EQ(split_tab_view->children()[0]->width(), expected_child_width);
  EXPECT_EQ(split_tab_view->children()[1]->width(), expected_child_width);
}

IN_PROC_BROWSER_TEST_F(HorizontalSplitTabViewBrowserTest,
                       MinimumAndCrossoverWidth) {
  AppendSplitTab();
  RunScheduledLayouts();

  auto* split = unpinned_collection_node()
                    ->GetChildNodeOfType(TabCollectionNode::Type::SPLIT)
                    ->view();
  ASSERT_TRUE(views::IsViewClass<SplitTabView>(split));
  SplitTabView* split_tab_view = views::AsViewClass<SplitTabView>(split);
  ASSERT_EQ(split_tab_view->children().size(), 2u);

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  const int child_min_width =
      split_tab_view->children()[0]->GetMinimumSize().width();
  EXPECT_EQ(split_tab_view->GetMinimumSize().width(),
            2 * child_min_width - tab_overlap);
  EXPECT_EQ(split_tab_view->GetCrossoverWidth(),
            2 * TabStyle::Get()->GetMinimumActiveWidth(/*is_split=*/true) -
                tab_overlap);
}

class HorizontalSplitTabViewPinnedStylingTest
    : public HorizontalSplitTabViewBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsNewPinnedTabStylingEnabled() const { return GetParam(); }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = HorizontalSplitTabViewBrowserTest::GetEnabledFeatures();
    if (IsNewPinnedTabStylingEnabled()) {
      enabled.push_back({tabs::kNewHorizontalPinnedTabStyling, {}});
    }
    return enabled;
  }

  const std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    auto disabled = HorizontalSplitTabViewBrowserTest::GetDisabledFeatures();
    if (!IsNewPinnedTabStylingEnabled()) {
      disabled.push_back(tabs::kNewHorizontalPinnedTabStyling);
    }
    return disabled;
  }
};

IN_PROC_BROWSER_TEST_P(HorizontalSplitTabViewPinnedStylingTest,
                       ActivePinnedSplitTabPaintsTabSilhouette) {
  SplitTabView* split_view = CreateActivePinnedSplitTab();
  ASSERT_TRUE(split_view);
  ASSERT_EQ(split_view->children().size(), 2u);
  auto* tab_0 = views::AsViewClass<TabView>(split_view->children()[0]);
  ASSERT_TRUE(tab_0);
  ASSERT_TRUE(tab_0->IsActive());
  ASSERT_TRUE(tab_0->GetBackgroundColor().has_value());
  const SkColor active_bg = tab_0->GetBackgroundColor().value();

  const SkBitmap tab_0_bitmap = PaintView(tab_0);
  const SkBitmap split_bitmap = PaintView(split_view);

  EXPECT_EQ(split_view->GetBorder(), nullptr);
  EXPECT_EQ(tab_0_bitmap.getColor(tab_0->width() / 2, tab_0->height() - 2),
            active_bg);
  EXPECT_EQ(split_bitmap.getColor(split_view->width() / 2, 2),
            SK_ColorTRANSPARENT);
  EXPECT_EQ(split_bitmap.getColor(4, split_view->height() / 2),
            SK_ColorTRANSPARENT);
  EXPECT_EQ(split_bitmap.getColor(4, split_view->height() - 1), active_bg);
  EXPECT_EQ(
      split_bitmap.getColor(split_view->width() - 4, split_view->height() - 1),
      active_bg);
}

IN_PROC_BROWSER_TEST_P(HorizontalSplitTabViewPinnedStylingTest,
                       InactivePinnedSplitTabOmitsVerticalBorder) {
  SplitTabView* split_view = CreateActivePinnedSplitTab();
  ASSERT_TRUE(split_view);
  ASSERT_EQ(split_view->children().size(), 2u);
  auto* tab_0 = views::AsViewClass<TabView>(split_view->children()[0]);
  ASSERT_TRUE(tab_0);
  ASSERT_TRUE(tab_0->IsActive());

  tab_strip_model()->ActivateTabAt(2);
  RunScheduledLayouts();
  const SkBitmap tab_0_bitmap = PaintView(tab_0);
  const SkBitmap split_bitmap = PaintView(split_view);

  EXPECT_FALSE(tab_0->IsActive());
  EXPECT_EQ(split_view->GetBorder(), nullptr);
  EXPECT_EQ(tab_0->tab_styling()->GetStrokeThickness(),
            IsNewPinnedTabStylingEnabled() ? 1 : 0);
  EXPECT_EQ(tab_0->tab_styling()
                ->GetPath(TabStyle::PathType::kActiveTab, 1.0f, {})
                .isRRect(nullptr),
            IsNewPinnedTabStylingEnabled());
  EXPECT_EQ(tab_0_bitmap.getColor(14, tab_0->height() / 2),
            !tab_0->tab_styling()->delegate()->IsGlassFrame() &&
                    (IsNewPinnedTabStylingEnabled() ||
                     tab_0->tab_styling()
                         ->delegate()
                         ->ShouldPaintTabBackgroundColor())
                ? tab_0->tab_styling()->CalculateTargetColors().background_color
                : SK_ColorTRANSPARENT);
  EXPECT_EQ(split_bitmap.getColor(split_view->width() / 2, 0),
            SK_ColorTRANSPARENT);
  EXPECT_EQ(split_bitmap.getColor(4, split_view->height() / 2),
            SK_ColorTRANSPARENT);
}

INSTANTIATE_TEST_SUITE_P(All,
                         HorizontalSplitTabViewPinnedStylingTest,
                         testing::Bool());

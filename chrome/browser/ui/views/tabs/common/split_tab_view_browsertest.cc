
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/views/view_utils.h"

class SplitTabViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
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

class HorizontalSplitTabViewTest : public SplitTabViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  bool IsNewPinnedTabStylingEnabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    SplitTabViewTest::SetUpOnMainThread();
    ExitVerticalTabsMode();
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = SplitTabViewTest::GetEnabledFeatures();
    enabled.push_back({tabs::kTabStripUnification, {}});
    if (IsNewPinnedTabStylingEnabled()) {
      enabled.push_back({tabs::kNewHorizontalPinnedTabStyling, {}});
    }
    return enabled;
  }

  const std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    auto disabled = SplitTabViewTest::GetDisabledFeatures();
    if (!IsNewPinnedTabStylingEnabled()) {
      disabled.push_back(tabs::kNewHorizontalPinnedTabStyling);
    }
    return disabled;
  }
};

class SplitTabViewParameterizedTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  TabStripOrientation orientation() const { return GetParam(); }
  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin<InProcessBrowserTest>::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsBrowserTestMixin<
        InProcessBrowserTest>::GetEnabledFeatures();
    enabled.push_back({tabs::kTabStripUnification, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_F(SplitTabViewTest, ProposedLayout_Unbounded) {
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

IN_PROC_BROWSER_TEST_F(SplitTabViewTest, ProposedLayout_LargeBounds) {
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

IN_PROC_BROWSER_TEST_F(SplitTabViewTest, ProposedLayout_LimitedBounds) {
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

IN_PROC_BROWSER_TEST_P(SplitTabViewParameterizedTest,
                       SplitTabsInGroup_LineViewPaint) {
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

IN_PROC_BROWSER_TEST_P(SplitTabViewParameterizedTest,
                       CreateSplitWithCollapsedTabGroup) {
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

IN_PROC_BROWSER_TEST_F(SplitTabViewTest,
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

IN_PROC_BROWSER_TEST_P(HorizontalSplitTabViewTest,
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

IN_PROC_BROWSER_TEST_P(HorizontalSplitTabViewTest,
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

INSTANTIATE_TEST_SUITE_P(All, HorizontalSplitTabViewTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(All,
                         SplitTabViewParameterizedTest,
                         testing::Values(TabStripOrientation::kVertical,
                                         TabStripOrientation::kHorizontal));

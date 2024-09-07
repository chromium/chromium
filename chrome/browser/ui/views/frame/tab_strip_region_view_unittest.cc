// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "tab_strip_region_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

// TabStripRegionViewTestBase contains no test cases.
class TabStripRegionViewTestBase : public ChromeViewsTestBase {
 public:
  explicit TabStripRegionViewTestBase(bool has_scrolling)
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    if (has_scrolling) {
      scoped_feature_list_.InitWithFeatures(
          {tabs::kScrollableTabStrip, features::kTabScrollingButtonPosition},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {tabs::kScrollableTabStrip});
    }
  }
  TabStripRegionViewTestBase(const TabStripRegionViewTestBase&) = delete;
  TabStripRegionViewTestBase& operator=(const TabStripRegionViewTestBase&) =
      delete;
  ~TabStripRegionViewTestBase() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    BuildTabStripRegionView();

    // Prevent hover cards from appearing when the mouse is over the tab. Tests
    // don't typically account for this possibly, so it can cause unrelated
    // tests to fail due to tab data not being set. See crbug.com/1050012.
    Tab::SetShowHoverCardOnMouseHoverForTesting(false);
  }

  void BuildTabStripRegionView() {
    auto controller = std::make_unique<FakeBaseTabStripController>();
    controller_ = controller.get();
    auto tab_strip = std::make_unique<TabStrip>(
        std::unique_ptr<TabStripController>(controller.release()));
    tab_strip_ = tab_strip.get();
    controller_->set_tab_strip(tab_strip_);
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    tab_strip_region_view_ = widget_->SetContentsView(
        std::make_unique<TabStripRegionView>(std::move(tab_strip)));
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  int GetInactiveTabWidth() { return tab_strip_->GetInactiveTabWidth(); }
  void CompleteAnimationAndLayout() {
    views::test::RunScheduledLayout(tab_strip_region_view_);
  }

  // Owned by TabStrip.
  raw_ptr<FakeBaseTabStripController, DanglingUntriaged> controller_ = nullptr;
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_ = nullptr;
  raw_ptr<TabStripRegionView, DanglingUntriaged> tab_strip_region_view_ =
      nullptr;
  std::unique_ptr<views::Widget> widget_;

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TabStripRegionViewTest contains tests that will run with scrolling enabled
// and disabled.
class TabStripRegionViewTest : public TabStripRegionViewTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  TabStripRegionViewTest() : TabStripRegionViewTestBase(GetParam()) {}
  TabStripRegionViewTest(const TabStripRegionViewTest&) = delete;
  TabStripRegionViewTest& operator=(const TabStripRegionViewTest&) = delete;
  ~TabStripRegionViewTest() override = default;
};

// TODO (crbug/1520595): Skip for now due to test failing when CR2023 enabled.
TEST_P(TabStripRegionViewTest, DISABLED_GrabHandleSpaceStaysVisible) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view_->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  for (int i = 0; i < 100; ++i) {
    controller_->AddTab(i,
                        (i == 0) ? TabActive::kActive : TabActive::kInactive);
    CompleteAnimationAndLayout();
    EXPECT_LE(tab_strip_region_view_->reserved_grab_handle_space_for_testing()
                  ->bounds()
                  .right(),
              kTabStripRegionViewWidth);
  }
}

// TODO (crbug/1520595): Skip for now due to test failing when CR2023 enabled.
TEST_P(TabStripRegionViewTest, DISABLED_NewTabButtonStaysVisible) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view_->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  for (int i = 0; i < 100; ++i) {
    controller_->AddTab(i,
                        (i == 0) ? TabActive::kActive : TabActive::kInactive);
    CompleteAnimationAndLayout();
    EXPECT_LE(tab_strip_region_view_->new_tab_button()->bounds().right(),
              kTabStripRegionViewWidth);
  }
}

// TODO (crbug/1520595): Skip for now due to test failing when CR2023 enabled.
TEST_P(TabStripRegionViewTest, DISABLED_NewTabButtonRightOfTabs) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view_->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  controller_->AddTab(0, TabActive::kActive);

  CompleteAnimationAndLayout();

  EXPECT_EQ(tab_strip_region_view_->new_tab_button()->bounds().x(),
            tab_strip_->tab_at(0)->bounds().right());
}

// TODO (crbug/1523257): Skip for now due to test failing when CR2023 enabled.
TEST_P(TabStripRegionViewTest, DISABLED_NewTabButtonInkDrop) {
  constexpr int kTabStripRegionViewWidth = 500;
  tab_strip_region_view_->SetBounds(0, 0, kTabStripRegionViewWidth,
                                    GetLayoutConstant(TAB_STRIP_HEIGHT));

  // Add a few tabs and simulate the new tab button's ink drop animation. This
  // should not cause any crashes since the ink drop layer size as well as the
  // ink drop container size should remain equal to the new tab button visible
  // bounds size. https://crbug.com/814105.
  auto* button = static_cast<TabStripControlButton*>(
      tab_strip_region_view_->new_tab_button());
  for (int i = 0; i < 10; ++i) {
    button->AnimateToStateForTesting(views::InkDropState::ACTION_TRIGGERED);
    controller_->AddTab(i, TabActive::kActive);
    CompleteAnimationAndLayout();
    button->AnimateToStateForTesting(views::InkDropState::HIDDEN);
  }
}

// We want to make sure that the following children views sits flush with the
// top of tab strip region view:
// * tab strip
// * new tab button
// This is important in ensuring that we maximise the targetable area of these
// views when the tab strip is flush with the top of the screen when the window
// is maximized (Fitt's Law).
TEST_P(TabStripRegionViewTest, ChildrenAreFlushWithTopOfTabStripRegionView) {
  tab_strip_region_view_->SetBounds(0, 0, 1000, 100);
  controller_->AddTab(0, TabActive::kActive);

  CompleteAnimationAndLayout();

  // The tab strip should sit flush with the top of the
  // |tab_strip_region_view_|.
  gfx::Point tab_strip_origin(tab_strip_->bounds().origin());
  views::View::ConvertPointToTarget(tab_strip_, tab_strip_region_view_,
                                    &tab_strip_origin);
  EXPECT_EQ(0, tab_strip_origin.y());

  // The new tab button should sit flush with the top of the
  // |tab_strip_region_view_|.
  gfx::Point new_tab_button_origin(
      tab_strip_region_view_->new_tab_button()->bounds().origin());
  views::View::ConvertPointToTarget(tab_strip_, tab_strip_region_view_,
                                    &new_tab_button_origin);
  EXPECT_EQ(0, new_tab_button_origin.y());
}

TEST_P(TabStripRegionViewTest, TabSearchPositionLoggedOnConstruction) {
  using TabSearchPositionEnum = TabStripRegionView::TabSearchPositionEnum;
  const bool tab_search_trailing_tabstrip =
      tabs::GetTabSearchTrailingTabstrip(tab_strip_region_view_->profile());
  TabSearchPositionEnum expected_enum_val =
      tab_search_trailing_tabstrip ? TabSearchPositionEnum::kTrailing
                                   : TabSearchPositionEnum::kLeading;

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount("Tabs.TabSearch.PositionInTabstrip",
                                     TabSearchPositionEnum::kLeading, 0);
  histogram_tester.ExpectBucketCount("Tabs.TabSearch.PositionInTabstrip",
                                     TabSearchPositionEnum::kTrailing, 0);
  BuildTabStripRegionView();
  histogram_tester.ExpectBucketCount("Tabs.TabSearch.PositionInTabstrip",
                                     expected_enum_val, 1);
}

TEST_P(TabStripRegionViewTest, HasMultiselectableState) {
  ui::AXNodeData ax_node_data;
  tab_strip_region_view_->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data);
  EXPECT_TRUE(ax_node_data.HasState(ax::mojom::State::kMultiselectable));
}

class TabStripRegionViewTestWithScrollingDisabled
    : public TabStripRegionViewTestBase {
 public:
  TabStripRegionViewTestWithScrollingDisabled()
      : TabStripRegionViewTestBase(false) {}
  TabStripRegionViewTestWithScrollingDisabled(
      const TabStripRegionViewTestWithScrollingDisabled&) = delete;
  TabStripRegionViewTestWithScrollingDisabled& operator=(
      const TabStripRegionViewTestWithScrollingDisabled&) = delete;
  ~TabStripRegionViewTestWithScrollingDisabled() override = default;

 private:
};

// When scrolling is disabled, the tab strip cannot be larger than the container
// so tabs that do not fit in the tabstrip will become invisible. This is the
// opposite behavior from
// TabStripRegionViewTestWithScrollingEnabled.TabStripCanBeLargerThanContainer.
TEST_F(TabStripRegionViewTestWithScrollingDisabled,
       TabStripCannotBeLargerThanContainer) {
  const int minimum_active_width = TabStyle::Get()->GetMinimumInactiveWidth();
  controller_->AddTab(0, TabActive::kActive);
  CompleteAnimationAndLayout();

  // Add tabs to the tabstrip until it is full.
  while (GetInactiveTabWidth() > minimum_active_width) {
    controller_->AddTab(0, TabActive::kInactive);
    CompleteAnimationAndLayout();
    EXPECT_LT(tab_strip_->width(), tab_strip_region_view_->width());
  }

  // Add a few more tabs after the tabstrip is full to ensure tabs added
  // afterwards are not visible.
  for (int i = 0; i < 10; i++) {
    controller_->AddTab(0, TabActive::kInactive);
    CompleteAnimationAndLayout();
  }
  EXPECT_LT(tab_strip_->width(), tab_strip_region_view_->width());
  EXPECT_FALSE(
      tab_strip_->tab_at(tab_strip_->GetModelCount() - 1)->GetVisible());
}

class TabStripRegionViewTestWithScrollingEnabled
    : public TabStripRegionViewTestBase {
 public:
  TabStripRegionViewTestWithScrollingEnabled()
      : TabStripRegionViewTestBase(true) {}
  TabStripRegionViewTestWithScrollingEnabled(
      const TabStripRegionViewTestWithScrollingEnabled&) = delete;
  TabStripRegionViewTestWithScrollingEnabled& operator=(
      const TabStripRegionViewTestWithScrollingEnabled&) = delete;
  ~TabStripRegionViewTestWithScrollingEnabled() override = default;
};

// When scrolling is enabled, the tab strip can grow to be larger than the
// container. This is the opposite behavior from
// TabStripRegionViewTestWithScrollingDisabled.
// TabStripCannotBeLargerThanContainer.
TEST_F(TabStripRegionViewTestWithScrollingEnabled,
       TabStripCanBeLargerThanContainer) {
  const int minimum_active_width = TabStyle::Get()->GetMinimumInactiveWidth();
  controller_->AddTab(0, TabActive::kActive);
  CompleteAnimationAndLayout();

  // Add tabs to the tabstrip until it is full and should start overflowing.
  while (GetInactiveTabWidth() > minimum_active_width) {
    controller_->AddTab(0, TabActive::kInactive);
    CompleteAnimationAndLayout();
    EXPECT_LT(tab_strip_->width(), tab_strip_region_view_->width());
  }

  // Add a few more tabs after the tabstrip is full to ensure the tabstrip
  // starts scrolling. This needs to expand the tabstrip width by a decent
  // amount in order to get the tabstrip to be wider than the entire tabstrip
  // region, not just the portion of that that's allocated to the tabstrip
  // itself (e.g. some of that space is for the NTB).
  for (int i = 0; i < 10; i++) {
    controller_->AddTab(0, TabActive::kInactive);
    CompleteAnimationAndLayout();
  }
  EXPECT_GT(tab_strip_->width(), tab_strip_region_view_->width());
  EXPECT_TRUE(
      tab_strip_->tab_at(tab_strip_->GetModelCount() - 1)->GetVisible());
}

TEST_F(TabStripRegionViewTestWithScrollingEnabled,
       TabStripScrollButtonsNotInWindowCaption) {
  const int minimum_active_width = TabStyle::Get()->GetMinimumInactiveWidth();
  controller_->AddTab(0, TabActive::kActive);
  CompleteAnimationAndLayout();

  // Add tabs to the tabstrip until it is full and should start overflowing.
  while (GetInactiveTabWidth() > minimum_active_width) {
    controller_->AddTab(0, TabActive::kInactive);
    CompleteAnimationAndLayout();
  }

  // Add a few more tabs after the tabstrip is full to ensure the tabstrip
  // starts scrolling. This needs to expand the tabstrip width by a decent
  // amount in order to get the tabstrip to be wider than the entire tabstrip
  // region, not just the portion of that that's allocated to the tabstrip
  // itself (e.g. some of that space is for the NTB).
  for (int i = 0; i < 10; i++) {
    controller_->AddTab(0, TabActive::kInactive);
    CompleteAnimationAndLayout();
  }

  raw_ptr<TabStripScrollContainer> scroll_container =
      views::AsViewClass<TabStripScrollContainer>(
          tab_strip_region_view_->GetTabStripContainerForTesting());
  raw_ptr<views::ImageButton> leading_scroll_button_ =
      scroll_container->GetLeadingScrollButtonForTesting();
  raw_ptr<views::ImageButton> trailing_scroll_button_ =
      scroll_container->GetTrailingScrollButtonForTesting();

  // Check to see if children are visible
  EXPECT_TRUE(leading_scroll_button_ != nullptr &&
              leading_scroll_button_->IsDrawn());
  EXPECT_TRUE(trailing_scroll_button_ != nullptr &&
              trailing_scroll_button_->IsDrawn());

  gfx::Point scrolling_button_point =
      leading_scroll_button_->bounds().CenterPoint();
  gfx::Rect scrolling_button_rect =
      gfx::Rect(scrolling_button_point, gfx::Size(1, 1));
  gfx::RectF floating_rect_in_target_coords_f(scrolling_button_rect);
  views::View::ConvertRectToTarget(leading_scroll_button_,
                                   tab_strip_region_view_,
                                   &floating_rect_in_target_coords_f);

  EXPECT_FALSE(tab_strip_region_view_->IsRectInWindowCaption(
      gfx::ToEnclosingRect(floating_rect_in_target_coords_f)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripRegionViewTest,
                         ::testing::Values(true, false));

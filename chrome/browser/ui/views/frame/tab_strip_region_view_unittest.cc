// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// TabStripRegionViewTestBase contains no test cases.
class TabStripRegionViewTestBase : public ChromeViewsTestBase {
 public:
  explicit TabStripRegionViewTestBase(bool has_scrolling)
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    if (has_scrolling) {
      scoped_feature_list_.InitWithFeatures({features::kScrollableTabStrip},
                                            {});
    } else {
      scoped_feature_list_.InitWithFeatures({},
                                            {features::kScrollableTabStrip});
    }
  }
  TabStripRegionViewTestBase(const TabStripRegionViewTestBase&) = delete;
  TabStripRegionViewTestBase& operator=(const TabStripRegionViewTestBase&) =
      delete;
  ~TabStripRegionViewTestBase() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    auto controller = std::make_unique<FakeBaseTabStripController>();
    controller_ = controller.get();
    auto tab_strip = std::make_unique<TabStrip>(
        std::unique_ptr<TabStripController>(controller.release()));
    tab_strip_ = tab_strip.get();
    controller_->set_tab_strip(tab_strip_);
    widget_ = CreateTestWidget();
    tab_strip_region_view_ = widget_->SetContentsView(
        std::make_unique<TabStripRegionView>(std::move(tab_strip)));

    // Prevent hover cards from appearing when the mouse is over the tab. Tests
    // don't typically account for this possibly, so it can cause unrelated
    // tests to fail due to tab data not being set. See crbug.com/1050012.
    Tab::SetShowHoverCardOnMouseHoverForTesting(false);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  int GetInactiveTabWidth() { return tab_strip_->GetInactiveTabWidth(); }
  void CompleteAnimationAndLayout() {
    tab_strip_->CompleteAnimationAndLayout();
  }

  // Owned by TabStrip.
  FakeBaseTabStripController* controller_ = nullptr;
  TabStrip* tab_strip_ = nullptr;
  TabStripRegionView* tab_strip_region_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;
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
  const int minimum_active_width = TabStyleViews::GetMinimumInactiveWidth();
  controller_->AddTab(0, true);
  CompleteAnimationAndLayout();

  // Add tabs to the tabstrip until it is full.
  while (GetInactiveTabWidth() > minimum_active_width) {
    controller_->AddTab(0, false);
    CompleteAnimationAndLayout();
  }

  // Add a few more tabs after the tabstrip is full to ensure tabs added
  // afterwards are not visible.
  for (int i = 0; i < 5; i++) {
    controller_->AddTab(0, false);
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
  const int minimum_active_width = TabStyleViews::GetMinimumInactiveWidth();
  controller_->AddTab(0, true);
  CompleteAnimationAndLayout();

  // Add tabs to the tabstrip until it is full and should start overflowing.
  while (GetInactiveTabWidth() > minimum_active_width) {
    controller_->AddTab(0, false);
    CompleteAnimationAndLayout();
  }

  // Add a few more tabs after the tabstrip is full to ensure the tabstrip
  // starts scrolling.
  for (int i = 0; i < 5; i++) {
    controller_->AddTab(0, false);
    CompleteAnimationAndLayout();
  }
  EXPECT_GT(tab_strip_->width(), tab_strip_region_view_->width());
  EXPECT_TRUE(
      tab_strip_->tab_at(tab_strip_->GetModelCount() - 1)->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripRegionViewTest,
                         ::testing::Values(true, false));

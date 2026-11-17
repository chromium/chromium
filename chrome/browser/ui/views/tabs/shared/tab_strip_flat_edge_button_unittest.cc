// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"

#include <memory>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/widget/widget.h"

namespace {

class TestTabStripFlatEdgeButton : public TabStripFlatEdgeButton {
 public:
  void set_frame_condensed(bool condensed) { condensed_ = condensed; }

  // TabStripFlatEdgeButton:
  bool IsFrameCondensed() const override { return condensed_; }

 private:
  bool condensed_ = false;
};

}  // namespace

class TabStripFlatEdgeButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    button_ = std::make_unique<TabStripFlatEdgeButton>();
  }

  void TearDown() override {
    button_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TabStripFlatEdgeButton> button_;
};

TEST_F(TabStripFlatEdgeButtonTest, DefaultCornerRadiiAcrossFlatEdges) {
  constexpr float kDefaultRadius = 10.0f;
  constexpr float kFlatRadius = 2.0f;

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kNone);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kDefaultRadius, kDefaultRadius, kDefaultRadius,
                                 kDefaultRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kTop);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kFlatRadius, kFlatRadius, kDefaultRadius,
                                 kDefaultRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kLeft);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kFlatRadius, kDefaultRadius, kDefaultRadius,
                                 kFlatRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kBottom);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kDefaultRadius, kDefaultRadius, kFlatRadius,
                                 kFlatRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kRight);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kDefaultRadius, kFlatRadius, kFlatRadius,
                                 kDefaultRadius));
}

TEST_F(TabStripFlatEdgeButtonTest, CustomCornerRadiusAcrossFlatEdges) {
  constexpr float kCustomRadius = 16.0f;
  constexpr float kFlatRadius = 2.0f;

  button_->SetCornerRadius(kCustomRadius);

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kNone);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kCustomRadius, kCustomRadius, kCustomRadius,
                                 kCustomRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kTop);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kFlatRadius, kFlatRadius, kCustomRadius,
                                 kCustomRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kLeft);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kFlatRadius, kCustomRadius, kCustomRadius,
                                 kFlatRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kBottom);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kCustomRadius, kCustomRadius, kFlatRadius,
                                 kFlatRadius));

  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kRight);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kCustomRadius, kFlatRadius, kFlatRadius,
                                 kCustomRadius));
}

TEST_F(TabStripFlatEdgeButtonTest, FlatEdgeFactorWithCustomCornerRadius) {
  constexpr float kCustomRadius = 16.0f;
  constexpr float kFlatRadius = 2.0f;

  button_->SetCornerRadius(kCustomRadius);
  button_->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kTop);

  // Full flat edge (factor = 1.0)
  button_->SetFlatEdgeFactor(1.0f);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kFlatRadius, kFlatRadius, kCustomRadius,
                                 kCustomRadius));

  // Half flat edge (factor = 0.5): 2.0 + (16.0 - 2.0) * (1.0 - 0.5) = 9.0
  button_->SetFlatEdgeFactor(0.5f);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(9.0f, 9.0f, kCustomRadius, kCustomRadius));

  // No flat edge (factor = 0.0): fully rounded to custom radius
  button_->SetFlatEdgeFactor(0.0f);
  EXPECT_EQ(button_->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(kCustomRadius, kCustomRadius, kCustomRadius,
                                 kCustomRadius));
}

TEST_F(TabStripFlatEdgeButtonTest, WidgetAttachmentUpdatesInkDrop) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  TabStripFlatEdgeButton* button = widget->SetContentsView(std::move(button_));

  // Changing properties before ink drop instantiation should not lazily create
  // an ink drop object.
  EXPECT_FALSE(views::InkDrop::Get(button)->HasInkDrop());
  button->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kLeft);
  button->SetFlatEdgeFactor(0.5f);
  button->SetCornerRadius(14.0f);
  EXPECT_FALSE(views::InkDrop::Get(button)->HasInkDrop());

  EXPECT_EQ(button->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(8.0f, 14.0f, 14.0f, 8.0f));

  // Once an ink drop exists, property updates should notify it safely.
  views::InkDrop::Get(button)->GetInkDrop();
  EXPECT_TRUE(views::InkDrop::Get(button)->HasInkDrop());
  button->SetFlatEdge(TabStripFlatEdgeButton::FlatEdge::kRight);
  button->SetFlatEdgeFactor(0.0f);
  button->SetCornerRadius(10.0f);
  EXPECT_EQ(button->GetButtonCornerRadiiForTesting(),
            gfx::RoundedCornersF(10.0f, 10.0f, 10.0f, 10.0f));
}

TEST_F(TabStripFlatEdgeButtonTest, PaintTransparentDefaultAndSetter) {
  EXPECT_FALSE(button_->paint_transparent_for_testing());
  button_->SetPaintTransparent(true);
  EXPECT_TRUE(button_->paint_transparent_for_testing());
  button_->SetPaintTransparent(false);
  EXPECT_FALSE(button_->paint_transparent_for_testing());
}

TEST_F(TabStripFlatEdgeButtonTest, ForegroundFrameColorIds) {
  EXPECT_EQ(button_->foreground_frame_active_color_id_for_testing(),
            kColorTabSearchButtonCRForegroundFrameActive);
  EXPECT_EQ(button_->foreground_frame_inactive_color_id_for_testing(),
            kColorTabSearchButtonCRForegroundFrameInactive);

  button_->SetForegroundFrameActiveColorId(
      kColorTabForegroundInactiveFrameActive);
  EXPECT_EQ(button_->foreground_frame_active_color_id_for_testing(),
            kColorTabForegroundInactiveFrameActive);

  button_->SetForegroundFrameInactiveColorId(
      kColorNewTabButtonCRForegroundFrameInactive);
  EXPECT_EQ(button_->foreground_frame_inactive_color_id_for_testing(),
            kColorNewTabButtonCRForegroundFrameInactive);
}

TEST_F(TabStripFlatEdgeButtonTest, SetOuterInsetsUpdatesPreferredSizeAndMask) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  TabStripFlatEdgeButton* button = widget->SetContentsView(std::move(button_));

  button->SetButtonSize(28);
  button->SetCornerRadius(14.0f);
  constexpr auto kOuterInsets = gfx::Insets::TLBR(6, 0, 7, 0);
  button->SetOuterInsets(kOuterInsets);

  EXPECT_EQ(button->GetPreferredSize(), gfx::Size(28, 41));
  EXPECT_EQ(button->GetInsets(), kOuterInsets);

  // Internal insets combine with outer insets without double-counting.
  button->SetInsets(gfx::Insets::VH(0, 4));
  EXPECT_EQ(button->GetInsets(), gfx::Insets::TLBR(6, 4, 7, 4));

  button->SetSize(button->GetPreferredSize());
  // Inside the circular visual button.
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(14, 20)));
  // Outside the rounded top-left corner of the circle.
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(0, 6)));
  // Above and below the visual button in the outer insets.
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(14, 2)));
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(14, 38)));
}

TEST_F(TabStripFlatEdgeButtonTest, ExtendHitTestToTopRespectsCondensedFrame) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* button =
      widget->SetContentsView(std::make_unique<TestTabStripFlatEdgeButton>());

  button->SetButtonSize(28);
  button->SetCornerRadius(14.0f);
  button->SetOuterInsets(gfx::Insets::TLBR(6, 0, 7, 0));
  button->SetExtendHitTestToTop(true);
  button->SetSize(button->GetPreferredSize());

  // When the frame is not condensed, the top outer inset is not hit-tested.
  button->set_frame_condensed(false);
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(0, 0)));
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(14, 0)));
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(14, 2)));
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(14, 20)));
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(14, 38)));

  // When the frame is condensed, the top corners become square and the hit
  // test mask extends all the way to the top edge (y = 0).
  button->set_frame_condensed(true);
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(0, 0)));
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(14, 0)));
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(27, 0)));
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(14, 2)));
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(14, 20)));
  // Bottom remains rounded and excluded from the bottom outer inset.
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(0, 33)));
  EXPECT_FALSE(button->HitTestPoint(gfx::Point(14, 38)));
}

TEST_F(TabStripFlatEdgeButtonTest,
       InternalInsetsStayClickableWithoutOuterInsets) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  TabStripFlatEdgeButton* button = widget->SetContentsView(std::move(button_));

  const int default_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);
  button->SetInsets(gfx::Insets::VH(5, 6));

  EXPECT_EQ(button->GetPreferredSize(), gfx::Size(default_size, default_size));
  button->SetSize(button->GetPreferredSize());
  // Internal top inset remains inside the clickable button shape.
  EXPECT_TRUE(button->HitTestPoint(gfx::Point(default_size / 2, 2)));
}

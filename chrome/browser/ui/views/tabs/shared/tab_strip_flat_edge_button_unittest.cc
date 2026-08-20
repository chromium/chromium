// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/tab_strip_flat_edge_button.h"

#include <memory>

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/widget/widget.h"

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

TEST_F(TabStripFlatEdgeButtonTest, PaintTransparentForGlassDefaultAndSetter) {
  EXPECT_FALSE(button_->paint_transparent_for_glass_for_testing());
  button_->SetPaintTransparentForGlass(true);
  EXPECT_TRUE(button_->paint_transparent_for_glass_for_testing());
  button_->SetPaintTransparentForGlass(false);
  EXPECT_FALSE(button_->paint_transparent_for_glass_for_testing());
}

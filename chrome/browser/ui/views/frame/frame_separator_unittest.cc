// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/frame_separator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr SkColor kBackgroundColor = SK_ColorRED;
constexpr ui::ColorId kForegroundColorId = ui::kColorSeparator;
constexpr ui::ColorId kBackgroundColorId = ui::kColorAccent;
constexpr gfx::Size kTestImageSize{24, 24};
}  // namespace

// Base test fixture for Separator tests.
class FrameSeparatorTest : public views::ViewsTestBase {
 public:
  FrameSeparatorTest() = default;
  ~FrameSeparatorTest() override = default;

 protected:
  // views::ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget1_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget2_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    separator_ = widget1_->SetContentsView(std::make_unique<FrameSeparator>());
    separator_->SetSize({10, 10});
    separator_->SetColorId(kForegroundColorId);
    widget1_->Show();
    widget2_->ShowInactive();
    views::test::WaitForWidgetActive(widget1_.get(), true);
    ASSERT_TRUE(widget1_->ShouldPaintAsActive());
    ASSERT_FALSE(widget2_->ShouldPaintAsActive());
  }

  void TearDown() override {
    separator_ = nullptr;
    widget1_.reset();
    widget2_.reset();
    ViewsTestBase::TearDown();
  }

  void ActivateWidget2() {
    widget2_->Show();
    widget2_->Activate();
    views::test::WaitForWidgetActive(widget2_.get(), true);
  }

  SkBitmap PaintToCanvas() {
    gfx::Canvas canvas(kTestImageSize, 1.0f, true);
    canvas.DrawColor(kBackgroundColor);
    separator_->OnPaint(&canvas);
    return canvas.GetBitmap();
  }

  SkColor GetColor(ui::ColorId color) const {
    return separator_->GetColorProvider()->GetColor(color);
  }

  std::unique_ptr<views::Widget> widget1_;
  std::unique_ptr<views::Widget> widget2_;
  raw_ptr<FrameSeparator> separator_ = nullptr;
};

TEST_F(FrameSeparatorTest,
       UsesActiveColorInActiveWindowWhenInactiveColorNotSet) {
  const auto expected_color = GetColor(kForegroundColorId);
  const SkBitmap painted = PaintToCanvas();
  EXPECT_EQ(expected_color, painted.getColor(0, 0));
  EXPECT_EQ(expected_color, painted.getColor(0, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 0));
}

TEST_F(FrameSeparatorTest,
       UsesActiveColorInInactiveWindowWhenInactiveColorNotSet) {
  ActivateWidget2();
  const auto expected_color = GetColor(kForegroundColorId);
  const SkBitmap painted = PaintToCanvas();
  EXPECT_EQ(expected_color, painted.getColor(0, 0));
  EXPECT_EQ(expected_color, painted.getColor(0, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 0));
}

TEST_F(FrameSeparatorTest, UsesActiveColorInActiveWindowWhenInactiveColorSet) {
  EXPECT_EQ(std::nullopt, separator_->GetInactiveColorId());
  separator_->SetInactiveColorId(kBackgroundColorId);
  EXPECT_EQ(kBackgroundColorId, separator_->GetInactiveColorId());

  const auto expected_color = GetColor(kForegroundColorId);
  const SkBitmap painted = PaintToCanvas();
  EXPECT_EQ(expected_color, painted.getColor(0, 0));
  EXPECT_EQ(expected_color, painted.getColor(0, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 0));
}

TEST_F(FrameSeparatorTest, UsesInactiveColorInInactiveWindow) {
  separator_->SetInactiveColorId(kBackgroundColorId);
  ActivateWidget2();
  const auto expected_color = GetColor(kBackgroundColorId);
  const SkBitmap painted = PaintToCanvas();
  EXPECT_EQ(expected_color, painted.getColor(0, 0));
  EXPECT_EQ(expected_color, painted.getColor(0, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 9));
  EXPECT_EQ(expected_color, painted.getColor(9, 0));
}

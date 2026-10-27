// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/voice_plate.h"

#include <cstdlib>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace voice {

namespace {

// A voice plate whose contents are an empty view of the given size.
class TestVoicePlate : public VoicePlate {
 public:
  TestVoicePlate(views::View* anchor_view, const gfx::Size& contents_size)
      : VoicePlate(anchor_view) {
    auto contents = std::make_unique<views::View>();
    contents->SetPreferredSize(contents_size);
    SetContentsView(std::move(contents));
    CreateWidget();
  }
  ~TestVoicePlate() override = default;
};

class VoicePlateTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->SetBounds(gfx::Rect(0, 0, 800, 600));
    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_view_ = nullptr;
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  static int GetMinimumMargin() {
    return ChromeLayoutProvider::Get()->GetDistanceMetric(
               DISTANCE_TOAST_BUBBLE_BROWSER_WINDOW_MARGIN) -
           views::BubbleBorder::kShadowBlur;
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<views::View> anchor_view_ = nullptr;
};

TEST_F(VoicePlateTest, HostsContentsAndOwnsWidget) {
  auto voice_plate =
      std::make_unique<TestVoicePlate>(anchor_view_, gfx::Size(200, 30));

  ASSERT_NE(voice_plate->GetWidget(), nullptr);
  EXPECT_NE(voice_plate->GetContentsView(), nullptr);
  EXPECT_FALSE(voice_plate->ShouldCloseOnDeactivate());
}

TEST_F(VoicePlateTest, ShowsWithoutActivating) {
  auto voice_plate =
      std::make_unique<TestVoicePlate>(anchor_view_, gfx::Size(200, 30));
  voice_plate->Show();

  EXPECT_TRUE(voice_plate->GetWidget()->IsVisible());
  EXPECT_FALSE(voice_plate->GetWidget()->IsActive());
}

TEST_F(VoicePlateTest, CenteredOverlappingTheBottomOfTheAnchor) {
  auto voice_plate =
      std::make_unique<TestVoicePlate>(anchor_view_, gfx::Size(200, 30));
  voice_plate->Show();

  const gfx::Rect anchor_bounds = anchor_view_->GetBoundsInScreen();
  const gfx::Size preferred_size =
      voice_plate->GetWidget()->GetContentsView()->GetPreferredSize();
  const gfx::Rect bounds = voice_plate->GetBubbleBounds();

  EXPECT_EQ(bounds.size(), preferred_size);
  EXPECT_LE(
      std::abs(bounds.CenterPoint().x() - anchor_bounds.CenterPoint().x()), 1);
  EXPECT_EQ(bounds.y(), anchor_bounds.bottom() - 10);
}

TEST_F(VoicePlateTest, WidthIsLimitedByTheAnchor) {
  auto voice_plate =
      std::make_unique<TestVoicePlate>(anchor_view_, gfx::Size(2000, 30));
  voice_plate->Show();

  const gfx::Rect anchor_bounds = anchor_view_->GetBoundsInScreen();
  const gfx::Rect bounds = voice_plate->GetBubbleBounds();

  EXPECT_EQ(bounds.width(), anchor_bounds.width() - 2 * GetMinimumMargin());
  EXPECT_LE(
      std::abs(bounds.CenterPoint().x() - anchor_bounds.CenterPoint().x()), 1);
}

}  // namespace

}  // namespace voice

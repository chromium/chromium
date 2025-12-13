// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_tucker.h"

#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

class PictureInPictureTuckerTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    display::Screen::SetScreenInstance(&test_screen_);
    views::ViewsTestBase::SetUp();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    tucker_ = std::make_unique<PictureInPictureTucker>(*widget_.get());
  }

  void TearDown() override {
    tucker_.reset();
    widget_.reset();
    views::ViewsTestBase::TearDown();
    display::Screen::SetScreenInstance(nullptr);
  }

 protected:
  void Tuck() {
    tucker_->Tuck();
    tucker_->FinishAnimationForTesting();
  }

  void Untuck() {
    tucker_->Untuck();
    tucker_->FinishAnimationForTesting();
  }

  display::test::TestScreen& test_screen() { return test_screen_; }
  views::Widget* widget() { return widget_.get(); }
  PictureInPictureTucker* tucker() { return tucker_.get(); }

 private:
  display::test::TestScreen test_screen_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<PictureInPictureTucker> tucker_;
};

TEST_F(PictureInPictureTuckerTest, TucksTowardCloserEdge) {
  const gfx::Rect work_area =
      display::Screen::Get()
          ->GetDisplayNearestWindow(widget()->GetNativeWindow())
          .work_area();

  // Place the widget on the left side of the screen.
  const gfx::Rect initial_leftside_bounds({work_area.x() + 20, 50}, {200, 200});
  widget()->SetBounds(initial_leftside_bounds);
  EXPECT_EQ(initial_leftside_bounds, widget()->GetWindowBoundsInScreen());

  // Tuck the widget. This should tuck towards the left side.
  Tuck();
  EXPECT_LT(widget()->GetWindowBoundsInScreen().x(),
            initial_leftside_bounds.x());

  // Untucking should place the widget back where it started.
  Untuck();
  EXPECT_EQ(initial_leftside_bounds, widget()->GetWindowBoundsInScreen());

  // Place the widget on the right side of the screen.
  const gfx::Rect initial_rightside_bounds({work_area.right() - 220, 50},
                                           {200, 200});
  widget()->SetBounds(initial_rightside_bounds);
  EXPECT_EQ(initial_rightside_bounds, widget()->GetWindowBoundsInScreen());

  // Tuck the widget. This should tuck towards the right side.
  Tuck();
  EXPECT_GT(widget()->GetWindowBoundsInScreen().x(),
            initial_rightside_bounds.x());

  // Untucking should place the widget back where it started.
  Untuck();
  EXPECT_EQ(initial_rightside_bounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(PictureInPictureTuckerTest,
       RetuckingForResizeRemembersOriginalPosition) {
  const gfx::Rect initial_bounds({20, 50}, {200, 200});
  widget()->SetBounds(initial_bounds);
  EXPECT_EQ(initial_bounds, widget()->GetWindowBoundsInScreen());

  // Tuck the widget.
  Tuck();
  ASSERT_NE(initial_bounds, widget()->GetWindowBoundsInScreen());

  // Resize the widget and retuck it.
  gfx::Size new_size = {300, 300};
  widget()->SetSize(new_size);
  Tuck();

  // Untucking should place the widget back where it started.
  Untuck();
  const gfx::Rect expected_bounds(initial_bounds.origin(), new_size);
  EXPECT_EQ(expected_bounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(PictureInPictureTuckerTest, TucksTowardCloserEdge_Multiscreen) {
  // Set up two screens side-by-side.
  const gfx::Rect display1_bounds({0, 0}, {1000, 1000});
  const gfx::Rect display2_bounds({1000, 0}, {500, 500});
  display::Display display1 = test_screen().GetPrimaryDisplay();
  display1.set_work_area(display1_bounds);
  test_screen().display_list().UpdateDisplay(display1);
  display::Display display2(display1.id() + 1, display2_bounds);
  test_screen().display_list().AddOrUpdateDisplay(
      display2, display::DisplayList::Type::NOT_PRIMARY);

  // Place the widget on the left side of the left screen. This should tuck to
  // left side of the left screen.
  widget()->SetBounds({{20, 50}, {200, 200}});
  Tuck();
  EXPECT_LT(widget()->GetWindowBoundsInScreen().x(), display1_bounds.x());
  Untuck();

  // Place the widget on the right half of the left screen, but still closer to
  // the left edge of the left screen than the right edge of the right screen.
  // This should tuck to the left side of the left screen.
  widget()->SetBounds({{600, 50}, {200, 200}});
  Tuck();
  EXPECT_LT(widget()->GetWindowBoundsInScreen().x(), display1_bounds.x());
  Untuck();

  // Place the widget on the right half of the left screen, and closer to the
  // right side of the right screen than the left side of the left screen. This
  // should tuck to the right side of the right screen.
  widget()->SetBounds({{800, 50}, {200, 200}});
  Tuck();
  EXPECT_GT(widget()->GetWindowBoundsInScreen().right(),
            display2_bounds.right());
  Untuck();

  // Place the widget on the right half of the left screen, and closer to the
  // right side of the right screen than the left side of the left screen, BUT
  // close enough to the bottom of the left screen (which is taller) that moving
  // to the right side of the right screen would be below the right screen. This
  // should tuck to the right side of the left screen.
  widget()->SetBounds({{800, 550}, {200, 200}});
  Tuck();
  EXPECT_GT(widget()->GetWindowBoundsInScreen().right(),
            display1_bounds.right());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().x(), display1_bounds.right());
  Untuck();
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog_win/button_layout.h"

#include <algorithm>
#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

class ButtonLayoutTest
    : public ::testing::TestWithParam<::testing::tuple<int, int>> {
 private:
  enum {
    // The width of an imaginary host view in the test.
    kFixedHostWidth = 100,
  };

 public:
  // Various button widths to be tested.
  enum {
    // Magic width meaning no button at all.
    kNoButton = 0,

    // Some small "narrow" button that fits within half of the test's host.
    kNarrowButtonMin = 8,

    // The largest "narrow" button that could fit within the test's host.
    kNarrowButtonMax =
        (kFixedHostWidth - ButtonLayout::kPaddingBetweenButtons) / 2,

    // Some mid-sized "narrow" button that could fit within the test's host.
    kNarrowButtonMid = (kNarrowButtonMin + kNarrowButtonMax) / 2,

    // The least wide "wide" button that could fit within the test's host.
    kWideButtonMin = kNarrowButtonMax + 1,

    // The largest "wide" button that could fit within the test's host.
    kWideButtonMax = kFixedHostWidth,

    // Some mid-sized "wide" button that could fit within the test's host.
    kWideButtonMid = (kWideButtonMin + kWideButtonMax) / 2,

    // A button that is too big to fit within the host.
    kSuperSizedButton = kWideButtonMax + 1,
  };

 protected:
  ButtonLayoutTest()
      : layout_(host_.SetLayoutManager(
            std::make_unique<ButtonLayout>(kFixedHostWidth))),
        button_1_width_(::testing::get<0>(GetParam())),
        button_2_width_(::testing::get<1>(GetParam())) {}

  void SetUp() override {
    ASSERT_NE(0, button_1_width_) << "Button 1 must always be present.";
  }

  views::View* host() { return &host_; }
  views::LayoutManager* layout() { return layout_; }
  bool has_two_buttons() const { return button_2_width_; }

  // Adds one or two child views of widths specified by the test parameters.
  void AddChildViews() {
    auto view = std::make_unique<views::View>();
    view->SetPreferredSize({button_1_width_, kButtonHeight});
    host()->AddChildView(view.release());

    if (has_two_buttons()) {
      view = std::make_unique<views::View>();
      view->SetPreferredSize({button_2_width_, kButtonHeight});
      host()->AddChildView(view.release());
    }
  }

  // Returns the (fixed) width of the host.
  int GetExpectedWidth() const { return kFixedHostWidth; }

  // Returns the height of the host, which is dependent on the number and widths
  // of the children.
  int GetExpectedHeight() const {
    if (!has_two_buttons())
      return kButtonHeight;
    if (button_1_width_ <= kNarrowButtonMax &&
        button_2_width_ <= kNarrowButtonMax) {
      return kButtonHeight;
    }
    return 2 * kButtonHeight + ButtonLayout::kPaddingBetweenButtons;
  }

  // Returns the expected bounding rectangle for |child_number|.
  gfx::Rect GetExpectedButtonBounds(int child_number) const {
    gfx::Rect bounds;

    // Width is determined by the max of the two buttons being bigger than
    // the max narrow button.
    if (std::max(button_1_width_, button_2_width_) > kNarrowButtonMax)
      bounds.set_width(kWideButtonMax);
    else
      bounds.set_width(kNarrowButtonMax);

    // All buttons have the same height.
    bounds.set_height(kButtonHeight);

    // Position is based on which button we're talking about.
    switch (child_number) {
      case 1:
        // Offset button 1 if there's only one button and it's narrow.
        if (!has_two_buttons() && bounds.width() == kNarrowButtonMax)
          bounds.set_x(kRightButtonXOffset);
        break;
      case 2:
        // Offset button 2 horizontally if the buttons are narrow; vertically,
        // otherwise.
        if (bounds.width() == kNarrowButtonMax)
          bounds.set_x(kRightButtonXOffset);
        else
          bounds.set_y(kBottomButtonYOffset);
        break;
      default:
        ADD_FAILURE() << "child_number out of bounds";
        return gfx::Rect();
    }

    return bounds;
  }

  // Expects that the bounds of |view| are equal to |bounds|.
  void ExpectViewBoundsEquals(const views::View* view,
                              const gfx::Rect& bounds) {
    const gfx::Rect& child_bounds = view->bounds();
    EXPECT_EQ(child_bounds.x(), bounds.x());
    EXPECT_EQ(child_bounds.y(), bounds.y());
    EXPECT_EQ(child_bounds.width(), bounds.width());
    EXPECT_EQ(child_bounds.height(), bounds.height());
  }

 private:
  enum {
    // The height of an imaginary button in the test.
    kButtonHeight = 20,

    // The horizontal offset of the right-hand button for narrow button layouts.
    kRightButtonXOffset =
        kNarrowButtonMax + ButtonLayout::kPaddingBetweenButtons,

    // The vertical offset of the lower button for wide button layouts.
    kBottomButtonYOffset = kButtonHeight + ButtonLayout::kPaddingBetweenButtons,
  };

  views::View host_;
  ButtonLayout* const layout_;  // Owned by |host_|.
  const int button_1_width_;
  const int button_2_width_;

  DISALLOW_COPY_AND_ASSIGN(ButtonLayoutTest);
};

TEST_P(ButtonLayoutTest, GetPreferredSize) {
  AddChildViews();
  const gfx::Size preferred_size = layout()->GetPreferredSize(host());
  EXPECT_EQ(preferred_size.width(), GetExpectedWidth());
  EXPECT_EQ(preferred_size.height(), GetExpectedHeight());
}

TEST_P(ButtonLayoutTest, Layout) {
  AddChildViews();
  host()->SetBounds(0, 0, GetExpectedWidth(), GetExpectedHeight());
  layout()->Layout(host());

  ExpectViewBoundsEquals(host()->children()[0], GetExpectedButtonBounds(1));
  if (has_two_buttons())
    ExpectViewBoundsEquals(host()->children()[1], GetExpectedButtonBounds(2));
}

// Test all combinations of one or two buttons at many sizes.
INSTANTIATE_TEST_SUITE_P(
    All,
    ButtonLayoutTest,
    ::testing::Combine(::testing::Values(ButtonLayoutTest::kNarrowButtonMin,
                                         ButtonLayoutTest::kNarrowButtonMid,
                                         ButtonLayoutTest::kNarrowButtonMax,
                                         ButtonLayoutTest::kWideButtonMin,
                                         ButtonLayoutTest::kWideButtonMid,
                                         ButtonLayoutTest::kWideButtonMax,
                                         ButtonLayoutTest::kSuperSizedButton),
                       ::testing::Values(ButtonLayoutTest::kNoButton,
                                         ButtonLayoutTest::kNarrowButtonMin,
                                         ButtonLayoutTest::kNarrowButtonMid,
                                         ButtonLayoutTest::kNarrowButtonMax,
                                         ButtonLayoutTest::kWideButtonMin,
                                         ButtonLayoutTest::kWideButtonMid,
                                         ButtonLayoutTest::kWideButtonMax,
                                         ButtonLayoutTest::kSuperSizedButton)));

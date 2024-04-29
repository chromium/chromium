// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"

class TabStripController;

constexpr int kBorderThickness = 4;

class FrameCondensedController : public FakeBaseTabStripController {
 public:
  bool IsFrameCondensed() const override { return condensed_; }

  void set_condensed(bool condensed) { condensed_ = condensed; }

 private:
  bool condensed_ = false;
};

class TabStripControlButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FrameCondensedController>();
    button_ = std::make_unique<TabStripControlButton>(
        tab_strip_controller_.get(), base::BindLambdaForTesting([]() {}), u"");
    button_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets::VH(kBorderThickness, 0)));
    button_->SetSize(button_->CalculatePreferredSize({}));
  }

 protected:
  std::unique_ptr<TabStripControlButton> button_;
  std::unique_ptr<FrameCondensedController> tab_strip_controller_;
};

TEST_F(TabStripControlButtonTest, UncondensedFrameHitTestMask) {
  SkPath path;
  button_->GetHitTestMask(&path);
  SkRegion clip_region;
  clip_region.setRect({0, 0, button_->width(), button_->height()});
  SkRegion mask;
  mask.setPath(path, clip_region);

  const int center_x = button_->width() / 2;
  const int center_y = button_->height() / 2;

  const int top = kBorderThickness;
  const int left = 0;
  const int bottom = button_->height() - kBorderThickness - 1;
  const int right = button_->width() - 1;

  EXPECT_TRUE(mask.contains(center_x, center_y));

  // The points where the circle touches the border are in the mask.
  EXPECT_TRUE(mask.contains(center_x, top));
  EXPECT_TRUE(mask.contains(center_x, bottom));
  EXPECT_TRUE(mask.contains(left, center_y));
  EXPECT_TRUE(mask.contains(right, center_y));

  // The four corners of the border are not in the mask.
  EXPECT_FALSE(mask.contains(left, top));
  EXPECT_FALSE(mask.contains(left, bottom));
  EXPECT_FALSE(mask.contains(right, top));
  EXPECT_FALSE(mask.contains(right, bottom));
}

TEST_F(TabStripControlButtonTest, CondensedFrameHitTestMask) {
  tab_strip_controller_->set_condensed(true);

  SkPath path;
  button_->GetHitTestMask(&path);
  SkRegion clip_region;
  clip_region.setRect({0, 0, button_->width(), button_->height()});
  SkRegion mask;
  mask.setPath(path, clip_region);

  const int center_x = button_->width() / 2;
  const int center_y = button_->height() / 2;

  const int top = kBorderThickness;
  const int left = 0;
  const int bottom = button_->height() - kBorderThickness - 1;
  const int right = button_->width() - 1;

  EXPECT_TRUE(mask.contains(center_x, center_y));

  // The points where the circle touches the border are in the mask.
  EXPECT_TRUE(mask.contains(center_x, top));
  EXPECT_TRUE(mask.contains(center_x, bottom));
  EXPECT_TRUE(mask.contains(left, center_y));
  EXPECT_TRUE(mask.contains(right, center_y));

  // The entire top of the view (including the border!) is in the mask.
  EXPECT_TRUE(mask.contains(center_x, 0));
  EXPECT_TRUE(mask.contains(left, 0));
  EXPECT_TRUE(mask.contains(right, 0));

  // The bottom corners of the border are not in the mask.
  EXPECT_FALSE(mask.contains(right, bottom));
  EXPECT_FALSE(mask.contains(left, bottom));
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_button.h"

#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProductSpecificationsButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, &profile_);
    locked_expansion_view_ = std::make_unique<views::View>();
    button_ = std::make_unique<ProductSpecificationsButton>(
        tab_strip_controller_.get(), tab_strip_model_.get(), true,
        locked_expansion_view_.get());
  }

  void SetWidthFactor(float factor) { button_->SetWidthFactor(factor); }

 protected:
  std::unique_ptr<TabStripController> tab_strip_controller_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  TestingProfile profile_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<views::View> locked_expansion_view_;
  std::unique_ptr<ProductSpecificationsButton> button_;
};

TEST_F(ProductSpecificationsButtonTest, AppliesWidthFactor) {
  ASSERT_EQ(0, button_->width_factor_for_testing());
  ASSERT_EQ(0, button_->CalculatePreferredSize({}).width());

  SetWidthFactor(0.5);

  const int half_width = button_->CalculatePreferredSize({}).width();
  ASSERT_LT(0, half_width);

  SetWidthFactor(1);

  const int full_width = button_->CalculatePreferredSize({}).width();
  const int half_full_width = full_width / 2;
  ASSERT_LT(0, full_width);
  ASSERT_EQ(half_width, half_full_width);
}

TEST_F(ProductSpecificationsButtonTest, AnimatesToExpanded) {
  // Should be collapsed by default
  ASSERT_EQ(0, button_->expansion_animation_for_testing()->GetCurrentValue());

  ASSERT_EQ(0, button_->width_factor_for_testing());

  button_->Show();

  ASSERT_TRUE(button_->expansion_animation_for_testing()->IsShowing());

  button_->expansion_animation_for_testing()->Reset(1);

  ASSERT_EQ(1, button_->width_factor_for_testing());
}

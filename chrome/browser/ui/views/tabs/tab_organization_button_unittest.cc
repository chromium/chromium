// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_organization_button.h"

#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "fake_base_tab_strip_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabOrganizationButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    auto controller = std::make_unique<FakeBaseTabStripController>();
    auto tab_strip = std::make_unique<TabStrip>(std::move(controller));
    button_ =
        std::make_unique<TabOrganizationButton>(tab_strip.get(), Edge::kRight);
  }

 protected:
  std::unique_ptr<TabOrganizationButton> button_;
};

TEST_F(TabOrganizationButtonTest, AppliesWidthFactor) {
  ASSERT_EQ(0, button_->width_factor_for_testing());
  ASSERT_EQ(0, button_->CalculatePreferredSize().width());

  button_->SetWidthFactor(0.5);

  const float half_width = button_->CalculatePreferredSize().width();
  ASSERT_LT(0, half_width);

  button_->SetWidthFactor(1);

  const float full_width = button_->CalculatePreferredSize().width();
  ASSERT_LT(0, full_width);
  ASSERT_EQ(half_width, full_width / 2);
}

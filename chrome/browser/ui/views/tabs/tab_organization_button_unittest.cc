// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_organization_button.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class TabOrganizationButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    button_ = std::make_unique<TabOrganizationButton>(
        tab_strip_controller_.get(),
        base::BindRepeating(&TabOrganizationButtonTest::MockButtonCallback,
                            base::Unretained(this)),
        base::BindRepeating(&TabOrganizationButtonTest::MockButtonCallback,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(IDS_TAB_ORGANIZE),
        l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE),
        l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE),
        kAutoTabGroupButtonElementId, Edge::kRight);
  }

  void MockButtonCallback() { button_callback_count_++; }

 protected:
  std::unique_ptr<TabOrganizationButton> button_;
  std::unique_ptr<TabStripController> tab_strip_controller_;
  int button_callback_count_ = 0;
};

TEST_F(TabOrganizationButtonTest, AppliesWidthFactor) {
  ASSERT_EQ(0, button_->width_factor_for_testing());
  ASSERT_EQ(0, button_->CalculatePreferredSize({}).width());

  button_->SetWidthFactor(0.5);

  const int half_width = button_->CalculatePreferredSize({}).width();
  ASSERT_LT(0, half_width);

  button_->SetWidthFactor(1);

  const int full_width = button_->CalculatePreferredSize({}).width();
  const int half_full_width = full_width / 2;
  ASSERT_LT(0, full_width);
  ASSERT_EQ(half_width, half_full_width);
}

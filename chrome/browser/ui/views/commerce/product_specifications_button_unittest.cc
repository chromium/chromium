// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_button.h"

#include <memory>

#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class MockProductSpecificationsEntryPointController
    : public commerce::ProductSpecificationsEntryPointController {
 public:
  explicit MockProductSpecificationsEntryPointController(
      BrowserWindowInterface* browser)
      : commerce::ProductSpecificationsEntryPointController(browser) {}
  ~MockProductSpecificationsEntryPointController() override = default;

  MOCK_METHOD(bool, ShouldExecuteEntryPointShow, (), (override));
};

class ProductSpecificationsButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
    profile_ = profile_builder.Build();
    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_, profile_.get());
    EXPECT_CALL(*browser_window_interface_, GetTabStripModel())
        .WillRepeatedly(testing::Return(tab_strip_model_.get()));
    EXPECT_CALL(*browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(profile_.get()));
    EXPECT_CALL(*browser_window_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    entry_point_controller_ =
        std::make_unique<MockProductSpecificationsEntryPointController>(
            browser_window_interface_.get());
    locked_expansion_view_ = std::make_unique<views::View>();
    button_ = std::make_unique<ProductSpecificationsButton>(
        browser_window_interface_.get(), entry_point_controller_.get(), true,
        locked_expansion_view_.get());
    ON_CALL(*entry_point_controller_, ShouldExecuteEntryPointShow)
        .WillByDefault(testing::Return(true));
  }

  void SetWidthFactor(float factor) { button_->SetWidthFactor(factor); }

  void ShowButton() { button_->Show(); }

 protected:
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<MockProductSpecificationsEntryPointController>
      entry_point_controller_;
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

  ShowButton();

  ASSERT_TRUE(button_->expansion_animation_for_testing()->IsShowing());

  button_->expansion_animation_for_testing()->Reset(1);

  ASSERT_EQ(1, button_->width_factor_for_testing());
}

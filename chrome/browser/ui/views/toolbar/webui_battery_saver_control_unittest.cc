// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_battery_saver_control.h"

#include <memory>

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/mock_webui_toolbar_control_delegate.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class WebUIBatterySaverControlTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    browser_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();

    ON_CALL(*browser_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    user_education_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserUserEducationInterface>>(
            browser_interface_.get());

    delegate_ =
        std::make_unique<testing::NiceMock<MockWebUIToolbarControlDelegate>>();
    ON_CALL(*delegate_, GetBrowser())
        .WillByDefault(testing::Return(browser_interface_.get()));
    control_ = std::make_unique<WebUIBatterySaverControl>(delegate_.get());
  }

  void TearDown() override {
    control_.reset();
    delegate_.reset();
    user_education_interface_.reset();
    browser_interface_.reset();
    testing::Test::TearDown();
  }

 protected:
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_interface_;
  std::unique_ptr<testing::NiceMock<MockBrowserUserEducationInterface>>
      user_education_interface_;
  std::unique_ptr<testing::NiceMock<MockWebUIToolbarControlDelegate>> delegate_;
  std::unique_ptr<WebUIBatterySaverControl> control_;
};

TEST_F(WebUIBatterySaverControlTest, ShowAndHide) {
  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(1);
  EXPECT_CALL(
      *delegate_,
      OnBatterySaverControlStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::BatterySaverControlState::should_be_shown,
          true))));
  control_->Show();

  EXPECT_CALL(*delegate_, OnPreferredSizeChanged()).Times(1);
  EXPECT_CALL(
      *delegate_,
      OnBatterySaverControlStateChanged(testing::Pointee(testing::Field(
          &toolbar_ui_api::mojom::BatterySaverControlState::should_be_shown,
          false))));
  control_->Hide();
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"

#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

class PasswordChangeBubbleControllerTest : public ::testing::Test {
 public:
  PasswordChangeBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  void CreateController() {
    EXPECT_CALL(*mock_delegate_, OnBubbleShown());
    password_change_delegate_ = std::make_unique<PasswordChangeDelegateMock>();
    ON_CALL(*mock_delegate_, GetPasswordChangeDelegate)
        .WillByDefault(Return(password_change_delegate_.get()));
    controller_ = std::make_unique<PasswordChangeInfoBubbleController>(
        mock_delegate_->AsWeakPtr(),
        PasswordChangeDelegate::State::kWaitingForChangePasswordForm);
  }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  PasswordChangeInfoBubbleController* controller() { return controller_.get(); }
  PasswordChangeDelegateMock* password_change_delegate() {
    return password_change_delegate_.get();
  }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<PasswordChangeInfoBubbleController> controller_;
  std::unique_ptr<PasswordChangeDelegateMock> password_change_delegate_;
};

TEST_F(PasswordChangeBubbleControllerTest, ControllerDestroyed) {
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
}

TEST_F(PasswordChangeBubbleControllerTest, CancelsFlow) {
  CreateController();
  ON_CALL(*password_change_delegate(), GetCurrentState)
      .WillByDefault(
          Return(PasswordChangeDelegate::State::kWaitingForChangePasswordForm));

  EXPECT_CALL(*password_change_delegate(), Stop);
  controller()->CancelPasswordChange();
}

TEST_F(PasswordChangeBubbleControllerTest, OpensPasswordManagerPage) {
  CreateController();
  EXPECT_CALL(*delegate(), NavigateToPasswordManagerSettingsPage(
                               password_manager::ManagePasswordsReferrer::
                                   kPasswordChangeInfoBubble));
  controller()->OnGooglePasswordManagerLinkClicked();
}

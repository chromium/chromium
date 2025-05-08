// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/otp_during_password_change_bubble_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

class OtpDuringPasswordChangeBubbleControllerTest : public ::testing::Test {
 public:
  PasswordsModelDelegateMock& mock_delegate() { return mock_delegate_; }
  PasswordChangeDelegateMock& password_change_delegate() {
    return password_change_delegate_;
  }

 private:
  PasswordsModelDelegateMock mock_delegate_;
  PasswordChangeDelegateMock password_change_delegate_;
};

TEST_F(OtpDuringPasswordChangeBubbleControllerTest, FixManually) {
  EXPECT_CALL(mock_delegate(), GetPasswordChangeDelegate)
      .WillRepeatedly(Return(&password_change_delegate()));
  OtpDuringPasswordChangeBubbleController controller(
      mock_delegate().AsWeakPtr());

  EXPECT_CALL(password_change_delegate(), OpenPasswordChangeTab);
  EXPECT_CALL(password_change_delegate(), Stop);

  controller.FixManually();
}

TEST_F(OtpDuringPasswordChangeBubbleControllerTest, FinishPasswordChange) {
  EXPECT_CALL(mock_delegate(), GetPasswordChangeDelegate)
      .WillRepeatedly(Return(&password_change_delegate()));
  OtpDuringPasswordChangeBubbleController controller(
      mock_delegate().AsWeakPtr());

  EXPECT_CALL(password_change_delegate(), Stop);

  controller.FinishPasswordChange();
}

TEST_F(OtpDuringPasswordChangeBubbleControllerTest,
       NavigateToPasswordChangeSettings) {
  EXPECT_CALL(mock_delegate(), GetPasswordChangeDelegate)
      .WillRepeatedly(Return(&password_change_delegate()));
  OtpDuringPasswordChangeBubbleController controller(
      mock_delegate().AsWeakPtr());

  EXPECT_CALL(mock_delegate(), NavigateToPasswordChangeSettings);

  controller.NavigateToPasswordChangeSettings();
}

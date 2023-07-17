// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/shared_passwords_notifications_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SharedPasswordsNotificationBubbleControllerTest : public ::testing::Test {
 public:
  SharedPasswordsNotificationBubbleControllerTest() {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    controller_ = std::make_unique<SharedPasswordsNotificationBubbleController>(
        mock_delegate_->AsWeakPtr());
  }
  ~SharedPasswordsNotificationBubbleControllerTest() override = default;

  void SetUp() override {
    ON_CALL(*delegate(), GetCurrentForms)
        .WillByDefault(testing::ReturnRef(current_forms_));
  }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  SharedPasswordsNotificationBubbleController* controller() {
    return controller_.get();
  }

 private:
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<SharedPasswordsNotificationBubbleController> controller_;
};

TEST_F(SharedPasswordsNotificationBubbleControllerTest, HasTitle) {
  EXPECT_FALSE(controller()->GetTitle().empty());
}

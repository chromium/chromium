// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/biometric_authentication_confirmation_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BiometricAuthenticationConfirmationBubbleControllerTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  ~BiometricAuthenticationConfirmationBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  BiometricAuthenticationConfirmationBubbleController* controller() {
    return controller_.get();
  }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ =
        std::make_unique<BiometricAuthenticationConfirmationBubbleController>(
            mock_delegate_->AsWeakPtr());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<BiometricAuthenticationConfirmationBubbleController>
      controller_;
};

TEST_F(BiometricAuthenticationConfirmationBubbleControllerTest, Destroy) {
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
}

TEST_F(BiometricAuthenticationConfirmationBubbleControllerTest,
       DestroyImplicictly) {
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

TEST_F(BiometricAuthenticationConfirmationBubbleControllerTest, Content) {
  CreateController();
  EXPECT_NE(std::u16string(), controller()->GetTitle());
  EXPECT_EQ(IDR_BIOMETRIC_AUTHENTICATION_CONFIRMATION_PROMPT_DARK,
            controller()->GetImageID(true));
  EXPECT_EQ(IDR_BIOMETRIC_AUTHENTICATION_CONFIRMATION_PROMPT_LIGHT,
            controller()->GetImageID(false));
}

TEST_F(BiometricAuthenticationConfirmationBubbleControllerTest,
       SettingsLinkClick) {
  CreateController();
  EXPECT_CALL(*delegate(),
              NavigateToPasswordManagerSettingsPage(
                  password_manager::ManagePasswordsReferrer::
                      kBiometricAuthenticationBeforeFillingDialog));
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnNavigateToSettingsLinkClicked();
}

}  // namespace

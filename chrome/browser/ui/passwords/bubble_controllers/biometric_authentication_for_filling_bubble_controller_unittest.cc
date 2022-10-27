// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/biometric_authentication_for_filling_bubble_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kAcceptHistogram[] =
    "PasswordBubble.BiometricAuthenticationPromo.AcceptClicked";
constexpr char kAuthResultHistogram[] =
    "PasswordManager.BiometricAuthenticationPromo.AuthenticationResult";

class BiometricAuthenticationForFillingBubbleControllerTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_pref_service_->registry()->RegisterBooleanPref(
        password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo,
        false);
    test_pref_service_->registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }
  ~BiometricAuthenticationForFillingBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  BiometricAuthenticationForFillingBubbleController* controller() {
    return controller_.get();
  }

  TestingPrefServiceSimple* test_pref_service() {
    return test_pref_service_.get();
  }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ =
        std::make_unique<BiometricAuthenticationForFillingBubbleController>(
            mock_delegate_->AsWeakPtr(), test_pref_service_.get(),
            PasswordBubbleControllerBase::DisplayReason::kAutomatic);
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

  void ResetController() { controller_.reset(); }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<BiometricAuthenticationForFillingBubbleController>
      controller_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
};

TEST_F(BiometricAuthenticationForFillingBubbleControllerTest, Destroy) {
  base::HistogramTester histograms;
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
  histograms.ExpectUniqueSample(kAcceptHistogram, false, 1);
}

TEST_F(BiometricAuthenticationForFillingBubbleControllerTest,
       DestroyImplicictly) {
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

TEST_F(BiometricAuthenticationForFillingBubbleControllerTest, Content) {
  CreateController();
  EXPECT_NE(std::u16string(), controller()->GetBody());
  EXPECT_NE(std::u16string(), controller()->GetContinueButtonText());
  EXPECT_NE(std::u16string(), controller()->GetNoThanksButtonText());
  EXPECT_EQ(IDR_BIOMETRIC_AUTHENTICATION_PROMPT_DARK,
            controller()->GetImageID(true));
  EXPECT_EQ(IDR_BIOMETRIC_AUTHENTICATION_PROMPT_LIGHT,
            controller()->GetImageID(false));
}

TEST_F(BiometricAuthenticationForFillingBubbleControllerTest, Cancel) {
  CreateController();

  EXPECT_CALL(*delegate(), OnBiometricAuthBeforeFillingDeclined);
  controller()->OnCanceled();
  EXPECT_FALSE(test_pref_service()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
  EXPECT_TRUE(test_pref_service()->GetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo));
}

TEST_F(BiometricAuthenticationForFillingBubbleControllerTest,
       OnAcceptedFailure) {
  base::HistogramTester histograms;
  CreateController();

  EXPECT_CALL(*delegate(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>(
          [](auto callback) { std::move(callback).Run(/*success=*/false); }));
  controller()->OnAccepted();

  ResetController();
  EXPECT_FALSE(test_pref_service()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
  EXPECT_FALSE(test_pref_service()->GetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo));
  histograms.ExpectUniqueSample(kAcceptHistogram, true, 1);
  histograms.ExpectUniqueSample(kAuthResultHistogram, false, 1);
}

TEST_F(BiometricAuthenticationForFillingBubbleControllerTest,
       OnAcceptedSuccess) {
  base::HistogramTester histograms;
  CreateController();

  EXPECT_CALL(*delegate(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>(
          [](auto callback) { std::move(callback).Run(/*success=*/true); }));
  EXPECT_CALL(*delegate(), ShowBiometricActivationConfirmation);
  controller()->OnAccepted();

  ResetController();
  EXPECT_TRUE(test_pref_service()->GetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling));
  EXPECT_TRUE(test_pref_service()->GetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo));
  histograms.ExpectUniqueSample(kAcceptHistogram, true, 1);
  histograms.ExpectUniqueSample(kAuthResultHistogram, true, 1);
}

}  // namespace

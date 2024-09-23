// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_not_accepted_bubble_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ::testing::Return;

constexpr char kUIDismissalReasonMetric[] = "PasswordManager.UIDismissalReason";

class PasskeyNotAcceptedBubbleControllerTest : public ::testing::Test {
 public:
  ~PasskeyNotAcceptedBubbleControllerTest() override = default;

  void SetUp() override {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  PasskeyNotAcceptedBubbleController* controller() { return controller_.get(); }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<PasskeyNotAcceptedBubbleController>(
        mock_delegate_->AsWeakPtr(),
        password_manager::metrics_util::AUTOMATIC_PASSKEY_NOT_ACCEPTED_BUBBLE);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

  void DestroyController() { controller_.reset(); }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<PasskeyNotAcceptedBubbleController> controller_;
};

TEST_F(PasskeyNotAcceptedBubbleControllerTest, DestroyImplicictly) {
  CreateController();
  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

TEST_F(PasskeyNotAcceptedBubbleControllerTest,
       OnGooglePasswordManagerButtonClicked) {
  base::HistogramTester histogram_tester;
  CreateController();
  EXPECT_CALL(*delegate(), NavigateToPasswordManagerSettingsPage(
                               password_manager::ManagePasswordsReferrer::
                                   kPasskeyNotAcceptedBubble));
  controller()->OnGooglePasswordManagerLinkClicked();
  DestroyController();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric, password_manager::metrics_util::CLICKED_MANAGE,
      1);
}

}  // namespace

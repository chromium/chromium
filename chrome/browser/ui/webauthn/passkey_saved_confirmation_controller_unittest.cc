// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_saved_confirmation_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ::testing::Return;

constexpr char kUIDismissalReasonMetric[] = "PasswordManager.UIDismissalReason";

}  // namespace

class PasskeySavedConfirmationControllerTest : public ::testing::Test {
 public:
  ~PasskeySavedConfirmationControllerTest() override = default;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactories(
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    profile_ = profile_builder.Build();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    ON_CALL(*mock_delegate_, GpmPinCreatedDuringRecentPasskeyCreation)
        .WillByDefault(Return(false));
  }

  TestingProfile* profile() { return profile_.get(); }
  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  PasskeySavedConfirmationController* controller() { return controller_.get(); }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<PasskeySavedConfirmationController>(
        mock_delegate_->AsWeakPtr());
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

  void DestroyController() { controller_.reset(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<PasskeySavedConfirmationController> controller_;
};

TEST_F(PasskeySavedConfirmationControllerTest, Destroy) {
  CreateController();
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
}

TEST_F(PasskeySavedConfirmationControllerTest, DestroyImplicictly) {
  CreateController();
  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

TEST_F(PasskeySavedConfirmationControllerTest, ContentWithoutPinCreation) {
  CreateController();
  EXPECT_EQ(controller()->GetTitle(),
            l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PASSKEY_SAVED_TITLE));
}

TEST_F(PasskeySavedConfirmationControllerTest, ContentWithPinCreation) {
  ON_CALL(*delegate(), GpmPinCreatedDuringRecentPasskeyCreation)
      .WillByDefault(Return(true));
  CreateController();
  EXPECT_EQ(controller()->GetTitle(),
            l10n_util::GetStringUTF16(
                IDS_WEBAUTHN_GPM_PASSKEY_SAVED_PIN_CREATED_TITLE));
}

TEST_F(PasskeySavedConfirmationControllerTest,
       OnGooglePasswordManagerLinkClicked) {
  base::HistogramTester histogram_tester;
  CreateController();
  EXPECT_CALL(*delegate(), NavigateToPasswordManagerSettingsPage(
                               password_manager::ManagePasswordsReferrer::
                                   kPasskeySavedConfirmationBubble));
  controller()->OnGooglePasswordManagerLinkClicked();
  DestroyController();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonMetric, password_manager::metrics_util::CLICKED_MANAGE,
      1);
}

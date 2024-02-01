// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/default_store_changed_bubble_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace {
constexpr char kContinueHistogram[] =
    "PasswordBubble.DefaultStoreChangedBubble.ContinueButtonInBubbleClicked";
}

class DefaultStoreChangedBubbleControllerTest : public ::testing::Test {
 public:
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
    ON_CALL(*mock_delegate_, GetPasswordFeatureManager)
        .WillByDefault(Return(&password_feature_manager_));
    ON_CALL(password_feature_manager_, GetDefaultPasswordStore)
        .WillByDefault(
            Return(password_manager::PasswordForm::Store::kProfileStore));
  }
  ~DefaultStoreChangedBubbleControllerTest() override = default;

  TestingProfile* profile() { return profile_.get(); }
  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  DefaultStoreChangedBubbleController* controller() {
    return controller_.get();
  }

  password_manager::MockPasswordFeatureManager* password_feature_manager() {
    return &password_feature_manager_;
  }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    EXPECT_CALL(*password_feature_manager(),
                SetDefaultPasswordStore(
                    password_manager::PasswordForm::Store::kAccountStore));

    controller_ = std::make_unique<DefaultStoreChangedBubbleController>(
        mock_delegate_->AsWeakPtr());
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<password_manager::MockPasswordFeatureManager>
      password_feature_manager_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<DefaultStoreChangedBubbleController> controller_;
};

TEST_F(DefaultStoreChangedBubbleControllerTest, Destroy) {
  base::HistogramTester histograms;
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
  histograms.ExpectUniqueSample(kContinueHistogram, false, 1);
}

TEST_F(DefaultStoreChangedBubbleControllerTest, DestroyImplicictly) {
  CreateController();

  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

TEST_F(DefaultStoreChangedBubbleControllerTest, Content) {
  CreateController();
  EXPECT_NE(std::u16string(), controller()->GetTitle());
  EXPECT_NE(std::u16string(), controller()->GetBody());
  EXPECT_NE(std::u16string(), controller()->GetContinueButtonText());
  EXPECT_NE(std::u16string(), controller()->GetGoToSettingsButtonText());
}

TEST_F(DefaultStoreChangedBubbleControllerTest, SettingsLinkClick) {
  base::HistogramTester histograms;
  CreateController();
  EXPECT_CALL(*delegate(), NavigateToPasswordManagerSettingsAccountStoreToggle(
                               password_manager::ManagePasswordsReferrer::
                                   kDefaultStoreChangedBubble));
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnNavigateToSettingsButtonClicked();
  controller()->OnBubbleClosing();
  histograms.ExpectUniqueSample(kContinueHistogram, false, 1);
}

TEST_F(DefaultStoreChangedBubbleControllerTest, ContinueButtonClick) {
  base::HistogramTester histograms;
  CreateController();
  EXPECT_CALL(*delegate(), PromptSaveBubbleAfterDefaultStoreChanged);
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnContinueButtonClicked();
  controller()->OnBubbleClosing();
  histograms.ExpectUniqueSample(kContinueHistogram, true, 1);
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using SigninInterceptionType = WebSigninInterceptor::SigninInterceptionType;

class DiceWebSigninInterceptionBubbleViewTestBase : public testing::Test {
 public:
  DiceWebSigninInterceptionBubbleViewTestBase() {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    signin::IdentityTestEnvironment* identity_test_env =
        identity_test_env_adaptor_->identity_test_env();

    enterprise_account_ =
        identity_test_env->MakeAccountAvailable("bob@example.com");
    enterprise_account_.hosted_domain = "example.com";
    identity_test_env->UpdateAccountInfoForAccount(enterprise_account_);
    personal_account_ =
        identity_test_env->MakeAccountAvailable("alice@gmail.com");
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_adaptor_->identity_test_env()->identity_manager();
  }

  Profile* profile() { return profile_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  AccountInfo enterprise_account_;
  AccountInfo personal_account_;
};

class DiceWebSigninInterceptionBubbleViewSyncParamTest
    : public DiceWebSigninInterceptionBubbleViewTestBase,
      public testing::WithParamInterface<
          std::tuple<SigninInterceptionType, SigninInterceptionResult>> {};

TEST_P(DiceWebSigninInterceptionBubbleViewSyncParamTest, HistogramTests) {
  SigninInterceptionType type = std::get<0>(GetParam());
  SigninInterceptionResult result = std::get<1>(GetParam());

  base::HistogramTester histogram_tester;

  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
      type, enterprise_account_, personal_account_);

  DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
      bubble_parameters, profile(), result);

  // Check enterprise histograms.
  if (type == SigninInterceptionType::kEnterprise) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.Enterprise",
                                        result, 1);
  } else {
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise", 0);
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise.Sync",
                                      0);
    histogram_tester.ExpectTotalCount(
        "Signin.InterceptResult.Enterprise.NoSync", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.InterceptResult.Enterprise.NewIsEnterprise", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.InterceptResult.Enterprise.PrimaryIsEnterprise", 0);
  }

  // Check multi-user histograms.
  if (type == SigninInterceptionType::kMultiUser) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                        result, 1);
  } else {
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.MultiUser", 0);
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.MultiUser.Sync",
                                      0);
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.MultiUser.NoSync",
                                      0);
  }

  // Check switch histograms.
  if (type == SigninInterceptionType::kProfileSwitch) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.Switch", result,
                                        1);
  } else {
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch", 0);
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch.Sync", 0);
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.Switch.NoSync",
                                      0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    BubbleParameterCombinations,
    DiceWebSigninInterceptionBubbleViewSyncParamTest,
    testing::Combine(testing::ValuesIn({
                         SigninInterceptionType::kEnterprise,
                         SigninInterceptionType::kMultiUser,
                         SigninInterceptionType::kProfileSwitch,
                     }),
                     testing::ValuesIn({
                         SigninInterceptionResult::kAccepted,
                         SigninInterceptionResult::kDeclined,
                         SigninInterceptionResult::kIgnored,
                         SigninInterceptionResult::kNotDisplayed,
                     })));

TEST_F(DiceWebSigninInterceptionBubbleViewTestBase, SyncHistograms) {
  SigninInterceptionResult result = SigninInterceptionResult::kAccepted;
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
      SigninInterceptionType::kEnterprise, enterprise_account_,
      personal_account_);

  // Not Syncing.
  {
    base::HistogramTester histogram_tester;
    DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
        bubble_parameters, profile(), result);
    histogram_tester.ExpectTotalCount("Signin.InterceptResult.Enterprise.Sync",
                                      0);
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.Enterprise.NoSync", result, 1);
  }

  // Syncing.
  identity_test_env()->SetPrimaryAccount(personal_account_.email,
                                         signin::ConsentLevel::kSync);
  {
    base::HistogramTester histogram_tester;
    DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
        bubble_parameters, profile(), result);
    histogram_tester.ExpectTotalCount(
        "Signin.InterceptResult.Enterprise.NoSync", 0);
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.Enterprise.Sync", result, 1);
  }
}

TEST_F(DiceWebSigninInterceptionBubbleViewTestBase, EnterpriseHistograms) {
  SigninInterceptionResult result = SigninInterceptionResult::kAccepted;

  // New account is Enterprise.
  {
    base::HistogramTester histogram_tester;
    WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
        SigninInterceptionType::kEnterprise, enterprise_account_,
        personal_account_);
    DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
        bubble_parameters, profile(), result);
    histogram_tester.ExpectTotalCount(
        "Signin.InterceptResult.Enterprise.PrimaryIsEnterprise", 0);
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.Enterprise.NewIsEnterprise", result, 1);
  }

  // Primary account is Enterprise.
  identity_test_env()->SetPrimaryAccount(personal_account_.email,
                                         signin::ConsentLevel::kSync);
  {
    base::HistogramTester histogram_tester;
    WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
        SigninInterceptionType::kEnterprise, personal_account_,
        enterprise_account_);
    DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
        bubble_parameters, profile(), result);
    histogram_tester.ExpectTotalCount(
        "Signin.InterceptResult.Enterprise.NewIsEnterprise", 0);
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.Enterprise.PrimaryIsEnterprise", result, 1);
  }
}

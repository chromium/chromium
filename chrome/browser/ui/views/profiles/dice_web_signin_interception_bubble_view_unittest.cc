// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using SigninInterceptionType = WebSigninInterceptor::SigninInterceptionType;

namespace {

// Helper function to provide a readable test case name.
std::string SigninInterceptTypeToString(SigninInterceptionType type) {
  switch (type) {
    case SigninInterceptionType::kEnterprise:
      return "Entreprise";
    case SigninInterceptionType::kMultiUser:
      return "MultiUser";
    case SigninInterceptionType::kProfileSwitch:
      return "ProfileSwitch";
    case SigninInterceptionType::kChromeSignin:
      return "ChromeSignin";
    default:
      NOTREACHED_IN_MIGRATION()
          << "Interception type not supported in the tests.";
      return std::string();
  }
}

// Helper function to provide a readable test case name.
std::string SigninInterceptResultToString(SigninInterceptionResult result) {
  switch (result) {
    case SigninInterceptionResult::kAccepted:
      return "Accepted";
    case SigninInterceptionResult::kDeclined:
      return "Declined";
    case SigninInterceptionResult::kIgnored:
      return "Ignored";
    case SigninInterceptionResult::kDismissed:
      return "Dismissed";
    case SigninInterceptionResult::kNotDisplayed:
      return "NotDisplayed";
    default:
      NOTREACHED_IN_MIGRATION()
          << "Interception result not supported in the tests.";
      return std::string();
  }
}

}  // namespace

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
    personal_account_2_ =
        identity_test_env->MakeAccountAvailable("carol@gmail.com");
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
  AccountInfo personal_account_2_;
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

  base::HistogramTester::CountsMap expected_counts;

  // Check enterprise histograms.
  if (type == SigninInterceptionType::kEnterprise) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.Enterprise",
                                        result, 1);
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.Enterprise.NewIsEnterprise", result, 1);
    expected_counts["Signin.InterceptResult.Enterprise"] = 1;
    expected_counts["Signin.InterceptResult.Enterprise.NoSync"] = 1;
    expected_counts["Signin.InterceptResult.Enterprise.NewIsEnterprise"] = 1;
  }

  // Check multi-user histograms.
  if (type == SigninInterceptionType::kMultiUser) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                        result, 1);
    expected_counts["Signin.InterceptResult.MultiUser"] = 1;
    expected_counts["Signin.InterceptResult.MultiUser.NoSync"] = 1;
  }

  // Check switch histograms.
  if (type == SigninInterceptionType::kProfileSwitch) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.Switch", result,
                                        1);
    expected_counts["Signin.InterceptResult.Switch"] = 1;
    expected_counts["Signin.InterceptResult.Switch.NoSync"] = 1;
  }

  // Check ChromeSignin histograms.
  if (type == SigninInterceptionType::kChromeSignin) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.ChromeSignin",
                                        result, 1);
    expected_counts["Signin.InterceptResult.ChromeSignin"] = 1;
    expected_counts["Signin.InterceptResult.ChromeSignin.NoSync"] = 1;
  }

  // Make sure no other histogram are recorded.
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.InterceptResult"),
      testing::ContainerEq(expected_counts));
}

INSTANTIATE_TEST_SUITE_P(
    BubbleParameterCombinations,
    DiceWebSigninInterceptionBubbleViewSyncParamTest,
    testing::Combine(testing::ValuesIn({
                         SigninInterceptionType::kEnterprise,
                         SigninInterceptionType::kMultiUser,
                         SigninInterceptionType::kProfileSwitch,
                         SigninInterceptionType::kChromeSignin,
                     }),
                     testing::ValuesIn({
                         SigninInterceptionResult::kAccepted,
                         SigninInterceptionResult::kDeclined,
                         SigninInterceptionResult::kIgnored,
                         SigninInterceptionResult::kDismissed,
                         SigninInterceptionResult::kNotDisplayed,
                     })),
    [](const testing::TestParamInfo<
        DiceWebSigninInterceptionBubbleViewSyncParamTest::ParamType>& info) {
      return SigninInterceptTypeToString(std::get<0>(info.param)) + "_" +
             SigninInterceptResultToString(std::get<1>(info.param));
    });

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

TEST_F(DiceWebSigninInterceptionBubbleViewTestBase, SigninPendingHistograms) {
  // The primary account is in sign in pending state. We are already signed into
  // web with different account, therefore inducing an inconsistent state.
  identity_test_env()->SetPrimaryAccount(personal_account_.email,
                                         signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  {
    base::HistogramTester histogram_tester;
    SigninInterceptionResult result = SigninInterceptionResult::kAccepted;
    WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
        SigninInterceptionType::kMultiUser, personal_account_2_,
        personal_account_);
    DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
        bubble_parameters, profile(), result);
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.MultiUser.SigninPending", result, 1);
  }

  {
    base::HistogramTester histogram_tester;
    SigninInterceptionResult result = SigninInterceptionResult::kDismissed;
    WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
        SigninInterceptionType::kMultiUser, personal_account_2_,
        personal_account_);
    DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
        bubble_parameters, profile(), result);
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.MultiUser.SigninPending", result, 1);
  }
}

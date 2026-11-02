// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <optional>
#include <string_view>
#include <tuple>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/data_type.h"
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
    case SigninInterceptionType::kChromeSigninNonIntercept:
      return "ChromeSigninNonIntercept";
    default:
      NOTREACHED() << "Interception type not supported in the tests.";
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
      NOTREACHED() << "Interception result not supported in the tests.";
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

    AccountInfo account_info =
        identity_test_env->MakeAccountAvailable("bob@example.com");
    enterprise_account_ = AccountInfo::Builder(account_info)
                              .SetHostedDomain("example.com")
                              .Build();
    AccountCapabilitiesTestMutator(&enterprise_account_)
        .set_is_subject_to_enterprise_features(true);
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
    expected_counts["Signin.InterceptResult.Enterprise"] = 1;
    expected_counts["Signin.InterceptResult.Enterprise.NoSync"] = 1;
  }

  // Check multi-user histograms.
  if (type == SigninInterceptionType::kMultiUser) {
    histogram_tester.ExpectUniqueSample("Signin.InterceptResult.MultiUser",
                                        result, 1);
    expected_counts["Signin.InterceptResult.MultiUser"] = 1;
    expected_counts["Signin.InterceptResult.MultiUser.NoSync"] = 1;
    expected_counts["Signin.InterceptResult.MultiUser.NotPersonalized"] = 1;
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
    expected_counts["Signin.InterceptResult.ChromeSignin.NotPersonalized"] = 1;
  }

  // Check ChromeSigninNonIntercept histograms.
  if (type == SigninInterceptionType::kChromeSigninNonIntercept) {
    histogram_tester.ExpectUniqueSample(
        "Signin.InterceptResult.ChromeSigninNonIntercept", result, 1);
    expected_counts["Signin.InterceptResult.ChromeSigninNonIntercept"] = 1;
    expected_counts["Signin.InterceptResult.ChromeSigninNonIntercept.NoSync"] =
        1;
    expected_counts
        ["Signin.InterceptResult.ChromeSigninNonIntercept.NotPersonalized"] = 1;
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
                         SigninInterceptionType::kChromeSigninNonIntercept,
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
  identity_test_env()->SetPrimaryAccount(
      std::string(personal_account_.GetEmail()), signin::ConsentLevel::kSync);
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

TEST_F(DiceWebSigninInterceptionBubbleViewTestBase, SigninPendingHistograms) {
  // The primary account is in sign in pending state. We are already signed into
  // web with different account, therefore inducing an inconsistent state.
  identity_test_env()->SetPrimaryAccount(
      std::string(personal_account_.GetEmail()), signin::ConsentLevel::kSignin);
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

struct AccountPreviewHistogramTestCase {
  std::string_view test_suffix;
  base::TimeDelta fetch_duration;
  std::optional<syncer::DataType> preferred_data_type;
  SigninInterceptionResult result;
  std::string_view expected_delay_variant;
  std::string_view expected_personalized_variant;
};

constexpr AccountPreviewHistogramTestCase kAccountPreviewHistogramTestCases[] =
    {
        {"ShortDelay_Personalized", base::Milliseconds(100), syncer::BOOKMARKS,
         SigninInterceptionResult::kAccepted, ".ShortDelay", ".Personalized"},
        {"ShortDelay_UnsupportedDataType", base::Milliseconds(299),
         syncer::HISTORY, SigninInterceptionResult::kDeclined, ".ShortDelay",
         ".NotPersonalized"},
        {"MediumDelay_Personalized", base::Milliseconds(300), syncer::BOOKMARKS,
         SigninInterceptionResult::kDismissed, ".MediumDelay", ".Personalized"},
        {"MediumDelay_NoPreference", base::Milliseconds(599), std::nullopt,
         SigninInterceptionResult::kIgnored, ".MediumDelay",
         ".NotPersonalized"},
        {"LongDelay_Personalized", base::Milliseconds(600), syncer::BOOKMARKS,
         SigninInterceptionResult::kDeclined, ".LongDelay", ".Personalized"},
        {"LongDelay_UnsupportedDataType", base::Milliseconds(800),
         syncer::HISTORY, SigninInterceptionResult::kDismissed, ".LongDelay",
         ".NotPersonalized"},
};

class DiceWebSigninInterceptionBubbleViewAccountPreviewParamTest
    : public DiceWebSigninInterceptionBubbleViewTestBase,
      public testing::WithParamInterface<
          std::tuple<SigninInterceptionType, AccountPreviewHistogramTestCase>> {
};

TEST_P(DiceWebSigninInterceptionBubbleViewAccountPreviewParamTest,
       AccountPreviewHistograms) {
  SigninInterceptionType type = std::get<0>(GetParam());
  const AccountPreviewHistogramTestCase& test_case = std::get<1>(GetParam());

  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      preference;
  if (test_case.preferred_data_type.has_value()) {
    preference.emplace();
    preference->preferred_data_types.push_back(
        {*test_case.preferred_data_type, signin::SyncDataQuartile::kAboveQ3});
  }

  base::HistogramTester histogram_tester;
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters(
      type, personal_account_2_, personal_account_, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/false,
      preference, test_case.fetch_duration);

  DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
      bubble_parameters, profile(), test_case.result);

  std::string base_histogram = base::StrCat(
      {"Signin.InterceptResult.", SigninInterceptTypeToString(type)});
  histogram_tester.ExpectUniqueSample(base_histogram, test_case.result, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({base_histogram, test_case.expected_delay_variant}),
      test_case.result, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({base_histogram, test_case.expected_personalized_variant}),
      test_case.result, 1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({base_histogram, test_case.expected_delay_variant,
                    test_case.expected_personalized_variant}),
      test_case.result, 1);
}

INSTANTIATE_TEST_SUITE_P(
    AccountPreviewHistogramCombinations,
    DiceWebSigninInterceptionBubbleViewAccountPreviewParamTest,
    testing::Combine(
        testing::Values(SigninInterceptionType::kChromeSignin,
                        SigninInterceptionType::kChromeSigninNonIntercept,
                        SigninInterceptionType::kMultiUser),
        testing::ValuesIn(kAccountPreviewHistogramTestCases)),
    [](const testing::TestParamInfo<
        DiceWebSigninInterceptionBubbleViewAccountPreviewParamTest::ParamType>&
           info) {
      return base::StrCat({SigninInterceptTypeToString(std::get<0>(info.param)),
                           "_", std::get<1>(info.param).test_suffix});
    });

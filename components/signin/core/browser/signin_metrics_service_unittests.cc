// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics_service.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Equivalent to private pref content: `kWebSigninAccountStartTimesPref`.
constexpr char kWebSigninAccountStartTimesPrefForTesting[] =
    "signin.web_signin_accounts_start_time_dict";

// Equivalent to private pref content: `kSigninPendingStartTimePref`.
constexpr char kSigninPendingStartTimePrefForTesting[] =
    "signin.signin_pending_start_time";

const signin_metrics::AccessPoint kDefaultTestAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;

}  // namespace

enum class Resolution { kReauth, kWebSignin, kSignout };

class SigninMetricsServiceTest : public ::testing::Test {
 public:
  SigninMetricsServiceTest() {
    SigninMetricsService::RegisterProfilePrefs(pref_service_.registry());
  }

  void CreateSigninMetricsService() {
    signin_metrics_service_ = std::make_unique<SigninMetricsService>(
        *identity_manager(), pref_service_);
  }

  void DestroySigninMetricsService() { signin_metrics_service_ = nullptr; }

  void Signin(
      const std::string& email,
      signin_metrics::AccessPoint access_point = kDefaultTestAccessPoint) {
    identity_test_environment_.MakeAccountAvailable(
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithAccessPoint(access_point)
            .Build(email));
  }

  void EnableSync(const std::string& email) {
    identity_test_environment_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSync);
  }

  AccountInfo WebSignin(const std::string& email) {
    return signin::MakeAccountAvailable(
        identity_manager(),
        signin::AccountAvailabilityOptionsBuilder()
            .WithAccessPoint(
                signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
            .Build(email));
  }

  void RemoveAccount(const CoreAccountId account_id) {
    identity_test_environment_.RemoveRefreshTokenForAccount(account_id);
  }

  void TriggerSigninPending() {
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    TriggerErrorStateInAccount(
        identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  }

  void TriggerErrorStateInSecondaryAccount(AccountInfo account) {
    ASSERT_NE(account, identity_manager()->GetPrimaryAccountInfo(
                           signin::ConsentLevel::kSignin));

    TriggerErrorStateInAccount(account.account_id);
  }

  void ResolveSigninPending(Resolution resolution) {
    CoreAccountInfo core_account_info =
        identity_manager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin);
    ASSERT_FALSE(core_account_info.IsEmpty());

    // Clear the error.
    switch (resolution) {
      case Resolution::kReauth:
        // Calling `IdentityTestEnvironment::SetRefreshTokenForPrimaryAccount()`
        // will not fire the notification event in unit tests. Directly fire it
        // here.
        identity_test_environment_
            .UpdatePersistentErrorOfRefreshTokenForAccount(
                core_account_info.account_id,
                GoogleServiceAuthError::AuthErrorNone());
        return;
      case Resolution::kWebSignin: {
        AccountInfo account_info =
            identity_manager()->FindExtendedAccountInfo(core_account_info);
        account_info.access_point =
            signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN;
        identity_test_environment_.UpdateAccountInfoForAccount(account_info);
        // Calling `IdentityTestEnvironment::SetRefreshTokenForPrimaryAccount()`
        // will not fire the notification event in unit tests. Directly fire it
        // here.
        identity_test_environment_
            .UpdatePersistentErrorOfRefreshTokenForAccount(
                account_info.account_id,
                GoogleServiceAuthError::AuthErrorNone());
        return;
      }
      case Resolution::kSignout:
        identity_test_environment_.ClearPrimaryAccount();
        return;
    }
  }

  void TriggerPrimaryAccountRefreshToken(const std::string& token) {
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    signin::SetRefreshTokenForPrimaryAccount(identity_manager(), token);
  }

  // Value is expected to be there.
  base::Time GetAccountWebSigninStartTime(CoreAccountId account_id) const {
    CHECK(pref_service_.HasPrefPath(kWebSigninAccountStartTimesPrefForTesting));
    const base::Value::Dict& first_websignin_account_dict =
        pref_service_.GetDict(kWebSigninAccountStartTimesPrefForTesting);
    std::optional<base::Time> start_time = base::ValueToTime(
        first_websignin_account_dict.Find(account_id.ToString()));
    CHECK(start_time.has_value());
    return start_time.value();
  }

  bool HasWebSigninStartTimePref(CoreAccountId account_id) {
    return pref_service_.HasPrefPath(
               kWebSigninAccountStartTimesPrefForTesting) &&
           pref_service_.GetDict(kWebSigninAccountStartTimesPrefForTesting)
               .contains(account_id.ToString());
  }

  PrefService& pref_service() { return pref_service_; }

 private:
  signin::IdentityManager* identity_manager() {
    return identity_test_environment_.identity_manager();
  }

  void TriggerErrorStateInAccount(CoreAccountId account_id) {
    // Inject the error.
    // Calling
    // `IdentityTestEnvironment::SetInvalidRefreshTokenForPrimaryAccount()` will
    // not fire the notification event in unit tests. Directly fire it here.
    GoogleServiceAuthError error1 =
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER);
    identity_test_environment_.UpdatePersistentErrorOfRefreshTokenForAccount(
        account_id, error1);

    // Trigger two different errors to make sure the effect of the error is well
    // propagated and not dismissed due to caching the last error.
    GoogleServiceAuthError error2 =
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT);
    ASSERT_NE(error1, error2);
    identity_test_environment_.UpdatePersistentErrorOfRefreshTokenForAccount(
        account_id, error2);
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<SigninMetricsService> signin_metrics_service_;

  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

TEST_F(SigninMetricsServiceTest, SigninPendingResolutionReauth) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  TriggerSigninPending();

  ResolveSigninPending(Resolution::kReauth);

  histogram_tester.ExpectUniqueSample("Signin.SigninPending.Resolution",
                                      /*SigninPendingResolution::kReauth*/ 0,
                                      1);
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionTime.Reauth", 1);

  // Started histogram is not expected to be recorded through this part of the
  // flow.
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionSourceStarted", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SigninPending.ResolutionSourceCompleted", kDefaultTestAccessPoint,
      1);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(SigninMetricsServiceTest, SigninPendingResolutionSignout) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  TriggerSigninPending();

  ResolveSigninPending(Resolution::kSignout);

  histogram_tester.ExpectUniqueSample("Signin.SigninPending.Resolution",
                                      /*SigninPendingResolution::kSignout*/ 1,
                                      1);
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionTime.Signout", 1);
}

TEST_F(SigninMetricsServiceTest, SigninPendingResolutionAfterRestart) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  TriggerSigninPending();

  // Destroy and recreate the `SigninMetricsService` to simulate a restart.
  DestroySigninMetricsService();
  CreateSigninMetricsService();

  ResolveSigninPending(Resolution::kSignout);

  // Histograms should still be recorded.
  histogram_tester.ExpectUniqueSample("Signin.SigninPending.Resolution",
                                      /*SigninPendingResolution::kSignout*/ 1,
                                      1);
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionTime.Signout", 1);
}
#endif

TEST_F(SigninMetricsServiceTest, ReceivingNewTokenWhileNotInError) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  // Receiving new token or resolving a pending state while not in error should
  // not record or store anything.
  TriggerPrimaryAccountRefreshToken("new_token_value");
  ResolveSigninPending(Resolution::kReauth);

  EXPECT_FALSE(
      pref_service().HasPrefPath(kSigninPendingStartTimePrefForTesting));
  histogram_tester.ExpectTotalCount("Signin.SigninPending.ResolutionTime", 0);
}

TEST_F(SigninMetricsServiceTest, ReceivingMultipleErrorsDoesNotResetPref) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  const std::string primary_email("primary_test@gmail.com");
  Signin(primary_email);

  TriggerSigninPending();

  EXPECT_TRUE(
      pref_service().HasPrefPath(kSigninPendingStartTimePrefForTesting));
  base::Time signin_pending_start_time =
      pref_service().GetTime(kSigninPendingStartTimePrefForTesting);

  TriggerSigninPending();
  // Second error should not affect the pref time.
  EXPECT_EQ(signin_pending_start_time,
            pref_service().GetTime(kSigninPendingStartTimePrefForTesting));

  const std::string secondary_email("secondary_test@gmail.com");
  ASSERT_NE(primary_email, secondary_email);
  AccountInfo secondary_account = WebSignin(secondary_email);
  TriggerErrorStateInSecondaryAccount(secondary_account);

  // Secondary accounts errors should not affect the pref time.
  EXPECT_EQ(signin_pending_start_time,
            pref_service().GetTime(kSigninPendingStartTimePrefForTesting));
}

TEST_F(SigninMetricsServiceTest,
       SecondaryAccountsErrorDoNotTriggerPendingPrefStartTime) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  const std::string primary_email("primary_test@gmail.com");
  Signin(primary_email);

  const std::string secondary_email("secondary_test@gmail.com");
  ASSERT_NE(primary_email, secondary_email);
  AccountInfo secondary_account = WebSignin(secondary_email);
  TriggerErrorStateInSecondaryAccount(secondary_account);

  // Secondary accounts error should not set the pref.
  EXPECT_FALSE(
      pref_service().HasPrefPath(kSigninPendingStartTimePrefForTesting));
}

struct AccessPointParam {
  signin_metrics::AccessPoint access_point;
  std::string histogram_time_name;  // Empty for no histogram expectations.
};

const AccessPointParam params[] = {
    {signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
     "Signin.WebSignin.TimeToChromeSignin.ProfileMenu"},
    {signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE,
     "Signin.WebSignin.TimeToChromeSignin.PasswordSigninPromo"},
    {signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS, ""},
};

class SigninMetricsServiceAccessPointParamTest
    : public SigninMetricsServiceTest,
      public testing::WithParamInterface<AccessPointParam> {};

TEST_P(SigninMetricsServiceAccessPointParamTest, WebSigninToChromeSignin) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  AccountInfo account = WebSignin("test@gmail.com");

  Signin(account.email, GetParam().access_point);

  if (!GetParam().histogram_time_name.empty()) {
    histogram_tester.ExpectTotalCount(GetParam().histogram_time_name, 1);
  } else {
    EXPECT_EQ(
        0., histogram_tester
                .GetTotalCountsForPrefix("Signin.WebSignin.TimeToChromeSignin")
                .size());
  }

  histogram_tester.ExpectUniqueSample("Signin.WebSignin.SourceToChromeSignin",
                                      GetParam().access_point, 1);

  // No metrics should be recorded from Signin to Sync.
  base::HistogramTester histogram_tester_sync;
  EnableSync(account.email);
  EXPECT_EQ(
      0.,
      histogram_tester_sync.GetTotalCountsForPrefix("Signin.WebSignin").size());
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SigninMetricsServiceAccessPointParamTest,
                         testing::ValuesIn(params));

TEST_F(SigninMetricsServiceTest, WebSigninToChromeSigninAfterRestart) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  AccountInfo account = WebSignin("test@gmail.com");

  // Destroy and recreate the `SigninMetricsService` to simulate a restart.
  DestroySigninMetricsService();
  CreateSigninMetricsService();

  Signin(account.email,
         signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);

  histogram_tester.ExpectTotalCount(
      "Signin.WebSignin.TimeToChromeSignin.ProfileMenu", 1);
}

TEST_F(SigninMetricsServiceTest, WebSigninWithMultipleAccounts) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  AccountInfo first_account = WebSignin("first_test@gmail.com");
  EXPECT_TRUE(
      pref_service().HasPrefPath(kWebSigninAccountStartTimesPrefForTesting));
  base::Time first_web_signin_start_time =
      GetAccountWebSigninStartTime(first_account.account_id);

  AccountInfo second_account = WebSignin("second_test@gmail.com");
  ASSERT_NE(first_account.email, second_account.email);
  base::Time second_web_signin_start_time =
      GetAccountWebSigninStartTime(second_account.account_id);
  EXPECT_NE(first_web_signin_start_time, second_web_signin_start_time);

  // Secondary accounts through the settings page, this is a real use case.
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;
  Signin(second_account.email, access_point);

  // Pref should be cleared and metrics should be measured even with the
  // secondary account signing in.
  EXPECT_FALSE(
      pref_service().HasPrefPath(kWebSigninAccountStartTimesPrefForTesting));

  histogram_tester.ExpectUniqueSample("Signin.WebSignin.SourceToChromeSignin",
                                      access_point, 1);
}

TEST_F(SigninMetricsServiceTest, WebSigninToSignout) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  AccountInfo account = WebSignin("test@gmail.com");
  EXPECT_TRUE(HasWebSigninStartTimePref(account.account_id));

  RemoveAccount(account.account_id);
  EXPECT_FALSE(HasWebSigninStartTimePref(account.account_id));

  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);
  EXPECT_EQ(
      0., histogram_tester.GetTotalCountsForPrefix("Signin.WebSignin.").size());
}

TEST_F(SigninMetricsServiceTest, WebSigninForSigninPendingResolution) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");
  TriggerSigninPending();
  ResolveSigninPending(Resolution::kWebSignin);

  histogram_tester.ExpectBucketCount(
      "Signin.SigninPending.ResolutionSourceStarted",
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SigninPending.ResolutionSourceCompleted",
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN, 1);
}

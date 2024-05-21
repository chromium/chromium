// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Equivalent to private pref content: `kFirstAccountWebSigninStartTimePref`.
constexpr char kFirstAccountWebSigninStartTimePrefForTesting[] =
    "signin.first_account_web_signin_start_time";

// Equivalent to private pref content: `kSigninPendingStartTimePref`.
constexpr char kSigninPendingStartTimePrefForTesting[] =
    "signin.sigin_pending_start_time";

}  // namespace

enum class Resolution { kReauth, kSignout };

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

  void Signin(const std::string& email,
              signin_metrics::AccessPoint access_point =
                  signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS) {
    identity_test_environment_.MakeAccountAvailable(
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithAccessPoint(access_point)
            .Build(email));
  }

  AccountInfo WebSignin(const std::string& email) {
    return identity_test_environment_.MakeAccountAvailable(
        email, {.set_cookie = true});
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
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    // Clear the error.
    switch (resolution) {
      case Resolution::kReauth:
        // Calling `IdentityTestEnvironment::SetRefreshTokenForPrimaryAccount()`
        // will not fire the notification event in unit tests. Directly fire it
        // here.
        identity_test_environment_
            .UpdatePersistentErrorOfRefreshTokenForAccount(
                identity_manager()->GetPrimaryAccountId(
                    signin::ConsentLevel::kSignin),
                GoogleServiceAuthError::AuthErrorNone());
        return;
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
    identity_test_environment_.UpdatePersistentErrorOfRefreshTokenForAccount(
        account_id, GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                            CREDENTIALS_REJECTED_BY_CLIENT));
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
  std::string histogram_name;  // Empty for no histogram expectations.
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

  if (!GetParam().histogram_name.empty()) {
    histogram_tester.ExpectTotalCount(GetParam().histogram_name, 1);
  } else {
    EXPECT_EQ(
        0.,
        histogram_tester.GetTotalCountsForPrefix("Signin.WebSignin.").size());
  }
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

TEST_F(SigninMetricsServiceTest, WebSigninWithMultipleAccountsStartTimePref) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  AccountInfo first_account = WebSignin("first_test@gmail.com");
  EXPECT_TRUE(pref_service().HasPrefPath(
      kFirstAccountWebSigninStartTimePrefForTesting));
  base::Time web_signin_start_time =
      pref_service().GetTime(kFirstAccountWebSigninStartTimePrefForTesting);

  AccountInfo second_account = WebSignin("second_test@gmail.com");
  ASSERT_NE(first_account.email, second_account.email);

  // Signing a secondary account should not alter the web signin pref that is
  // tied to the first account only.
  EXPECT_EQ(
      web_signin_start_time,
      pref_service().GetTime(kFirstAccountWebSigninStartTimePrefForTesting));
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics_service.h"

#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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

// Equivalent to private pref content: `kSyncPausedStartTimePref`.
constexpr char kSyncPausedStartTimePrefForTesting[] =
    "signin.sync_paused_start_time";

const signin_metrics::AccessPoint kDefaultTestAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS;

}  // namespace

enum class Resolution { kReauth, kWebSignin, kSignout };

class SigninMetricsServiceTest : public ::testing::Test {
 public:
  SigninMetricsServiceTest()
      : identity_test_environment_(/*test_url_loader_factory=*/nullptr,
                                   &pref_service_) {
    SigninMetricsService::RegisterProfilePrefs(pref_service_.registry());
    signin::ActivePrimaryAccountsMetricsRecorder::RegisterLocalStatePrefs(
        local_state_.registry());

    active_primary_accounts_metrics_recorder_ =
        std::make_unique<signin::ActivePrimaryAccountsMetricsRecorder>(
            local_state_);
  }

  void CreateSigninMetricsService() {
    signin_metrics_service_ = std::make_unique<SigninMetricsService>(
        *identity_manager(), pref_service_,
        active_primary_accounts_metrics_recorder_.get());
  }

  void DestroySigninMetricsService() { signin_metrics_service_ = nullptr; }

  AccountInfo Signin(
      const std::string& email,
      signin_metrics::AccessPoint access_point = kDefaultTestAccessPoint,
      const std::string& gaia_id = "") {
    signin::AccountAvailabilityOptionsBuilder builder;
    builder.AsPrimary(signin::ConsentLevel::kSignin)
        .WithAccessPoint(access_point);
    if (!gaia_id.empty()) {
      builder.WithGaiaId(gaia_id);
    }
    return identity_test_environment_.MakeAccountAvailable(
        builder.Build(email));
  }

  void Signout() { identity_test_environment_.ClearPrimaryAccount(); }

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

    identity_test_environment_.SetInvalidRefreshTokenForPrimaryAccount();
  }

  void TriggerErrorStateInSecondaryAccount(AccountInfo account) {
    ASSERT_NE(account, identity_manager()->GetPrimaryAccountInfo(
                           signin::ConsentLevel::kSignin));

    identity_test_environment_.SetInvalidRefreshTokenForAccount(
        account.account_id);
  }

  void ResolveAuthErrorState(Resolution resolution) {
    CoreAccountInfo core_account_info =
        identity_manager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin);
    ASSERT_FALSE(core_account_info.IsEmpty());

    // Clear the error.
    switch (resolution) {
      case Resolution::kReauth:
        identity_test_environment_.SetRefreshTokenForPrimaryAccount();
        break;
      case Resolution::kWebSignin: {
        AccountInfo account_info =
            identity_manager()->FindExtendedAccountInfo(core_account_info);
        account_info.access_point =
            signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN;
        identity_test_environment_.UpdateAccountInfoForAccount(account_info);
        identity_test_environment_.SetRefreshTokenForPrimaryAccount();
        break;
      }
      case Resolution::kSignout:
        Signout();
        break;
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

  void TriggerSyncPaused() {
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

    identity_test_environment_.SetInvalidRefreshTokenForPrimaryAccount();
  }

  // When loading credentials, simulates that the client is not aware of any
  // error. The error may have occurred while the client was off or the error
  // was not persisted.
  void TriggerLoadingCredentialsWithNoError() {
    CoreAccountInfo core_account_info =
        identity_manager()->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin);
    ASSERT_FALSE(core_account_info.IsEmpty());

    identity_test_environment_.OnErrorStateOfRefreshTokenUpdatedForAccount(
        core_account_info, GoogleServiceAuthError::AuthErrorNone(),
        signin_metrics::SourceForRefreshTokenOperation::
            kTokenService_LoadCredentials);
  }

  PrefService& pref_service() { return pref_service_; }

  signin::ActivePrimaryAccountsMetricsRecorder*
  active_primary_accounts_metrics_recorder() {
    return active_primary_accounts_metrics_recorder_.get();
  }

  signin::IdentityTestEnvironment& GetIdentityTestEnvironment() {
    return identity_test_environment_;
  }

 private:
  signin::IdentityManager* identity_manager() {
    return identity_test_environment_.identity_manager();
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<signin::ActivePrimaryAccountsMetricsRecorder>
      active_primary_accounts_metrics_recorder_;
  signin::IdentityTestEnvironment identity_test_environment_;

  std::unique_ptr<SigninMetricsService> signin_metrics_service_;

  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

TEST_F(SigninMetricsServiceTest, SigninPendingResolutionReauth) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  TriggerSigninPending();

  ResolveAuthErrorState(Resolution::kReauth);

  histogram_tester.ExpectUniqueSample("Signin.SigninPending.Resolution",
                                      /*PendingResolutionSource::kReauth*/ 0,
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

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SyncPaused"),
              base::HistogramTester::CountsMap());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(SigninMetricsServiceTest, SigninPendingResolutionSignout) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  TriggerSigninPending();

  ResolveAuthErrorState(Resolution::kSignout);

  histogram_tester.ExpectUniqueSample("Signin.SigninPending.Resolution",
                                      /*PendingResolutionSource::kSignout*/ 1,
                                      1);
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionTime.Signout", 1);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SyncPaused"),
              base::HistogramTester::CountsMap());
}

TEST_F(SigninMetricsServiceTest, SigninPendingResolutionAfterRestart) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  TriggerSigninPending();

  // Destroy and recreate the `SigninMetricsService` to simulate a restart.
  DestroySigninMetricsService();
  CreateSigninMetricsService();

  ResolveAuthErrorState(Resolution::kSignout);

  // Histograms should still be recorded.
  histogram_tester.ExpectUniqueSample("Signin.SigninPending.Resolution",
                                      /*PendingResolutionSource::kSignout*/ 1,
                                      1);
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionTime.Signout", 1);
}

TEST_F(SigninMetricsServiceTest, SigninPendingWithLoadingCredentials) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  const std::string email("test@gmail.com");
  Signin(email);

  TriggerSigninPending();

  ASSERT_TRUE(
      pref_service().HasPrefPath(kSigninPendingStartTimePrefForTesting));
  base::Time signin_pending_start_time =
      pref_service().GetTime(kSigninPendingStartTimePrefForTesting);

  // This simulates restarting the client, where the error may not necessarily
  // be stored properly.
  TriggerLoadingCredentialsWithNoError();

  // Error time still exists and is not modified.
  ASSERT_TRUE(
      pref_service().HasPrefPath(kSigninPendingStartTimePrefForTesting));
  EXPECT_EQ(signin_pending_start_time,
            pref_service().GetTime(kSigninPendingStartTimePrefForTesting));

  // Reauth should not be recorded, even though we were in SyncPaused state.
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionTime.Reauth", 0);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SyncPaused"),
              base::HistogramTester::CountsMap());

  // In practice after a while, the client will notify the actual correct error
  // when attempting to use the token.
  TriggerSigninPending();

  // Make sure the initial error time is not modified.
  ASSERT_TRUE(
      pref_service().HasPrefPath(kSigninPendingStartTimePrefForTesting));
  EXPECT_EQ(signin_pending_start_time,
            pref_service().GetTime(kSigninPendingStartTimePrefForTesting));

  // And still no values are recorded.
  histogram_tester.ExpectTotalCount(
      "Signin.SigninPending.ResolutionTime.Reauth", 0);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SyncPaused"),
              base::HistogramTester::CountsMap());
}

TEST_F(SigninMetricsServiceTest, ReceivingNewTokenWhileNotInError) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");

  // Receiving new token or resolving a pending state while not in error should
  // not record or store anything.
  TriggerPrimaryAccountRefreshToken("new_token_value");
  ResolveAuthErrorState(Resolution::kReauth);

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
#endif

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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(SigninMetricsServiceTest, WebSigninForSigninPendingResolution) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  Signin("test@gmail.com");
  TriggerSigninPending();
  ResolveAuthErrorState(Resolution::kWebSignin);

  histogram_tester.ExpectBucketCount(
      "Signin.SigninPending.ResolutionSourceStarted",
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SigninPending.ResolutionSourceCompleted",
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN, 1);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(SigninMetricsServiceTest, ExplicitSigninMigration) {
  {
    base::HistogramTester histogram_tester;
    CreateSigninMetricsService();
    histogram_tester.ExpectUniqueSample(
        kExplicitSigninMigrationHistogramName,
        SigninMetricsService::ExplicitSigninMigration::kMigratedSignedOut,
        /*expected_bucket_count=*/1);
  }

  Signin("test@gmail.com",
         signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);
  ASSERT_FALSE(pref_service().GetBoolean(prefs::kExplicitBrowserSignin));

  {
    base::HistogramTester histogram_tester;
    CreateSigninMetricsService();
    histogram_tester.ExpectUniqueSample(
        kExplicitSigninMigrationHistogramName,
        SigninMetricsService::ExplicitSigninMigration::kNotMigratedSignedIn,
        /*expected_bucket_count=*/1);
  }

  pref_service().SetBoolean(prefs::kExplicitBrowserSignin, true);

  {
    base::HistogramTester histogram_tester;
    CreateSigninMetricsService();
    histogram_tester.ExpectUniqueSample(
        kExplicitSigninMigrationHistogramName,
        SigninMetricsService::ExplicitSigninMigration::kMigratedSignedIn,
        /*expected_bucket_count=*/1);
  }

  EnableSync("test@gmail.com");

  {
    base::HistogramTester histogram_tester;
    CreateSigninMetricsService();
    histogram_tester.ExpectUniqueSample(
        kExplicitSigninMigrationHistogramName,
        SigninMetricsService::ExplicitSigninMigration::kMigratedSyncing,
        /*expected_bucket_count=*/1);
  }
}

TEST_F(SigninMetricsServiceTest, ChromeSigninSettingOnSignin) {
  base::HistogramTester histogram_tester;
  CreateSigninMetricsService();

  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN;
  AccountInfo account = Signin("test@gmail.com", access_point);

  // Default user choice is no choice.
  histogram_tester.ExpectUniqueSample("Signin.Settings.ChromeSignin.OnSignin",
                                      ChromeSigninUserChoice::kNoChoice, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Settings.ChromeSignin.AccessPointWithDoNotSignin", 0);

  Signout();

  // Repeat with an explicit user choice.
  ChromeSigninUserChoice user_choice1 = ChromeSigninUserChoice::kAlwaysAsk;
  SigninPrefs signin_prefs(pref_service());
  signin_prefs.SetChromeSigninInterceptionUserChoice(account.gaia,
                                                     user_choice1);
  Signin("test@gmail.com", access_point);

  histogram_tester.ExpectBucketCount("Signin.Settings.ChromeSignin.OnSignin",
                                     user_choice1, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Settings.ChromeSignin.AccessPointWithDoNotSignin", 0);

  Signout();

  // Repeat with choice `kDoNotSignin`.
  ChromeSigninUserChoice user_choice2 = ChromeSigninUserChoice::kDoNotSignin;
  signin_prefs.SetChromeSigninInterceptionUserChoice(account.gaia,
                                                     user_choice2);
  Signin("test@gmail.com", access_point);

  histogram_tester.ExpectBucketCount("Signin.Settings.ChromeSignin.OnSignin",
                                     user_choice2, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Settings.ChromeSignin.AccessPointWithDoNotSignin", access_point,
      1);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(SigninMetricsServiceTest, UpdatesAccountLastActiveTimeOnSignin) {
  const std::string gaia_id("gaia_id");

  CreateSigninMetricsService();

  // Sanity check: Before signing in, there's no last-active timestamp for the
  // account.
  ASSERT_FALSE(active_primary_accounts_metrics_recorder()
                   ->GetLastActiveTimeForAccount(gaia_id)
                   .has_value());

  base::Time before_signin = base::Time::Now();
  Signin("test@gmail.com", kDefaultTestAccessPoint, gaia_id);

  // After signin, the last-active timestamp should've been updated.
  EXPECT_TRUE(active_primary_accounts_metrics_recorder()
                  ->GetLastActiveTimeForAccount(gaia_id)
                  .has_value());
  EXPECT_GE(active_primary_accounts_metrics_recorder()
                ->GetLastActiveTimeForAccount(gaia_id)
                .value_or(base::Time()),
            before_signin);
}

TEST_F(SigninMetricsServiceTest, SyncPausedResolutionTimeReauth) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  const std::string email("test@gmail.com");
  Signin(email);
  EnableSync(email);

  TriggerSyncPaused();

  ResolveAuthErrorState(Resolution::kReauth);

  histogram_tester.ExpectTotalCount("Signin.SyncPaused.ResolutionTime.Reauth",
                                    1);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SigninPending"),
              base::HistogramTester::CountsMap());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(SigninMetricsServiceTest, SyncPausedResolutionTimeSignout) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  const std::string email("test@gmail.com");
  Signin(email);
  EnableSync(email);

  TriggerSyncPaused();

  ResolveAuthErrorState(Resolution::kSignout);

  histogram_tester.ExpectTotalCount("Signin.SyncPaused.ResolutionTime.Signout",
                                    1);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SigninPending"),
              base::HistogramTester::CountsMap());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(SigninMetricsServiceTest, SyncPausedWithLoadingCredentials) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  const std::string email("test@gmail.com");
  Signin(email);
  EnableSync(email);

  TriggerSyncPaused();

  ASSERT_TRUE(pref_service().HasPrefPath(kSyncPausedStartTimePrefForTesting));
  base::Time sync_paused_start_time =
      pref_service().GetTime(kSyncPausedStartTimePrefForTesting);

  // This simulates restarting the client, where the error may not necessarily
  // be stored properly.
  TriggerLoadingCredentialsWithNoError();

  // Error time still exists and is not modified.
  ASSERT_TRUE(pref_service().HasPrefPath(kSyncPausedStartTimePrefForTesting));
  EXPECT_EQ(sync_paused_start_time,
            pref_service().GetTime(kSyncPausedStartTimePrefForTesting));

  // Reauth should not be recorded, even though we are in SyncPaused state.
  histogram_tester.ExpectTotalCount("Signin.SyncPaused.ResolutionTime.Reauth",
                                    0);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SigninPending"),
              base::HistogramTester::CountsMap());

  // Make sure the initial error time is not modified.
  ASSERT_TRUE(pref_service().HasPrefPath(kSyncPausedStartTimePrefForTesting));
  EXPECT_EQ(sync_paused_start_time,
            pref_service().GetTime(kSyncPausedStartTimePrefForTesting));

  // And still no values are recorded.
  histogram_tester.ExpectTotalCount("Signin.SyncPaused.ResolutionTime.Reauth",
                                    0);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SigninPending"),
              base::HistogramTester::CountsMap());
}

// Regression test for: crbug.com/363401501.
TEST_F(SigninMetricsServiceTest, ErrorNotificationEmptyAccount) {
  base::HistogramTester histogram_tester;

  CreateSigninMetricsService();

  ASSERT_FALSE(
      GetIdentityTestEnvironment().identity_manager()->HasPrimaryAccount(
          signin::ConsentLevel::kSignin));

  GetIdentityTestEnvironment().OnErrorStateOfRefreshTokenUpdatedForAccount(
      CoreAccountInfo(), GoogleServiceAuthError::AuthErrorNone(),
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SigninPending"),
              base::HistogramTester::CountsMap());
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("Signin.SyncPaused"),
              base::HistogramTester::CountsMap());
}

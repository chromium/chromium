// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/fake_account_fetcher_factory.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using signin::ConsentLevel;
using signin_metrics::AccessPoint;
using signin_metrics::ProfileSignout;
using testing::ElementsAreArray;

namespace {
struct ExpectedAccessPoints {
  std::optional<AccessPoint> sign_in = std::nullopt;
  std::optional<AccessPoint> sync_opt_in = std::nullopt;
  std::optional<ProfileSignout> sign_out = std::nullopt;
  std::optional<ProfileSignout> turn_off_sync = std::nullopt;
};
}  // namespace

class PrimaryAccountManagerTest : public testing::Test,
                                  public PrimaryAccountManager::Observer {
 public:
  PrimaryAccountManagerTest() : test_signin_client_(&user_prefs_) {
#if BUILDFLAG(IS_ANDROID)
    // Mock AccountManagerFacade in java code for tests that require its
    // initialization.
    signin::SetUpFakeAccountManagerFacade();
#endif
    AccountFetcherService::RegisterPrefs(user_prefs_.registry());
    AccountTrackerService::RegisterPrefs(user_prefs_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(user_prefs_.registry());
    PrimaryAccountManager::RegisterProfilePrefs(user_prefs_.registry());
    SigninPrefs::RegisterProfilePrefs(user_prefs_.registry());
    account_tracker_ = std::make_unique<AccountTrackerService>();
    account_tracker_->Initialize(&user_prefs_, base::FilePath());
    token_service_ = std::make_unique<ProfileOAuth2TokenService>(
        &user_prefs_,
        std::make_unique<FakeProfileOAuth2TokenServiceDelegate>());
    account_fetcher_ = std::make_unique<AccountFetcherService>();
    auto account_fetcher_factory = std::make_unique<FakeAccountFetcherFactory>(
        *token_service_.get(), *signin_client());
    account_fetcher_->Initialize(
        &test_signin_client_, token_service_.get(), account_tracker_.get(),
        std::make_unique<image_fetcher::FakeImageDecoder>(),
        std::move(account_fetcher_factory));
  }

  ~PrimaryAccountManagerTest() override {
    if (manager_) {
      ShutDownManager();
    }
    test_signin_client_.Shutdown();
  }

  TestSigninClient* signin_client() { return &test_signin_client_; }

  AccountTrackerService* account_tracker() { return account_tracker_.get(); }
  AccountFetcherService* account_fetcher() { return account_fetcher_.get(); }
  PrefService* prefs() { return &user_prefs_; }

  // Seed the account tracker with information from logged in user.  Normally
  // this is done by UI code before calling PrimaryAccountManager.
  // Returns the string to use as the account_id.
  CoreAccountId AddToAccountTracker(const GaiaId& gaia_id,
                                    const std::string& email) {
    account_tracker_->SeedAccountInfo(gaia_id, email);
    return account_tracker_->PickAccountIdForAccount(gaia_id, email);
  }

  void CheckSigninMetrics(ExpectedAccessPoints access_points) {
    std::vector<Bucket> expected_sign_in_buckets;
    if (access_points.sign_in.has_value()) {
      expected_sign_in_buckets.emplace_back(*access_points.sign_in, 1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples("Signin.SignIn.Completed"),
                ElementsAreArray(expected_sign_in_buckets));

    std::vector<Bucket> expected_sync_opt_in_buckets;
    if (access_points.sync_opt_in.has_value()) {
      expected_sync_opt_in_buckets.emplace_back(*access_points.sync_opt_in, 1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples("Signin.SyncOptIn.Completed"),
                ElementsAreArray(expected_sync_opt_in_buckets));

    std::vector<Bucket> expected_sign_out_buckets;
    if (access_points.sign_out.has_value()) {
      expected_sign_out_buckets.emplace_back(*access_points.sign_out, 1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples("Signin.SignOut.Completed"),
                ElementsAreArray(expected_sign_out_buckets));

    std::vector<Bucket> expected_turn_off_sync_buckets;
    if (access_points.turn_off_sync.has_value()) {
      expected_turn_off_sync_buckets.emplace_back(*access_points.turn_off_sync,
                                                  1);
    }
    EXPECT_THAT(histogram_tester_.GetAllSamples("Signin.SyncTurnOff.Completed"),
                ElementsAreArray(expected_turn_off_sync_buckets));
  }

  void CheckInitializeAccountInfoStateHistogram(
      PrimaryAccountManager::InitializeAccountInfoState expected_sample) {
    histogram_tester_.ExpectUniqueSample(
        "Signin.PAMInitialize.PrimaryAccountInfoState",
        /*sample=*/expected_sample, /*expected_bucket_count=*/1);
  }

  void CreatePrimaryAccountManager(metrics::ProfileMetricsContext context =
                                       metrics::ProfileMetricsContext()) {
    CHECK(!manager_);
    CHECK(!profile_metrics_service_);
    profile_metrics_service_ =
        std::make_unique<metrics::ProfileMetricsService>(context);
    manager_ = std::make_unique<PrimaryAccountManager>(
        &test_signin_client_, token_service_.get(), account_tracker_.get(),
        profile_metrics_service_.get());
    manager_->AddObserver(this);
  }

  // Shuts down |manager_|.
  void ShutDownManager() {
    CHECK(manager_);
    CHECK(profile_metrics_service_);
    manager_->RemoveObserver(this);
    manager_.reset();
    profile_metrics_service_.reset();
  }

  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override {
    DCHECK(event_details.GetEventTypeFor(signin::ConsentLevel::kSync) !=
               signin::PrimaryAccountChangeEvent::Type::kNone ||
           event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
               signin::PrimaryAccountChangeEvent::Type::kNone)
        << "PrimaryAccountChangeEvent with no change: " << event_details;

    switch (event_details.GetEventTypeFor(ConsentLevel::kSync)) {
      case signin::PrimaryAccountChangeEvent::Type::kSet:
        num_successful_signins_++;
        break;
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        num_successful_signouts_++;
        break;
      case signin::PrimaryAccountChangeEvent::Type::kNone:
        break;
    }
    switch (event_details.GetEventTypeFor(ConsentLevel::kSignin)) {
      case signin::PrimaryAccountChangeEvent::Type::kSet:
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        num_unconsented_account_changed_++;
        break;
      case signin::PrimaryAccountChangeEvent::Type::kNone:
        break;
    }
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestSigninClient test_signin_client_;
  std::unique_ptr<metrics::ProfileMetricsService> profile_metrics_service_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
  std::unique_ptr<ProfileOAuth2TokenService> token_service_;
  std::unique_ptr<AccountFetcherService> account_fetcher_;
  std::unique_ptr<PrimaryAccountManager> manager_;
  std::vector<std::string> oauth_tokens_fetched_;
  std::vector<std::string> cookies_;
  base::HistogramTester histogram_tester_;
  int num_successful_signins_{0};
  int num_successful_signouts_{0};
  int num_unconsented_account_changed_{0};
};

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PrimaryAccountManagerTest, SignOut) {
  CreatePrimaryAccountManager();
  CoreAccountId main_account_id =
      AddToAccountTracker(GaiaId("account_id"), "user@gmail.com");
  AccountInfo account_info = account_tracker()->GetAccountInfo(main_account_id);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  {
    SigninPrefs signin_prefs(*prefs());
    std::optional<base::Time> last_signout_time =
        signin_prefs.GetChromeLastSignoutTime(account_info.gaia);
    EXPECT_FALSE(last_signout_time.has_value());
  }
#endif
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSync,
                                  AccessPoint::kStartPage);
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage});

  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_EQ(1, num_successful_signouts_);
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(
      manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email.empty());
  EXPECT_TRUE(manager_->GetPrimaryAccountId(ConsentLevel::kSync).empty());
  EXPECT_TRUE(manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin).IsEmpty());
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage,
                      .sign_out = signin_metrics::ProfileSignout::kTest,
                      .turn_off_sync = signin_metrics::ProfileSignout::kTest});
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  {
    SigninPrefs signin_prefs(*prefs());
    std::optional<base::Time> last_signout_time =
        signin_prefs.GetChromeLastSignoutTime(account_info.gaia);
    ASSERT_TRUE(last_signout_time.has_value());
    EXPECT_LE(base::Time::Now() - last_signout_time.value(), base::Seconds(10));
  }
#endif

  // Should not be persisted anymore
  ShutDownManager();
  CreatePrimaryAccountManager();
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(
      manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email.empty());
  EXPECT_TRUE(manager_->GetPrimaryAccountId(ConsentLevel::kSync).empty());
  EXPECT_TRUE(manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin).IsEmpty());
}

TEST_F(PrimaryAccountManagerTest, SignOutRevoke) {
  CreatePrimaryAccountManager();
  CoreAccountId main_account_id =
      AddToAccountTracker(GaiaId("main_id"), "user@gmail.com");
  CoreAccountId other_account_id =
      AddToAccountTracker(GaiaId("other_id"), "other@gmail.com");
  token_service_->UpdateCredentials(main_account_id, "token");
  token_service_->UpdateCredentials(other_account_id, "token");
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(main_account_id), ConsentLevel::kSync,
      AccessPoint::kStartPage);
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage});
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_EQ(main_account_id,
            manager_->GetPrimaryAccountId(ConsentLevel::kSync));

  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage,
                      .sign_out = signin_metrics::ProfileSignout::kTest,
                      .turn_off_sync = signin_metrics::ProfileSignout::kTest});

  // Tokens are revoked.
  EXPECT_EQ(1, num_successful_signouts_);
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(token_service_->GetAccounts().empty());
}

TEST_F(PrimaryAccountManagerTest, SignOutWhileProhibited) {
  CreatePrimaryAccountManager();
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(
      manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email.empty());
  EXPECT_TRUE(manager_->GetPrimaryAccountId(ConsentLevel::kSync).empty());

  CoreAccountId main_account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(main_account_id), ConsentLevel::kSync,
      AccessPoint::kStartPage);
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage});

  signin_client()->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage});

  signin_client()->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::ALLOW);
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_EQ(1, num_successful_signouts_);
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage,
                      .sign_out = signin_metrics::ProfileSignout::kTest,
                      .turn_off_sync = signin_metrics::ProfileSignout::kTest});
}

TEST_F(PrimaryAccountManagerTest, UnconsentedSignOutWhileProhibited) {
  CreatePrimaryAccountManager();
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(
      manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email.empty());
  EXPECT_TRUE(manager_->GetPrimaryAccountId(ConsentLevel::kSync).empty());

  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  CoreAccountInfo account_info = account_tracker()->GetAccountInfo(account_id);
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSignin,
                                  AccessPoint::kStartPage);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage});

  signin_client()->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage});

  signin_client()->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::ALLOW);
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sign_out = signin_metrics::ProfileSignout::kTest});
}
#endif

TEST_F(PrimaryAccountManagerTest, RevokeSyncConsentAllowedSignoutProhibited) {
  CreatePrimaryAccountManager();
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(
      manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email.empty());
  EXPECT_TRUE(manager_->GetPrimaryAccountId(ConsentLevel::kSync).empty());

  CoreAccountId main_account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(main_account_id), ConsentLevel::kSync,
      AccessPoint::kStartPage);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage});

  signin_client()->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  manager_->RevokeSyncConsent(signin_metrics::ProfileSignout::kTest);
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage,
                      .turn_off_sync = signin_metrics::ProfileSignout::kTest});

  // Unconsented primary account not changed.
  EXPECT_EQ(1, num_unconsented_account_changed_);
  // Sync consent cleared
  EXPECT_EQ(1, num_successful_signouts_);
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));

#if !BUILDFLAG(IS_CHROMEOS)
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  CheckSigninMetrics({.sign_in = AccessPoint::kStartPage,
                      .sync_opt_in = AccessPoint::kStartPage,
                      .turn_off_sync = signin_metrics::ProfileSignout::kTest});
#endif
}

// Regression test for https://crbug.com/1155519.
TEST_F(PrimaryAccountManagerTest, NoopSignOutDoesNotNotifyObservers) {
  CreatePrimaryAccountManager();
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));

  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  CoreAccountInfo account_info = account_tracker()->GetAccountInfo(account_id);
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSignin,
                                  AccessPoint::kStartPage);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));

  manager_->RevokeSyncConsent(signin_metrics::ProfileSignout::kTest);

  // Since there was no sync consent, observers shouldn't be notified.
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
}

TEST_F(PrimaryAccountManagerTest, SignIn) {
  CreatePrimaryAccountManager();
  EXPECT_EQ("", manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email);
  EXPECT_EQ(CoreAccountId(),
            manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  EXPECT_EQ(0, num_successful_signins_);
  EXPECT_EQ(0, num_unconsented_account_changed_);
  CheckSigninMetrics({});

  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  base::RunLoop loop;
  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  ConsentLevel::kSync, AccessPoint::kSettings,
                                  loop.QuitClosure());

  EXPECT_EQ(1, num_successful_signins_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ("user@gmail.com",
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email);
  EXPECT_EQ(account_id, manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  EXPECT_EQ(manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings,
                      .sync_opt_in = AccessPoint::kSettings});

  // The primary account info and metrics should be changed synchronously, only
  // the prefs commit should happen asynchronously and be verified after the
  // `loop.Run()` here.
  loop.Run();
  EXPECT_TRUE(user_prefs_.user_prefs_store()->committed());
}

TEST_F(PrimaryAccountManagerTest,
       ExternalSignIn_ReauthShouldNotSendNotification) {
  CreatePrimaryAccountManager();
  EXPECT_EQ("", manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email);
  EXPECT_EQ(CoreAccountId(),
            manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  EXPECT_EQ(0, num_successful_signins_);
  EXPECT_EQ(0, num_unconsented_account_changed_);

  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  ConsentLevel::kSync, AccessPoint::kSettings);
  EXPECT_EQ(1, num_successful_signins_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ("user@gmail.com",
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email);
  EXPECT_EQ(account_id, manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings,
                      .sync_opt_in = AccessPoint::kSettings});

  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  ConsentLevel::kSync, AccessPoint::kWebSignin);
  EXPECT_EQ(1, num_successful_signins_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ("user@gmail.com",
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email);
  EXPECT_EQ(account_id, manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings,
                      .sync_opt_in = AccessPoint::kSettings});
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrimaryAccountManagerTest, GaiaIdMigration) {
  ASSERT_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker()->GetMigrationState());
  std::string email = "user@gmail.com";
  GaiaId gaia_id("account_gaia_id");

  PrefService* client_prefs = signin_client()->GetPrefs();
  client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                           AccountTrackerService::MIGRATION_NOT_STARTED);
  ScopedListPrefUpdate update(client_prefs, prefs::kAccountInfo);
  update->clear();
  base::DictValue dict;
  dict.Set("account_id", email);
  dict.Set("email", email);
  dict.Set("gaia", gaia_id.ToString());
  update->Append(std::move(dict));

  account_tracker()->ResetForTesting();

  client_prefs->SetString(prefs::kGoogleServicesAccountId, email);
  client_prefs->SetBoolean(prefs::kGoogleServicesConsentedToSync, true);

  CreatePrimaryAccountManager();

  EXPECT_EQ(CoreAccountId::FromGaiaId(gaia_id),
            manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  EXPECT_EQ(gaia_id.ToString(),
            user_prefs_.GetString(prefs::kGoogleServicesAccountId));
}

TEST_F(PrimaryAccountManagerTest, GaiaIdMigrationCrashInTheMiddle) {
  ASSERT_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker()->GetMigrationState());
  std::string email = "user@gmail.com";
  GaiaId gaia_id("account_gaia_id");

  PrefService* client_prefs = signin_client()->GetPrefs();
  client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                           AccountTrackerService::MIGRATION_NOT_STARTED);
  ScopedListPrefUpdate update(client_prefs, prefs::kAccountInfo);
  update->clear();
  base::DictValue dict;
  dict.Set("account_id", email);
  dict.Set("email", email);
  dict.Set("gaia", gaia_id.ToString());
  update->Append(std::move(dict));

  account_tracker()->ResetForTesting();

  client_prefs->SetString(prefs::kGoogleServicesAccountId, gaia_id.ToString());
  client_prefs->SetBoolean(prefs::kGoogleServicesConsentedToSync, true);

  CreatePrimaryAccountManager();
  EXPECT_EQ(CoreAccountId::FromGaiaId(gaia_id),
            manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  EXPECT_EQ(gaia_id.ToString(),
            user_prefs_.GetString(prefs::kGoogleServicesAccountId));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AccountTrackerService::MIGRATION_DONE,
            account_tracker()->GetMigrationState());
}
#endif

TEST_F(PrimaryAccountManagerTest, RestoreFromPrefsConsented) {
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, true);
  CreatePrimaryAccountManager();
  EXPECT_EQ("user@gmail.com",
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).email);
  EXPECT_EQ(account_id, manager_->GetPrimaryAccountId(ConsentLevel::kSync));
  EXPECT_EQ(manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));

  // Not a logged event.
  CheckSigninMetrics({});
}

TEST_F(PrimaryAccountManagerTest, RestoreFromPrefsUnconsented) {
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, false);
  CreatePrimaryAccountManager();
  EXPECT_EQ("user@gmail.com",
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin).email);
  EXPECT_EQ(account_id,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin).account_id);
  EXPECT_TRUE(manager_->GetPrimaryAccountInfo(ConsentLevel::kSync).IsEmpty());

  // Not a logged event.
  CheckSigninMetrics({});
}

TEST_F(PrimaryAccountManagerTest, SetPrimaryAccountInfoWithSigninConsent) {
  CreatePrimaryAccountManager();
  EXPECT_EQ(CoreAccountInfo(),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(CoreAccountInfo(),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  EXPECT_EQ(0, num_unconsented_account_changed_);
  EXPECT_EQ(0, num_successful_signins_);
  CheckSigninMetrics({});

  // Set the primary account with sign-in consent.
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  CoreAccountInfo account_info = account_tracker()->GetAccountInfo(account_id);

  base::RunLoop loop;
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSignin,
                                  AccessPoint::kSettings, loop.QuitClosure());

  EXPECT_EQ(0, num_successful_signins_);
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ(account_info,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(CoreAccountInfo(),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  EXPECT_EQ(user_prefs_.GetString(prefs::kGoogleServicesLastSignedInUsername),
            "user@gmail.com");
  EXPECT_EQ(user_prefs_.GetString(prefs::kGoogleServicesLastSyncingUsername),
            std::string());
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings});

  // The primary account info and metrics should be changed synchronously, only
  // the prefs commit should happen asynchronously and be verified after the
  // `loop.Run()` here.
  loop.Run();
  EXPECT_TRUE(user_prefs_.user_prefs_store()->committed());

  // Set the same account again.
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSignin,
                                  AccessPoint::kWebSignin);
  EXPECT_EQ(0, num_successful_signins_);
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ(account_info,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(CoreAccountInfo(),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings});

  // Change the email to another equivalent email. The account is updated but
  // observers are not notified.
  account_info.email = "us.er@gmail.com";
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSignin,
                                  AccessPoint::kFullscreenSigninPromo);
  EXPECT_EQ(0, num_successful_signins_);
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ(account_info,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(CoreAccountInfo(),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings});
}

TEST_F(PrimaryAccountManagerTest, SetPrimaryAccountInfoWithSyncConsent) {
  CreatePrimaryAccountManager();
  EXPECT_EQ(CoreAccountInfo(),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(CoreAccountInfo(),
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  EXPECT_EQ(0, num_unconsented_account_changed_);
  EXPECT_EQ(0, num_successful_signins_);
  CheckSigninMetrics({});

  // Set the primary account with sync consent.
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  CoreAccountInfo account_info = account_tracker()->GetAccountInfo(account_id);

  base::RunLoop loop;
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSync,
                                  AccessPoint::kSettings, loop.QuitClosure());

  EXPECT_EQ(1, num_successful_signins_);
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ(account_info,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(account_info, manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  EXPECT_EQ(user_prefs_.GetString(prefs::kGoogleServicesLastSignedInUsername),
            "user@gmail.com");
  EXPECT_EQ(user_prefs_.GetString(prefs::kGoogleServicesLastSyncingUsername),
            "user@gmail.com");
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings,
                      .sync_opt_in = AccessPoint::kSettings});

  // The primary account info and metrics should be changed synchronously, only
  // the prefs commit should happen asynchronously and be verified after the
  // `loop.Run()` here.
  loop.Run();
  EXPECT_TRUE(user_prefs_.user_prefs_store()->committed());

  // Set the same account again.
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSync,
                                  AccessPoint::kWebSignin);
  EXPECT_EQ(1, num_successful_signins_);
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ(account_info,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(account_info, manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings,
                      .sync_opt_in = AccessPoint::kSettings});

  // Change the email to another equivalent email. The account is updated but
  // observers are not notified.
  account_info.email = "us.er@gmail.com";
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSync,
                                  AccessPoint::kFullscreenSigninPromo);
  EXPECT_EQ(1, num_successful_signins_);
  EXPECT_EQ(0, num_successful_signouts_);
  EXPECT_EQ(1, num_unconsented_account_changed_);
  EXPECT_EQ(account_info,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  EXPECT_EQ(account_info, manager_->GetPrimaryAccountInfo(ConsentLevel::kSync));
  CheckSigninMetrics({.sign_in = AccessPoint::kSettings,
                      .sync_opt_in = AccessPoint::kSettings});
}

TEST_F(PrimaryAccountManagerTest, RevokeSyncConsent) {
  CreatePrimaryAccountManager();
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  ConsentLevel::kSync, AccessPoint::kStartPage);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSync));

  manager_->RevokeSyncConsent(signin_metrics::ProfileSignout::kTest);
  EXPECT_EQ(1, num_successful_signouts_);
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(account_id,
            manager_->GetPrimaryAccountInfo(ConsentLevel::kSignin).account_id);
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PrimaryAccountManagerTest, ClearPrimaryAccount) {
  CreatePrimaryAccountManager();
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com");
  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  ConsentLevel::kSync, AccessPoint::kStartPage);
  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSync));

  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_EQ(1, num_successful_signouts_);
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PrimaryAccountManagerTest, RestoreSyncAccountInfo) {
  user_prefs_.SetString(prefs::kGoogleServicesLastSyncingUsername,
                        "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesLastSyncingGaiaId, "gaia_id");
  CoreAccountId account_id = account_tracker()->PickAccountIdForAccount(
      GaiaId("gaia_id"), "user@gmail.com");
  ASSERT_FALSE(account_id.empty());
  ASSERT_TRUE(account_tracker()->GetAccountInfo(account_id).IsEmpty());
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, true);
  CreatePrimaryAccountManager();

  EXPECT_TRUE(manager_->HasPrimaryAccount(ConsentLevel::kSync));
  CoreAccountInfo account_info = account_tracker()->GetAccountInfo(account_id);
  ASSERT_FALSE(account_info.IsEmpty());
  EXPECT_EQ(account_id, account_info.account_id);
  EXPECT_EQ(GaiaId("gaia_id"), account_info.gaia);
  EXPECT_EQ("user@gmail.com", account_info.email);
  CheckInitializeAccountInfoStateHistogram(
      PrimaryAccountManager::InitializeAccountInfoState::
          kEmptyAccountInfo_RestoreSuccessFromLastSyncInfo);
}

TEST_F(PrimaryAccountManagerTest, RestoreFailedLastSyncGaiaIDMissing) {
  user_prefs_.SetString(prefs::kGoogleServicesLastSyncingUsername,
                        "user@gmail.com");
  CoreAccountId account_id = account_tracker()->PickAccountIdForAccount(
      GaiaId("gaia_id"), "user@gmail.com");
  ASSERT_FALSE(account_id.empty());
  ASSERT_TRUE(account_tracker()->GetAccountInfo(account_id).IsEmpty());
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, true);
  CreatePrimaryAccountManager();

  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(account_tracker()->GetAccountInfo(account_id).IsEmpty());
  CheckInitializeAccountInfoStateHistogram(
      PrimaryAccountManager::InitializeAccountInfoState::
          kEmptyAccountInfo_RestoreFailedNoLastSyncGaiaId);
}

TEST_F(PrimaryAccountManagerTest, RestoreFailedLastSyncEmailMissing) {
  user_prefs_.SetString(prefs::kGoogleServicesLastSyncingGaiaId, "gaia_id");
  CoreAccountId account_id = account_tracker()->PickAccountIdForAccount(
      GaiaId("gaia_id"), "user@gmail.com");
  ASSERT_FALSE(account_id.empty());
  ASSERT_TRUE(account_tracker()->GetAccountInfo(account_id).IsEmpty());
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, true);
  CreatePrimaryAccountManager();

  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(account_tracker()->GetAccountInfo(account_id).IsEmpty());
  CheckInitializeAccountInfoStateHistogram(
      PrimaryAccountManager::InitializeAccountInfoState::
          kEmptyAccountInfo_RestoreFailedNoLastSyncEmail);
}

TEST_F(PrimaryAccountManagerTest, RestoreFailedNotSyncing) {
  CoreAccountId account_id = account_tracker()->PickAccountIdForAccount(
      GaiaId("gaia_id"), "user@gmail.com");
  ASSERT_FALSE(account_id.empty());
  ASSERT_TRUE(account_tracker()->GetAccountInfo(account_id).IsEmpty());
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, false);
  CreatePrimaryAccountManager();

  EXPECT_FALSE(manager_->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(account_tracker()->GetAccountInfo(account_id).IsEmpty());
  CheckInitializeAccountInfoStateHistogram(
      PrimaryAccountManager::InitializeAccountInfoState::
          kEmptyAccountInfo_RestoreFailedNotSyncConsented);
}

TEST_F(PrimaryAccountManagerTest, ExplicitSigninPref) {
  CreatePrimaryAccountManager();
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("account_id"), "user@gmail.com");

  ASSERT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Simulate an explicit signin through the Chrome Signin Intercept bubble.
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));
#else
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  // Clearing signin.
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);

  EXPECT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));
#endif
}

TEST_F(PrimaryAccountManagerTest, ImplicitSigninDoesNotSetExplicitSigninPref) {
  CreatePrimaryAccountManager();
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("account_id"), "user@gmail.com");

  ASSERT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Simulate an implicit signin through a web signin event.
  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  signin::ConsentLevel::kSignin,
                                  signin_metrics::AccessPoint::kWebSignin);

  EXPECT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));
}

TEST_F(PrimaryAccountManagerTest, ExplicitSigninFollowedByWebSignin) {
  // Web signin can trigger automatic sign in if the user previously enabled
  // automatic sign in. Signing in through WEB_SIGNIN should clear the
  // `prefs::kExplicitBrowserSignin` pref anyway.

  CreatePrimaryAccountManager();
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("account_id"), "user@gmail.com");

  ASSERT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Simulate an explicit signin through the Chrome Signin Intercept bubble.
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));
#else
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));
#endif

  // Creating a second account.
  CoreAccountId account_id2 =
      AddToAccountTracker(GaiaId("account_id2"), "user2@gmail.com");

  // Simulating an sign in from a web signin access point without prior sign
  // out.
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id2),
      signin::ConsentLevel::kSignin, signin_metrics::AccessPoint::kWebSignin);

  // The explicit sign in pref should be reset.
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));
}

TEST_F(PrimaryAccountManagerTest, AccountStoragePrefFeatureDisabled) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(switches::kEnablePreferencesAccountStorage);
  CreatePrimaryAccountManager();
  // false by default.
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  // Signing in does not set the pref.
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("account_id"), "user@gmail.com");
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kAvatarBubbleSignIn);
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSync,
      signin_metrics::AccessPoint::kAvatarBubbleSignIn);
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
}

TEST_F(PrimaryAccountManagerTest, AccountStoragePrefExistingSyncUser) {
  {
    // Feature disabled.
    base::test::ScopedFeatureList feature;
    feature.InitAndDisableFeature(switches::kEnablePreferencesAccountStorage);
    CreatePrimaryAccountManager();
    CoreAccountId account_id =
        AddToAccountTracker(GaiaId("account_id"), "user@gmail.com");
    manager_->SetPrimaryAccountInfo(
        account_tracker()->GetAccountInfo(account_id),
        signin::ConsentLevel::kSync,
        signin_metrics::AccessPoint::kAvatarBubbleSignIn);
    ASSERT_FALSE(prefs()->GetBoolean(
        prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
    ShutDownManager();
  }

  {
    // Restarting with kSync consent sets the preference.
    base::test::ScopedFeatureList feature{
        switches::kEnablePreferencesAccountStorage};
    CreatePrimaryAccountManager();
    ASSERT_TRUE(manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));
    EXPECT_TRUE(prefs()->GetBoolean(
        prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  }
}

TEST_F(PrimaryAccountManagerTest, AccountStoragePrefRollback) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(switches::kEnablePreferencesAccountStorage);

  prefs()->SetBoolean(prefs::kPrefsThemesSearchEnginesAccountStorageEnabled,
                      true);
  CreatePrimaryAccountManager();
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
}

TEST_F(PrimaryAccountManagerTest, AccountStoragePrefNewUser) {
  // New signin sets the prefs.
  base::test::ScopedFeatureList feature{
      switches::kEnablePreferencesAccountStorage};
  CreatePrimaryAccountManager();
  ASSERT_FALSE(manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // Pref is false by default.
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  CoreAccountId account_id =
      AddToAccountTracker(GaiaId("account_id"), "user@gmail.com");
  // Signing in sets the pref.
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kAvatarBubbleSignIn);
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));

// ChromeOS does not support signing out.
#if !BUILDFLAG(IS_CHROMEOS)
  // Signout does not clear the pref.
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

// Explicit sign-in prefs for bookmarks and extensions are only used on Dice
// platforms.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Test that the bookmarks explicit signin pref is preserved across restarts if
// the feature flag is still enabled, but is reset to its default value (false)
// if rhe feature flag is disabled.
TEST_F(PrimaryAccountManagerTest,
       RollingBackUsersOfBookmarksExplicitSigninPrefCheck) {
  GaiaId gaia_id("account_id");
  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  // Explicit sign in through `signin_metrics::AccessPoint::kBookmarkBubble`.
  {
    CreatePrimaryAccountManager();
    CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");

    // Simulate an explicit signin through the bookmark bubble.
    manager_->SetPrimaryAccountInfo(
        account_tracker()->GetAccountInfo(account_id),
        signin::ConsentLevel::kSignin,
        signin_metrics::AccessPoint::kBookmarkBubble);

    // The explicit sign in pref should now be true.
    EXPECT_TRUE(
        SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));
  }

  // Simulate a restart by shutting down the manager.
  ShutDownManager();
  {
    ASSERT_TRUE(
        SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

    CreatePrimaryAccountManager();

    // The explicit signin pref should still be true.
    EXPECT_TRUE(
        SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));
  }
}

// TODO(crbug.com/475822503): Delete this test once Dice migration is complete.
TEST_F(PrimaryAccountManagerTest,
       ExplicitSigninPrefsClearedWhenImplicitlySigningIn) {
  base::test::ScopedFeatureList feature{
      switches::kEnablePreferencesAccountStorage};
  GaiaId gaia_id("account_id");
  // Set prefs set during an explicit signin.
  prefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);
  prefs()->SetBoolean(prefs::kPrefsThemesSearchEnginesAccountStorageEnabled,
                      true);
  SigninPrefs(*prefs()).SetExtensionsExplicitBrowserSignin(gaia_id, true);
  SigninPrefs(*prefs()).SetBookmarksExplicitBrowserSignin(gaia_id, true);

  CreatePrimaryAccountManager();
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");

  // Simulate an implicit signin.
  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  signin::ConsentLevel::kSignin,
                                  signin_metrics::AccessPoint::kWebSignin);

  // The explicit signin pref should be cleared.
  EXPECT_FALSE(prefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Prefs, bookmarks and extensions explicit signin prefs should be cleared.
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrefsThemesSearchEnginesAccountStorageEnabled));
  EXPECT_FALSE(
      SigninPrefs(*prefs()).GetExtensionsExplicitBrowserSignin(gaia_id));
  EXPECT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(PrimaryAccountManagerTest, PerProfileMetrics) {
  // First PrimaryAccountManager with `context1`, records metric for both
  // `Profile1` and non-profile specific.
  metrics::ProfileMetricsContext context1(1);
  CreatePrimaryAccountManager(context1);

  CoreAccountInfo account_info = account_tracker()->GetAccountInfo(
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com"));
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSignin,
                                  AccessPoint::kStartPage);

  histogram_tester_.ExpectUniqueSample("Signin.SignIn.Completed",
                                       AccessPoint::kStartPage, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SignIn.Completed.Profile1",
                                       AccessPoint::kStartPage, 1);

  manager_->ClearPrimaryAccount(ProfileSignout::kTest);

  histogram_tester_.ExpectUniqueSample("Signin.SignOut.Completed",
                                       ProfileSignout::kTest, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SignOut.Completed.Profile1",
                                       ProfileSignout::kTest, 1);

  ShutDownManager();

  // Second PrimaryAccountManager with `context2`, records metric for both
  // `Profile2` and non-profile specific.
  metrics::ProfileMetricsContext context2(2);
  CreatePrimaryAccountManager(context2);

  CoreAccountInfo account_info2 = account_tracker()->GetAccountInfo(
      AddToAccountTracker(GaiaId("gaia_id2"), "user2@gmail.com"));
  manager_->SetPrimaryAccountInfo(account_info2, ConsentLevel::kSignin,
                                  AccessPoint::kStartPage);

  // Does not impact already recorded `Profie1` metric. Non profile specific
  // metric accumulates.
  histogram_tester_.ExpectUniqueSample("Signin.SignIn.Completed",
                                       AccessPoint::kStartPage, 2);
  histogram_tester_.ExpectUniqueSample("Signin.SignIn.Completed.Profile1",
                                       AccessPoint::kStartPage, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SignIn.Completed.Profile2",
                                       AccessPoint::kStartPage, 1);

  manager_->ClearPrimaryAccount(ProfileSignout::kTest);

  // Does not impact already recorded `Profie1` metric. Non profile specific
  // metric accumulates.
  histogram_tester_.ExpectUniqueSample("Signin.SignOut.Completed",
                                       ProfileSignout::kTest, 2);
  histogram_tester_.ExpectUniqueSample("Signin.SignOut.Completed.Profile1",
                                       ProfileSignout::kTest, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SignOut.Completed.Profile2",
                                       ProfileSignout::kTest, 1);
}

TEST_F(PrimaryAccountManagerTest, PerProfileMetricsSync) {
  // First PrimaryAccountManager with `context1`, records metric for both
  // `Profile1` and non-profile specific.
  metrics::ProfileMetricsContext context1(1);
  CreatePrimaryAccountManager(context1);

  CoreAccountInfo account_info = account_tracker()->GetAccountInfo(
      AddToAccountTracker(GaiaId("gaia_id"), "user@gmail.com"));
  manager_->SetPrimaryAccountInfo(account_info, ConsentLevel::kSync,
                                  AccessPoint::kStartPage);

  histogram_tester_.ExpectUniqueSample("Signin.SyncOptIn.Completed",
                                       AccessPoint::kStartPage, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SyncOptIn.Completed.Profile1",
                                       AccessPoint::kStartPage, 1);

  manager_->ClearPrimaryAccount(ProfileSignout::kTest);

  histogram_tester_.ExpectUniqueSample("Signin.SyncTurnOff.Completed",
                                       ProfileSignout::kTest, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SyncTurnOff.Completed.Profile1",
                                       ProfileSignout::kTest, 1);

  ShutDownManager();

  // Second PrimaryAccountManager with `context2`, records metric for both
  // `Profile2` and non-profile specific.
  metrics::ProfileMetricsContext context2(2);
  CreatePrimaryAccountManager(context2);

  CoreAccountInfo account_info2 = account_tracker()->GetAccountInfo(
      AddToAccountTracker(GaiaId("gaia_id2"), "user2@gmail.com"));
  manager_->SetPrimaryAccountInfo(account_info2, ConsentLevel::kSync,
                                  AccessPoint::kStartPage);

  // Does not impact already recorded `Profie1` metric. Non profile specific
  // metric accumulates.
  histogram_tester_.ExpectUniqueSample("Signin.SyncOptIn.Completed",
                                       AccessPoint::kStartPage, 2);
  histogram_tester_.ExpectUniqueSample("Signin.SyncOptIn.Completed.Profile1",
                                       AccessPoint::kStartPage, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SyncOptIn.Completed.Profile2",
                                       AccessPoint::kStartPage, 1);

  manager_->ClearPrimaryAccount(ProfileSignout::kTest);

  // Does not impact already recorded `Profie1` metric. Non profile specific
  // metric accumulates.
  histogram_tester_.ExpectUniqueSample("Signin.SyncTurnOff.Completed",
                                       ProfileSignout::kTest, 2);
  histogram_tester_.ExpectUniqueSample("Signin.SyncTurnOff.Completed.Profile1",
                                       ProfileSignout::kTest, 1);
  histogram_tester_.ExpectUniqueSample("Signin.SyncTurnOff.Completed.Profile2",
                                       ProfileSignout::kTest, 1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class PrimaryAccountManagerExplicitSigninNewFeatureTest
    : public PrimaryAccountManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  PrimaryAccountManagerExplicitSigninNewFeatureTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {syncer::kReplaceSyncPromosWithSigninPromosNewSignin},
          /*disabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos},
          /*disabled_features=*/{
              syncer::kReplaceSyncPromosWithSigninPromosNewSignin});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the extensions explicit signin pref is set if the user signs in
// through the extension install bubble.
TEST_P(PrimaryAccountManagerExplicitSigninNewFeatureTest,
       ExplicitSigninExtensionPref) {
  CreatePrimaryAccountManager();
  GaiaId gaia_id("account_id");
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");

  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetExtensionsExplicitBrowserSignin(gaia_id));

  // Simulate an explicit signin through the extension install bubble.
  // This should count as an extension explicit sign in.
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kExtensionInstallBubble);

  EXPECT_TRUE(
      SigninPrefs(*prefs()).GetExtensionsExplicitBrowserSignin(gaia_id));

  // Verify that we have logged a new opt in, both from the extension bubble and
  // any access point.
  histogram_tester_.ExpectUniqueSample(
      "Signin.Extensions.ExplicitSigninFromExtensionInstallBubble",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.Extensions.ExplicitSigninFromAnyAccessPoint",
      /*sample=*/true, /*expected_bucket_count=*/1);

  // Clearing signin.
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);

  // Now sign in with a different user through a non-extension access point.
  // The pref should also record an extension explicit sign in for them.
  GaiaId other_gaia_id("other_account_id");
  CoreAccountId other_account_id =
      AddToAccountTracker(other_gaia_id, "user2@gmail.com");

  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(other_account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);

  EXPECT_TRUE(
      SigninPrefs(*prefs()).GetExtensionsExplicitBrowserSignin(other_gaia_id));

  // Verify histograms as well that this was for a new opt-in.
  histogram_tester_.ExpectUniqueSample(
      "Signin.Extensions.ExplicitSigninFromExtensionInstallBubble",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester_.ExpectUniqueSample(
      "Signin.Extensions.ExplicitSigninFromAnyAccessPoint",
      /*sample=*/true, /*expected_bucket_count=*/2);

  // Sign out, then sign in again through the extensions install bubble.
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kExtensionInstallBubble);

  // Verify that entries are recorded for existing opt-in.
  histogram_tester_.ExpectBucketCount(
      "Signin.Extensions.ExplicitSigninFromExtensionInstallBubble",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.Extensions.ExplicitSigninFromExtensionInstallBubble",
      /*sample=*/false, /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      "Signin.Extensions.ExplicitSigninFromAnyAccessPoint",
      /*sample=*/true, /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(
      "Signin.Extensions.ExplicitSigninFromAnyAccessPoint",
      /*sample=*/false, /*expected_count=*/1);
}

// Test that the bookmarks explicit signin pref is set for all new sign-ins.
TEST_P(PrimaryAccountManagerExplicitSigninNewFeatureTest,
       ExplicitSigninBookmarksPref) {
  CreatePrimaryAccountManager();
  GaiaId gaia_id("account_id");
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");

  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  // Simulate an explicit signin through the bookmark bubble.
  manager_->SetPrimaryAccountInfo(account_tracker()->GetAccountInfo(account_id),
                                  signin::ConsentLevel::kSignin,
                                  signin_metrics::AccessPoint::kBookmarkBubble);

  EXPECT_TRUE(SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  // Clearing signin.
  manager_->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);

  // Now sign in with a different user through a non-bookmarks access point.
  GaiaId other_gaia_id("other_account_id");
  CoreAccountId other_account_id =
      AddToAccountTracker(other_gaia_id, "user2@gmail.com");

  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(other_account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);

  EXPECT_TRUE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(other_gaia_id));

  // Quickly verify that the pref is still true for `gaia_id`.
  EXPECT_TRUE(SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));
}

TEST_P(PrimaryAccountManagerExplicitSigninNewFeatureTest,
       ExplicitSigninBookmarksPref_ResetWhenSyncTurnedOn) {
  CreatePrimaryAccountManager();
  GaiaId gaia_id("account_id");
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");

  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);

  ASSERT_TRUE(SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  // Turn on sync. This should disable account storage for
  // bookmarks again.
  manager_->SetPrimaryAccountInfo(
      account_tracker()->GetAccountInfo(account_id),
      signin::ConsentLevel::kSync,
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);

  EXPECT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));
  histogram_tester_.ExpectUniqueSample(
      "Signin.Bookmarks.SyncTurnedOnWithAccountStorageEnabled",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

// Tests for users already signed `ExplicitSigninExtensionsPref` is only set to
// true if the user belongs to the group with
// `kReplaceSyncPromosWithSignInPromos` enabled but
// `kReplaceSyncPromosWithSigninPromosNewSignin` disabled.
TEST_P(PrimaryAccountManagerExplicitSigninNewFeatureTest,
       ExplicitSigninExtensionsPref_AlreadySignedIn) {
  GaiaId gaia_id("gaia_id");
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, false);

  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetExtensionsExplicitBrowserSignin(gaia_id));

  CreatePrimaryAccountManager();

  EXPECT_NE(GetParam(),
            SigninPrefs(*prefs()).GetExtensionsExplicitBrowserSignin(gaia_id));
}

// Tests for users already signed `ExplicitSigninBookmarksPref` is only set to
// true if the user belongs to the group with
// `kReplaceSyncPromosWithSignInPromos` enabled but
// `kReplaceSyncPromosWithSigninPromosNewSignin` disabled.
TEST_P(PrimaryAccountManagerExplicitSigninNewFeatureTest,
       ExplicitSigninBookmarksPref_AlreadySignedIn) {
  GaiaId gaia_id("gaia_id");
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, false);

  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  CreatePrimaryAccountManager();

  EXPECT_NE(GetParam(),
            SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));
}

TEST_P(PrimaryAccountManagerExplicitSigninNewFeatureTest,
       ExplicitSigninExtensionsPref_AlreadySyncing) {
  GaiaId gaia_id("gaia_id");
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, true);

  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  CreatePrimaryAccountManager();

  EXPECT_FALSE(
      SigninPrefs(*prefs()).GetExtensionsExplicitBrowserSignin(gaia_id));
}

TEST_P(PrimaryAccountManagerExplicitSigninNewFeatureTest,
       ExplicitSigninBookmarksPref_AlreadySyncing) {
  GaiaId gaia_id("gaia_id");
  CoreAccountId account_id = AddToAccountTracker(gaia_id, "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id.ToString());
  user_prefs_.SetBoolean(prefs::kGoogleServicesConsentedToSync, true);

  ASSERT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));

  CreatePrimaryAccountManager();

  EXPECT_FALSE(
      SigninPrefs(*prefs()).GetBookmarksExplicitBrowserSignin(gaia_id));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrimaryAccountManagerExplicitSigninNewFeatureTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NewSignins" : "ExistingUsers";
                         });
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

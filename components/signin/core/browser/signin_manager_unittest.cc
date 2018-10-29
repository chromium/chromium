// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_monster.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestSigninManagerObserver : public SigninManagerBase::Observer {
 public:
  TestSigninManagerObserver()
      : num_failed_signins_(0),
        num_successful_signins_(0),
        num_successful_signins_with_password_(0),
        num_signouts_(0) {}

  ~TestSigninManagerObserver() override {}

  int num_failed_signins_;
  int num_successful_signins_;
  int num_successful_signins_with_password_;
  int num_signouts_;

 private:
  // SigninManagerBase::Observer:
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override {
    num_failed_signins_++;
  }

  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override {
    num_successful_signins_++;
  }

  void GoogleSigninSucceededWithPassword(const std::string& account_id,
                                         const std::string& username,
                                         const std::string& password) override {
    num_successful_signins_with_password_++;
  }

  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override {
    num_signouts_++;
  }
};

}  // namespace

class SigninManagerTest : public testing::Test {
 public:
  SigninManagerTest()
      : test_signin_client_(&user_prefs_),
        token_service_(&user_prefs_),
        cookie_manager_service_(&token_service_,
                                GaiaConstants::kChromeSource,
                                &test_signin_client_),
        account_consistency_(signin::AccountConsistencyMethod::kDisabled) {
    AccountFetcherService::RegisterPrefs(user_prefs_.registry());
    AccountTrackerService::RegisterPrefs(user_prefs_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(user_prefs_.registry());
    SigninManagerBase::RegisterProfilePrefs(user_prefs_.registry());
    SigninManagerBase::RegisterPrefs(local_state_.registry());
    account_tracker_.Initialize(&user_prefs_, base::FilePath());
    account_fetcher_.Initialize(&test_signin_client_, &token_service_,
                                &account_tracker_,
                                std::make_unique<TestImageDecoder>());
  }

  ~SigninManagerTest() override {
    if (manager_) {
      manager_->RemoveObserver(&test_observer_);
      manager_->Shutdown();
    }
    token_service_.Shutdown();
    test_signin_client_.Shutdown();
    account_tracker_.Shutdown();
    cookie_manager_service_.Shutdown();
    account_fetcher_.Shutdown();
  }

  TestSigninClient* signin_client() { return &test_signin_client_; }

  AccountTrackerService* account_tracker() { return &account_tracker_; }
  FakeAccountFetcherService* account_fetcher() { return &account_fetcher_; }
  PrefService* prefs() { return &user_prefs_; }

  // Seed the account tracker with information from logged in user.  Normally
  // this is done by UI code before calling SigninManager.  Returns the string
  // to use as the account_id.
  std::string AddToAccountTracker(const std::string& gaia_id,
                                  const std::string& email) {
    account_tracker_.SeedAccountInfo(gaia_id, email);
    return account_tracker_.PickAccountIdForAccount(gaia_id, email);
  }

  // Create a naked signin manager if integration with PKSs is not needed.
  void CreateSigninManager() {
    DCHECK(!manager_);
    manager_ = std::make_unique<SigninManager>(
        &test_signin_client_, &token_service_, &account_tracker_,
        &cookie_manager_service_, nullptr /* signin_error_controller */,
        account_consistency_);
    manager_->Initialize(&local_state_);
    manager_->AddObserver(&test_observer_);
  }

  // Shuts down |manager_|.
  void ShutDownManager() {
    DCHECK(manager_);
    manager_->RemoveObserver(&test_observer_);
    manager_->Shutdown();
    manager_.reset();
  }

  void ExpectSignInWithRefreshTokenSuccess() {
    EXPECT_TRUE(manager_->IsAuthenticated());
    EXPECT_FALSE(manager_->GetAuthenticatedAccountId().empty());
    EXPECT_FALSE(manager_->GetAuthenticatedAccountInfo().email.empty());

    EXPECT_TRUE(token_service_.RefreshTokenIsAvailable(
        manager_->GetAuthenticatedAccountId()));

    // Should go into token service and stop.
    EXPECT_EQ(1, test_observer_.num_successful_signins_);
    EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
    EXPECT_EQ(0, test_observer_.num_failed_signins_);
  }

  void CompleteSigninCallback(const std::string& oauth_token) {
    oauth_tokens_fetched_.push_back(oauth_token);
    manager_->CompletePendingSignin();
  }

  base::MessageLoop loop_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  TestSigninClient test_signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_;
  FakeGaiaCookieManagerService cookie_manager_service_;
  FakeAccountFetcherService account_fetcher_;
  std::unique_ptr<SigninManager> manager_;
  TestSigninManagerObserver test_observer_;
  std::vector<std::string> oauth_tokens_fetched_;
  std::vector<std::string> cookies_;
  signin::AccountConsistencyMethod account_consistency_;
};

TEST_F(SigninManagerTest, SignInWithRefreshToken) {
  CreateSigninManager();
  EXPECT_FALSE(manager_->IsAuthenticated());

  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  manager_->StartSignInWithRefreshToken(
      "rt", "gaia_id", "user@gmail.com", "password",
      SigninManager::OAuthTokenFetchedCallback());

  ExpectSignInWithRefreshTokenSuccess();

  // Should persist across resets.
  ShutDownManager();
  CreateSigninManager();
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, SignInWithRefreshTokenCallbackComplete) {
  CreateSigninManager();
  EXPECT_FALSE(manager_->IsAuthenticated());

  // Since the password is empty, must verify the gaia cookies first.
  SigninManager::OAuthTokenFetchedCallback callback = base::Bind(
      &SigninManagerTest::CompleteSigninCallback, base::Unretained(this));
  manager_->StartSignInWithRefreshToken("rt", "gaia_id", "user@gmail.com",
                                        "password", callback);

  ExpectSignInWithRefreshTokenSuccess();
  ASSERT_EQ(1U, oauth_tokens_fetched_.size());
  EXPECT_EQ(oauth_tokens_fetched_[0], "rt");
}

TEST_F(SigninManagerTest, SignInWithRefreshTokenCallsPostSignout) {
  CreateSigninManager();
  EXPECT_FALSE(manager_->IsAuthenticated());

  std::string gaia_id = "12345";
  std::string email = "user@google.com";

  account_tracker()->SeedAccountInfo(gaia_id, email);
  account_fetcher()->OnRefreshTokensLoaded();

  ASSERT_TRUE(signin_client()->get_signed_in_password().empty());

  manager_->StartSignInWithRefreshToken(
      "rt1", gaia_id, email, "password",
      SigninManager::OAuthTokenFetchedCallback());

  // PostSignedIn is not called until the AccountTrackerService returns.
  ASSERT_EQ("", signin_client()->get_signed_in_password());

  account_fetcher()->FakeUserInfoFetchSuccess(
      account_tracker()->PickAccountIdForAccount(gaia_id, email), email,
      gaia_id, "google.com", "full_name", "given_name", "locale",
      "http://www.google.com");

  // AccountTracker and SigninManager are both done and PostSignedIn was called.
  ASSERT_EQ("password", signin_client()->get_signed_in_password());

  ExpectSignInWithRefreshTokenSuccess();
}

TEST_F(SigninManagerTest, SignOut) {
  CreateSigninManager();
  manager_->StartSignInWithRefreshToken(
      "rt", "gaia_id", "user@gmail.com", "password",
      SigninManager::OAuthTokenFetchedCallback());
  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_FALSE(manager_->IsAuthenticated());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountInfo().email.empty());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountId().empty());
  // Should not be persisted anymore
  ShutDownManager();
  CreateSigninManager();
  EXPECT_FALSE(manager_->IsAuthenticated());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountInfo().email.empty());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountId().empty());
}

TEST_F(SigninManagerTest, SignOutRevoke) {
  CreateSigninManager();
  std::string main_account_id =
      AddToAccountTracker("main_id", "user@gmail.com");
  std::string other_account_id =
      AddToAccountTracker("other_id", "other@gmail.com");
  token_service_.UpdateCredentials(main_account_id, "token");
  token_service_.UpdateCredentials(other_account_id, "token");
  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_TRUE(manager_->IsAuthenticated());
  EXPECT_EQ(main_account_id, manager_->GetAuthenticatedAccountId());

  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);

  // Tokens are revoked.
  EXPECT_FALSE(manager_->IsAuthenticated());
  EXPECT_TRUE(token_service_.GetAccounts().empty());
}

TEST_F(SigninManagerTest, SignOutDiceNoRevoke) {
  account_consistency_ = signin::AccountConsistencyMethod::kDice;
  CreateSigninManager();
  std::string main_account_id =
      AddToAccountTracker("main_id", "user@gmail.com");
  std::string other_account_id =
      AddToAccountTracker("other_id", "other@gmail.com");
  token_service_.UpdateCredentials(main_account_id, "token");
  token_service_.UpdateCredentials(other_account_id, "token");
  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_TRUE(manager_->IsAuthenticated());
  EXPECT_EQ(main_account_id, manager_->GetAuthenticatedAccountId());

  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);

  // Tokens are not revoked.
  EXPECT_FALSE(manager_->IsAuthenticated());
  std::vector<std::string> expected_tokens = {main_account_id,
                                              other_account_id};
  EXPECT_EQ(expected_tokens, token_service_.GetAccounts());
}

TEST_F(SigninManagerTest, SignOutDiceWithError) {
  account_consistency_ = signin::AccountConsistencyMethod::kDice;
  CreateSigninManager();
  std::string main_account_id =
      AddToAccountTracker("main_id", "user@gmail.com");
  std::string other_account_id =
      AddToAccountTracker("other_id", "other@gmail.com");
  token_service_.UpdateCredentials(main_account_id, "token");
  token_service_.UpdateCredentials(other_account_id, "token");
  manager_->OnExternalSigninCompleted("user@gmail.com");

  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  token_service_.UpdateAuthErrorForTesting(main_account_id, error);
  token_service_.UpdateAuthErrorForTesting(other_account_id, error);
  ASSERT_TRUE(token_service_.RefreshTokenHasError(main_account_id));
  ASSERT_TRUE(token_service_.RefreshTokenHasError(other_account_id));

  EXPECT_TRUE(manager_->IsAuthenticated());
  EXPECT_EQ(main_account_id, manager_->GetAuthenticatedAccountId());

  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);

  // Only main token is revoked.
  EXPECT_FALSE(manager_->IsAuthenticated());
  std::vector<std::string> expected_tokens = {other_account_id};
  EXPECT_EQ(expected_tokens, token_service_.GetAccounts());
}

TEST_F(SigninManagerTest, SignOutWhileProhibited) {
  CreateSigninManager();
  EXPECT_FALSE(manager_->IsAuthenticated());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountInfo().email.empty());
  EXPECT_TRUE(manager_->GetAuthenticatedAccountId().empty());

  manager_->SetAuthenticatedAccountInfo("gaia_id", "user@gmail.com");
  signin_client()->set_is_signout_allowed(false);
  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_TRUE(manager_->IsAuthenticated());
  signin_client()->set_is_signout_allowed(true);
  manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_FALSE(manager_->IsAuthenticated());
}

TEST_F(SigninManagerTest, Prohibited) {
  local_state_.SetString(prefs::kGoogleServicesUsernamePattern,
                         ".*@google.com");
  CreateSigninManager();
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(std::string()));
}

TEST_F(SigninManagerTest, TestAlternateWildcard) {
  // Test to make sure we accept "*@google.com" as a pattern (treat it as if
  // the admin entered ".*@google.com").
  local_state_.SetString(prefs::kGoogleServicesUsernamePattern, "*@google.com");
  CreateSigninManager();
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(std::string()));
}

TEST_F(SigninManagerTest, ProhibitedAtStartup) {
  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id);
  local_state_.SetString(prefs::kGoogleServicesUsernamePattern,
                         ".*@google.com");
  CreateSigninManager();
  // Currently signed in user is prohibited by policy, so should be signed out.
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, ProhibitedAfterStartup) {
  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, account_id);
  CreateSigninManager();
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
  // Update the profile - user should be signed out.
  local_state_.SetString(prefs::kGoogleServicesUsernamePattern,
                         ".*@google.com");
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, ExternalSignIn) {
  CreateSigninManager();
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
  EXPECT_EQ(0, test_observer_.num_successful_signins_);
  EXPECT_EQ(0, test_observer_.num_successful_signins_with_password_);

  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_EQ(1, test_observer_.num_successful_signins_);
  EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
  EXPECT_EQ(0, test_observer_.num_failed_signins_);
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, ExternalSignIn_ReauthShouldNotSendNotification) {
  CreateSigninManager();
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
  EXPECT_EQ(0, test_observer_.num_successful_signins_);
  EXPECT_EQ(0, test_observer_.num_successful_signins_with_password_);

  std::string account_id = AddToAccountTracker("gaia_id", "user@gmail.com");
  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_EQ(1, test_observer_.num_successful_signins_);
  EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
  EXPECT_EQ(0, test_observer_.num_failed_signins_);
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());

  manager_->OnExternalSigninCompleted("user@gmail.com");
  EXPECT_EQ(1, test_observer_.num_successful_signins_);
  EXPECT_EQ(1, test_observer_.num_successful_signins_with_password_);
  EXPECT_EQ(0, test_observer_.num_failed_signins_);
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ(account_id, manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, SigninNotAllowed) {
  std::string user("user@google.com");
  user_prefs_.SetString(prefs::kGoogleServicesAccountId, user);
  user_prefs_.SetBoolean(prefs::kSigninAllowed, false);
  CreateSigninManager();
  AddToAccountTracker("gaia_id", user);
  // Currently signing in is prohibited by policy, so should be signed out.
  EXPECT_EQ("", manager_->GetAuthenticatedAccountInfo().email);
  EXPECT_EQ("", manager_->GetAuthenticatedAccountId());
}

TEST_F(SigninManagerTest, UpgradeToNewPrefs) {
  user_prefs_.SetString(prefs::kGoogleServicesUsername, "user@gmail.com");
  user_prefs_.SetString(prefs::kGoogleServicesUserAccountId, "account_id");
  CreateSigninManager();
  EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountInfo().email);

  if (account_tracker()->GetMigrationState() ==
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    // TODO(rogerta): until the migration to gaia id, the account id will remain
    // the old username.
    EXPECT_EQ("user@gmail.com", manager_->GetAuthenticatedAccountId());
    EXPECT_EQ("user@gmail.com",
              user_prefs_.GetString(prefs::kGoogleServicesAccountId));
  } else {
    EXPECT_EQ("account_id", manager_->GetAuthenticatedAccountId());
    EXPECT_EQ("account_id",
              user_prefs_.GetString(prefs::kGoogleServicesAccountId));
  }
  EXPECT_EQ("", user_prefs_.GetString(prefs::kGoogleServicesUsername));

  // Make sure account tracker was updated.
  AccountInfo info =
      account_tracker()->GetAccountInfo(manager_->GetAuthenticatedAccountId());
  EXPECT_EQ("user@gmail.com", info.email);
  EXPECT_EQ("account_id", info.gaia);
}

TEST_F(SigninManagerTest, CanonicalizesPrefs) {
  // This unit test is not needed after migrating to gaia id.
  if (account_tracker()->GetMigrationState() ==
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    user_prefs_.SetString(prefs::kGoogleServicesUsername, "user.C@gmail.com");

    CreateSigninManager();
    EXPECT_EQ("user.C@gmail.com",
              manager_->GetAuthenticatedAccountInfo().email);

    // TODO(rogerta): until the migration to gaia id, the account id will remain
    // the old username.
    EXPECT_EQ("userc@gmail.com", manager_->GetAuthenticatedAccountId());
    EXPECT_EQ("userc@gmail.com",
              user_prefs_.GetString(prefs::kGoogleServicesAccountId));
    EXPECT_EQ("", user_prefs_.GetString(prefs::kGoogleServicesUsername));

    // Make sure account tracker has a canonicalized username.
    AccountInfo info = account_tracker()->GetAccountInfo(
        manager_->GetAuthenticatedAccountId());
    EXPECT_EQ("user.C@gmail.com", info.email);
    EXPECT_EQ("userc@gmail.com", info.account_id);
  }
}

TEST_F(SigninManagerTest, GaiaIdMigration) {
  if (account_tracker()->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "user@gmail.com";
    std::string gaia_id = "account_gaia_id";

    PrefService* client_prefs = signin_client()->GetPrefs();
    client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);
    ListPrefUpdate update(client_prefs,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("account_id", email);
    dict->SetString("email", email);
    dict->SetString("gaia", gaia_id);
    update->Append(std::move(dict));

    account_tracker()->Shutdown();
    account_tracker()->Initialize(prefs(), base::FilePath());

    client_prefs->SetString(prefs::kGoogleServicesAccountId, email);

    CreateSigninManager();

    EXPECT_EQ(gaia_id, manager_->GetAuthenticatedAccountId());
    EXPECT_EQ(gaia_id, user_prefs_.GetString(prefs::kGoogleServicesAccountId));
  }
}

TEST_F(SigninManagerTest, VeryOldProfileGaiaIdMigration) {
  if (account_tracker()->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "user@gmail.com";
    std::string gaia_id = "account_gaia_id";

    PrefService* client_prefs = signin_client()->GetPrefs();
    client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);
    ListPrefUpdate update(client_prefs,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("account_id", email);
    dict->SetString("email", email);
    dict->SetString("gaia", gaia_id);
    update->Append(std::move(dict));

    account_tracker()->Shutdown();
    account_tracker()->Initialize(prefs(), base::FilePath());

    client_prefs->ClearPref(prefs::kGoogleServicesAccountId);
    client_prefs->SetString(prefs::kGoogleServicesUsername, email);

    CreateSigninManager();
    EXPECT_EQ(gaia_id, manager_->GetAuthenticatedAccountId());
    EXPECT_EQ(gaia_id, user_prefs_.GetString(prefs::kGoogleServicesAccountId));
  }
}

TEST_F(SigninManagerTest, GaiaIdMigrationCrashInTheMiddle) {
  if (account_tracker()->GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "user@gmail.com";
    std::string gaia_id = "account_gaia_id";

    PrefService* client_prefs = signin_client()->GetPrefs();
    client_prefs->SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);
    ListPrefUpdate update(client_prefs,
                          AccountTrackerService::kAccountInfoPref);
    update->Clear();
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString("account_id", email);
    dict->SetString("email", email);
    dict->SetString("gaia", gaia_id);
    update->Append(std::move(dict));

    account_tracker()->Shutdown();
    account_tracker()->Initialize(prefs(), base::FilePath());

    client_prefs->SetString(prefs::kGoogleServicesAccountId, gaia_id);

    CreateSigninManager();
    EXPECT_EQ(gaia_id, manager_->GetAuthenticatedAccountId());
    EXPECT_EQ(gaia_id, user_prefs_.GetString(prefs::kGoogleServicesAccountId));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(AccountTrackerService::MIGRATION_DONE,
              account_tracker()->GetMigrationState());
  }
}

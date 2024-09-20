// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/mock_profile_oauth2_token_service_observer.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/ios/fake_device_accounts_provider.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using ProviderAccount = DeviceAccountsProvider::AccountInfo;

namespace {
constexpr char kNoBindingChallenge[] = "";
}

class ProfileOAuth2TokenServiceIOSDelegateTest
    : public PlatformTest,
      public OAuth2AccessTokenConsumer,
      public ProfileOAuth2TokenServiceObserver {
 public:
  ProfileOAuth2TokenServiceIOSDelegateTest()
      : client_(&prefs_),
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        access_token_success_(0),
        access_token_failure_(0),
        auth_error_changed_count_(0),
        last_access_token_error_(GoogleServiceAuthError::NONE) {}

  void SetUp() override {
    AccountTrackerService::RegisterPrefs(prefs_.registry());
    account_tracker_.Initialize(&prefs_, base::FilePath());

    fake_provider_ = new FakeDeviceAccountsProvider();
    oauth2_delegate_.reset(new ProfileOAuth2TokenServiceIOSDelegate(
        &client_, base::WrapUnique(fake_provider_), &account_tracker_));
    oauth2_delegate_->SetOnRefreshTokenRevokedNotified(base::DoNothing());
    token_service_observation_.Observe(oauth2_delegate_.get());
  }

  void TearDown() override { oauth2_delegate_->Shutdown(); }

  // OAuth2AccessTokenConsumer implementation.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    ++access_token_success_;
  }

  void OnGetTokenFailure(const GoogleServiceAuthError& error) override {
    ++access_token_failure_;
    last_access_token_error_ = error;
  }

  std::string GetConsumerName() const override {
    return "profile_oauth2_token_service_delegate_ios_unittest";
  }

  // ProfileOAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override {
    ++token_available_count_;
  }
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override {
    ++token_revoked_count_;
  }
  void OnRefreshTokensLoaded() override { ++tokens_loaded_count_; }
  void OnAuthErrorChanged(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation source) override {
    ++auth_error_changed_count_;
  }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
    token_available_count_ = 0;
    access_token_failure_ = 0;
    auth_error_changed_count_ = 0;
  }

  CoreAccountId GetAccountId(const ProviderAccount& provider_account) {
    return account_tracker_.PickAccountIdForAccount(provider_account.gaia,
                                                    provider_account.email);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  TestSigninClient client_;
  AccountTrackerService account_tracker_;
  FakeDeviceAccountsProvider* fake_provider_;
  std::unique_ptr<ProfileOAuth2TokenServiceIOSDelegate> oauth2_delegate_;
  TestingOAuth2AccessTokenManagerConsumer consumer_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
  int access_token_success_;
  int access_token_failure_;
  int auth_error_changed_count_;
  GoogleServiceAuthError last_access_token_error_;
  base::ScopedObservation<ProfileOAuth2TokenServiceIOSDelegate,
                          ProfileOAuth2TokenServiceObserver>
      token_service_observation_{this};
};

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       LoadRevokeCredentialsOneAccount) {
  ProviderAccount account = fake_provider_->AddAccount("gaia_1", "email_1@x");
  oauth2_delegate_->LoadCredentials(GetAccountId(account),
                                    /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1U, oauth2_delegate_->GetAccounts().size());
  EXPECT_TRUE(oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account)));

  ResetObserverCounts();
  oauth2_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_delegate_->GetAccounts().size());
  EXPECT_FALSE(oauth2_delegate_->RefreshTokenIsAvailable(
      CoreAccountId::FromGaiaId("another_account")));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       LoadRevokeCredentialsMultipleAccounts) {
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  ProviderAccount account2 = fake_provider_->AddAccount("gaia_2", "email_2@x");
  ProviderAccount account3 = fake_provider_->AddAccount("gaia_3", "email_3@x");
  oauth2_delegate_->LoadCredentials(GetAccountId(account1),
                                    /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(3U, oauth2_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account1)));
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account2)));
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account3)));

  ResetObserverCounts();
  oauth2_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(3, token_revoked_count_);
  EXPECT_EQ(0U, oauth2_delegate_->GetAccounts().size());
  EXPECT_FALSE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account1)));
  EXPECT_FALSE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account2)));
  EXPECT_FALSE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account3)));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       LoadCredentialsPrimaryAccountMissing) {
  CoreAccountId primary_account =
      account_tracker_.SeedAccountInfo("gaia_1", "email_1@x");
  oauth2_delegate_->LoadCredentials(primary_account, /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, auth_error_changed_count_);
  EXPECT_EQ(1U, oauth2_delegate_->GetAccounts().size());
  EXPECT_TRUE(oauth2_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            oauth2_delegate_->GetAuthError(primary_account).state());
  ResetObserverCounts();

  // Reloading the account should not remove the primary account even if it is
  // missing from the device.
  oauth2_delegate_->ReloadAllAccountsFromSystemWithPrimaryAccount(
      primary_account);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(0, auth_error_changed_count_);
  EXPECT_EQ(1U, oauth2_delegate_->GetAccounts().size());
  EXPECT_TRUE(oauth2_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            oauth2_delegate_->GetAuthError(primary_account).state());
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, ReloadAllAccountsFromSystem) {
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  ProviderAccount account2 = fake_provider_->AddAccount("gaia_2", "email_2@x");
  ProviderAccount account3 = fake_provider_->AddAccount("gaia_3", "email_3@x");
  oauth2_delegate_->LoadCredentials(GetAccountId(account1),
                                    /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  // Change the accounts.
  ResetObserverCounts();
  fake_provider_->ClearAccounts();
  fake_provider_->AddAccount(account1.gaia, account1.email);
  ProviderAccount account4 = fake_provider_->AddAccount("gaia_4", "email_4@x");
  oauth2_delegate_->ReloadAllAccountsFromSystemWithPrimaryAccount(
      GetAccountId(account1));

  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account1)));
  EXPECT_FALSE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account2)));
  EXPECT_FALSE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account3)));
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account4)));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       ReloadAllAccountsFromSystemWithPrimaryAccountId) {
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  ProviderAccount account2 = fake_provider_->AddAccount("gaia_2", "email_2@x");
  oauth2_delegate_->ReloadAllAccountsFromSystemWithPrimaryAccount(
      GetAccountId(account1));

  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account1)));
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account2)));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, StartRequestSuccess) {
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  oauth2_delegate_->LoadCredentials(GetAccountId(account1),
                                    /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  // Fetch access tokens.
  ResetObserverCounts();
  std::vector<std::string> scopes;
  scopes.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_delegate_->CreateAccessTokenFetcher(
          GetAccountId(account1), oauth2_delegate_->GetURLLoaderFactory(), this,
          kNoBindingChallenge));
  fetcher1->Start("foo", "bar", scopes);
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);

  ResetObserverCounts();
  fake_provider_->IssueAccessTokenForAllRequests();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, StartRequestFailure) {
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  oauth2_delegate_->LoadCredentials(GetAccountId(account1),
                                    /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  // Fetch access tokens.
  ResetObserverCounts();
  std::vector<std::string> scopes;
  scopes.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1(
      oauth2_delegate_->CreateAccessTokenFetcher(
          GetAccountId(account1), oauth2_delegate_->GetURLLoaderFactory(), this,
          kNoBindingChallenge));
  fetcher1->Start("foo", "bar", scopes);
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(0, access_token_failure_);

  ResetObserverCounts();
  fake_provider_->IssueAccessTokenErrorForAllRequests();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_);
  EXPECT_EQ(1, access_token_failure_);
}

// Verifies that UpdateAuthError does nothing after the credentials have been
// revoked.
TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       UpdateAuthErrorAfterRevokeCredentials) {
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  oauth2_delegate_->LoadCredentials(GetAccountId(account1),
                                    /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  ResetObserverCounts();
  GoogleServiceAuthError error(GoogleServiceAuthError::SERVICE_ERROR);
  oauth2_delegate_->UpdateAuthError(GetAccountId(account1), error);
  EXPECT_EQ(error, oauth2_delegate_->GetAuthError(GetAccountId(account1)));
  EXPECT_EQ(1, auth_error_changed_count_);

  oauth2_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  oauth2_delegate_->UpdateAuthError(GetAccountId(account1), error);
  EXPECT_EQ(0, auth_error_changed_count_);
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, GetAuthError) {
  // Accounts have no error by default.
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  oauth2_delegate_->LoadCredentials(GetAccountId(account1),
                                    /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_delegate_->GetAuthError(GetAccountId(account1)));
  // Update the error.
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  oauth2_delegate_->UpdateAuthError(GetAccountId(account1), error);
  EXPECT_EQ(error, oauth2_delegate_->GetAuthError(GetAccountId(account1)));
  // Unknown account has no error.
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      oauth2_delegate_->GetAuthError(CoreAccountId::FromGaiaId("gaia_2")));
}

TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest,
       OnAuthErrorChangedAfterUpdatingCredentials) {
  // Initialize delegate with an empty list of accounts.
  oauth2_delegate_->LoadCredentials(CoreAccountId(), /*is_syncing=*/false);
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  CoreAccountId account_id = GetAccountId(account1);
  testing::StrictMock<signin::MockProfileOAuth2TokenServiceObserver> observer(
      oauth2_delegate_.get());

  {
    testing::InSequence in_sequence;
    base::RunLoop run_loop;
    // `OnAuthErrorChanged()` is called *before* `OnRefreshTokenAvailable()`
    // after adding a new account on iOS.
    EXPECT_CALL(
        observer,
        OnAuthErrorChanged(account_id, GoogleServiceAuthError::AuthErrorNone(),
                           testing::_));
    EXPECT_CALL(observer, OnRefreshTokenAvailable(account_id))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    EXPECT_CALL(observer, OnEndBatchChanges());
    oauth2_delegate_->ReloadAllAccountsFromSystemWithPrimaryAccount(
        std::nullopt);
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&observer);
  }

  {
    testing::InSequence in_sequence;
    // No observer methods are called when a token is updated without changing
    // its error state.
    EXPECT_CALL(observer, OnAuthErrorChanged).Times(0);
    EXPECT_CALL(observer, OnRefreshTokenAvailable).Times(0);
    EXPECT_CALL(observer, OnEndBatchChanges).Times(0);
    oauth2_delegate_->AddOrUpdateAccount(account_id);
    testing::Mock::VerifyAndClearExpectations(&observer);
  }
}

// Tests that ProfileOAuth2TokenServiceIOSDelegate loads credentials when there
// is no primary account. kAlwaysLoadDeviceAccounts flag is enabled.
TEST_F(ProfileOAuth2TokenServiceIOSDelegateTest, LoadCredentialWhenSignedOut) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(switches::kAlwaysLoadDeviceAccounts);
  ProviderAccount account1 = fake_provider_->AddAccount("gaia_1", "email_1@x");
  ProviderAccount account2 = fake_provider_->AddAccount("gaia_2", "email_2@x");
  oauth2_delegate_->LoadCredentials(CoreAccountId(), /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(2U, oauth2_delegate_->GetAccounts().size());
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account1)));
  EXPECT_TRUE(
      oauth2_delegate_->RefreshTokenIsAvailable(GetAccountId(account2)));
}

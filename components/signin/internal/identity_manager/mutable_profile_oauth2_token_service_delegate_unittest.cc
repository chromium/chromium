// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/signin/internal/identity_manager/token_binding_helper.h"  // nogncheck
#include "components/unexportable_keys/fake_unexportable_key_service.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

class MutableProfileOAuth2TokenServiceDelegateTest
    : public testing::Test,
      public OAuth2AccessTokenConsumer,
      public ProfileOAuth2TokenServiceObserver,
      public WebDataServiceConsumer {
 public:
  MutableProfileOAuth2TokenServiceDelegateTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC),
        access_token_success_count_(0),
        access_token_failure_count_(0),
        access_token_failure_(GoogleServiceAuthError::NONE),
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        end_batch_changes_(0),
        auth_error_changed_count_(0),
        revoke_all_tokens_on_load_(false) {}

  void SetUp() override {
    OSCryptMocker::SetUp();
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    PrimaryAccountManager::RegisterProfilePrefs(pref_service_.registry());
    client_ = std::make_unique<TestSigninClient>(&pref_service_);
    client_->GetTestURLLoaderFactory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_revoke_url().spec(), "");
    LoadTokenDatabase();
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    if (oauth2_service_delegate_) {
      oauth2_service_delegate_->RemoveObserver(this);
      oauth2_service_delegate_->Shutdown();
    }
    OSCryptMocker::TearDown();
  }

  void LoadTokenDatabase() {
    base::FilePath path(WebDatabase::kInMemoryPath);
    scoped_refptr<WebDatabaseService> web_database = new WebDatabaseService(
        path, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    web_database->AddTable(std::make_unique<TokenServiceTable>());
    web_database->LoadDatabase();
    token_web_data_ = new TokenWebData(
        web_database, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    token_web_data_->Init(base::NullCallback());
  }

  void AddSuccessfulOAuhTokenResponse() {
    client_->GetTestURLLoaderFactory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        GetValidTokenResponse("token", 3600));
  }

  std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
  CreateOAuth2ServiceDelegate(
      signin::AccountConsistencyMethod account_consistency
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      std::unique_ptr<TokenBindingHelper> token_binding_helper = nullptr
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  ) {
    return std::make_unique<MutableProfileOAuth2TokenServiceDelegate>(
        client_.get(), &account_tracker_service_,
        network::TestNetworkConnectionTracker::GetInstance(), token_web_data_,
        account_consistency, revoke_all_tokens_on_load_,
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        std::move(token_binding_helper),
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
        MutableProfileOAuth2TokenServiceDelegate::FixRequestErrorCallback());
  }

  void InitializeOAuth2ServiceDelegate(
      signin::AccountConsistencyMethod account_consistency) {
    oauth2_service_delegate_ = CreateOAuth2ServiceDelegate(account_consistency);
    oauth2_service_delegate_->AddObserver(this);
  }

  void AddAuthTokenManually(const std::string& service,
                            const std::string& value) {
    if (token_web_data_)
      token_web_data_->SetTokenForService(service, value);
  }

  // WebDataServiceConsumer implementation
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override {
    DCHECK(!token_web_data_result_);
    DCHECK_EQ(TOKEN_RESULT, result->GetType());
    token_web_data_result_.reset(
        static_cast<WDResult<TokenResult>*>(result.release()));
  }

  // OAuth2AccessTokenConusmer implementation
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    ++access_token_success_count_;
  }

  void OnGetTokenFailure(const GoogleServiceAuthError& error) override {
    ++access_token_failure_count_;
    access_token_failure_ = error;
  }

  std::string GetConsumerName() const override {
    return "mutable_profile_oauth2_token_service_delegate_unittest";
  }

  // ProfileOAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override {
    ++token_available_count_;
  }
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override {
    ++token_revoked_count_;
  }
  void OnRefreshTokensLoaded() override { ++tokens_loaded_count_; }

  void OnEndBatchChanges() override { ++end_batch_changes_; }

  void OnAuthErrorChanged(const CoreAccountId& account_id,
                          const GoogleServiceAuthError& auth_error) override {
    ++auth_error_changed_count_;
  }

  // ProfileOAuth2TokenService callbacks.
  void OnRefreshTokenAvailableFromSource(const CoreAccountId& account_id,
                                         bool is_refresh_token_valid,
                                         const std::string& source) {
    source_for_refresh_token_available_ = source;
  }
  void OnRefreshTokenRevokedFromSource(const CoreAccountId& account_id,
                                       const std::string& source) {
    source_for_refresh_token_revoked_ = source;
  }

  void ResetObserverCounts() {
    token_available_count_ = 0;
    token_revoked_count_ = 0;
    tokens_loaded_count_ = 0;
    end_batch_changes_ = 0;
    auth_error_changed_count_ = 0;
  }

  void ExpectNoNotifications() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenAvailableNotification() {
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokenRevokedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(1, token_revoked_count_);
    EXPECT_EQ(0, tokens_loaded_count_);
    ResetObserverCounts();
  }

  void ExpectOneTokensLoadedNotification() {
    EXPECT_EQ(0, token_available_count_);
    EXPECT_EQ(0, token_revoked_count_);
    EXPECT_EQ(1, tokens_loaded_count_);
    ResetObserverCounts();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestSigninClient> client_;
  std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate>
      oauth2_service_delegate_;
  TestingOAuth2AccessTokenManagerConsumer consumer_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_service_;
  scoped_refptr<TokenWebData> token_web_data_;
  std::unique_ptr<WDResult<TokenResult>> token_web_data_result_;
  int access_token_success_count_;
  int access_token_failure_count_;
  GoogleServiceAuthError access_token_failure_;
  int token_available_count_;
  int token_revoked_count_;
  int tokens_loaded_count_;
  int end_batch_changes_;
  int auth_error_changed_count_;
  bool revoke_all_tokens_on_load_;
  std::string source_for_refresh_token_available_;
  std::string source_for_refresh_token_revoked_;
};

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, PersistenceDBUpgrade) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  CoreAccountId primary_account_id =
      CoreAccountId::FromGaiaId("primaryAccount");

  // Populate DB with legacy service tokens (all expected to be discarded).
  AddAuthTokenManually("chromiumsync", "syncServiceToken");
  AddAuthTokenManually("lso", "lsoToken");
  AddAuthTokenManually("kObfuscatedGaiaId", "primaryLegacyRefreshToken");

  // Force LoadCredentials.
  oauth2_service_delegate_->LoadCredentials(primary_account_id,
                                            /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  // 1. Legacy tokens get all discarded.
  // 2. Token for primary account is set to invalid as it cannot be found.
  // 3. Token for secondary account is loaded.
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(1U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account_id));
  EXPECT_EQ(GaiaConstants::kInvalidRefreshToken,
            oauth2_service_delegate_->refresh_tokens_[primary_account_id]);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceRevokeCredentials) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  CoreAccountId account_id_1 = CoreAccountId::FromGaiaId("account_id_1");
  std::string refresh_token_1 = "refresh_token_1";
  CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  std::string refresh_token_2 = "refresh_token_2";

  EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_1));
  EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_2));
  oauth2_service_delegate_->UpdateCredentials(account_id_1, refresh_token_1);
  oauth2_service_delegate_->UpdateCredentials(account_id_2, refresh_token_2);
  EXPECT_EQ(2, end_batch_changes_);

  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_2));

  ResetObserverCounts();
  oauth2_service_delegate_->RevokeCredentials(account_id_1);
  EXPECT_EQ(1, end_batch_changes_);
  ExpectOneTokenRevokedNotification();

  EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_1));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id_2));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, end_batch_changes_);
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadCredentialsStateEmptyPrimaryAccountId) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  // Ensure DB is clean.
  oauth2_service_delegate_->RevokeAllCredentials();

  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      oauth2_service_delegate_->load_credentials_state());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       RevokeAllCredentialsDuringLoad) {
  class TokenServiceForceRevokeObserver
      : public ProfileOAuth2TokenServiceObserver {
   public:
    explicit TokenServiceForceRevokeObserver(
        MutableProfileOAuth2TokenServiceDelegate* delegate)
        : delegate_(delegate) {}

    TokenServiceForceRevokeObserver(const TokenServiceForceRevokeObserver&) =
        delete;
    TokenServiceForceRevokeObserver& operator=(
        const TokenServiceForceRevokeObserver&) = delete;

    void OnRefreshTokenRevoked(const CoreAccountId& account_id) override {
      revoke_all_credentials_called_ = true;
      delegate_->RevokeAllCredentials();
    }

    raw_ptr<MutableProfileOAuth2TokenServiceDelegate> delegate_;
    bool revoke_all_credentials_called_ = false;
  };

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);

  TokenServiceForceRevokeObserver token_service_observer(
      oauth2_service_delegate_.get());
  oauth2_service_delegate_->AddObserver(&token_service_observer);

  CoreAccountId account1 = CoreAccountId::FromGaiaId("account1");
  CoreAccountId account2 = CoreAccountId::FromGaiaId("account2");

  AddAuthTokenManually("AccountId-" + account1.ToString(), "refresh_token");
  AddAuthTokenManually("AccountId-" + account2.ToString(), "refresh_token");
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(oauth2_service_delegate_->revoke_all_tokens_on_load_);
  EXPECT_TRUE(token_service_observer.revoke_all_credentials_called_);
  EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(account1));
  EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(account2));
  oauth2_service_delegate_->RemoveObserver(&token_service_observer);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceLoadCredentials) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id_2");

  // Ensure DB is clean.
  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();

  // Perform a load from an empty DB.
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials(account_id, /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(signin::LoadCredentialsState::
                LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT,
            oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_MISSING),
            oauth2_service_delegate_->GetAuthError(account_id));
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(1, auth_error_changed_count_);

  // A"tokens loaded" notification should have been fired.
  EXPECT_EQ(1, tokens_loaded_count_);

  // As the delegate puts the primary account into the token map with an invalid
  // token in the case of loading from an empty TB, a "token available"
  // notification should have been fired as well.
  EXPECT_EQ(1, token_available_count_);

  ResetObserverCounts();

  // LoadCredentials() guarantees that the account given to it as argument
  // is in the refresh_token map.
  EXPECT_EQ(1U, oauth2_service_delegate_->refresh_tokens_.size());
  EXPECT_EQ(GaiaConstants::kInvalidRefreshToken,
            oauth2_service_delegate_->refresh_tokens_[account_id]);
  // Setup a DB with tokens that don't require upgrade and clear memory.
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");
  oauth2_service_delegate_->refresh_tokens_.clear();
  oauth2_service_delegate_->ClearAuthError(absl::nullopt);
  EXPECT_EQ(2, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  oauth2_service_delegate_->LoadCredentials(account_id, /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id2));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0, auth_error_changed_count_);
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       PersistenceLoadCredentialsEmptyPrimaryAccountId_DiceEnabled) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id_2");

  // Ensure DB is clean.
  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  // Perform a load from an empty DB.
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0, auth_error_changed_count_);
  ExpectOneTokensLoadedNotification();

  // No account should be present in the refresh token as no primary account
  // was passed to the token service.
  EXPECT_TRUE(oauth2_service_delegate_->refresh_tokens_.empty());

  // Setup a DB with tokens that don't require upgrade and clear memory.
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");
  oauth2_service_delegate_->refresh_tokens_.clear();
  oauth2_service_delegate_->ClearAuthError(absl::nullopt);
  EXPECT_EQ(2, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      oauth2_service_delegate_->load_credentials_state());
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id2));

  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(0, tokens_loaded_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0, auth_error_changed_count_);
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadCredentialsClearsTokenDBWhenNoPrimaryAccount_DiceDisabled) {
  // Populate DB with 2 valid tokens.
  AddAuthTokenManually("AccountId-12345", "refresh_token");
  AddAuthTokenManually("AccountId-67890", "refresh_token");

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->LoadCredentials(
      /*primary_account_id=*/CoreAccountId(), /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  // No tokens were loaded.
  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_EQ(0U, oauth2_service_delegate_->refresh_tokens_.size());

  // Handle to the request reading tokens from database.
  token_web_data_->GetAllTokens(this);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(token_web_data_result_.get());
  ASSERT_EQ(0u, token_web_data_result_->GetValue().tokens.size());
}

// Tests that calling UpdateCredentials revokes the old token, without sending
// the notification.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, RevokeOnUpdate) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  // Add a token.
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  ASSERT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  ExpectOneTokenAvailableNotification();

  // Updating the token does not revoke the old one.
  // Regression test for http://crbug.com/865189
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token2");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  ExpectOneTokenAvailableNotification();

  // Flush the server revokes.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Set the same token again.
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token2");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  ExpectNoNotifications();

  // Clear the token.
  oauth2_service_delegate_->RevokeAllCredentials();
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  ExpectOneTokenRevokedNotification();

  // Flush the server revokes.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, DelayedRevoke) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  client_->SetNetworkCallsDelayed(true);
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  oauth2_service_delegate_->RevokeCredentials(account_id);

  // The revoke does not start until network calls are unblocked.
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());

  // Unblock network calls, and check that the revocation goes through.
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ShutdownDuringRevoke) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  // Shutdown cancels the revocation.
  client_->SetNetworkCallsDelayed(true);
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  oauth2_service_delegate_->RevokeCredentials(account_id);
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());

  // Shutdown.
  oauth2_service_delegate_->Shutdown();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Unblocking network calls after shutdown does not crash.
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, RevokeRetries) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  const std::string url = GaiaUrls::GetInstance()->oauth2_revoke_url().spec();
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  // Revokes will remain in "pending" state.
  client_->GetTestURLLoaderFactory()->ClearResponses();

  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_FALSE(client_->GetTestURLLoaderFactory()->IsPending(url));

  oauth2_service_delegate_->RevokeCredentials(account_id);
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  EXPECT_TRUE(client_->GetTestURLLoaderFactory()->IsPending(url));
  // Fail and retry.
  client_->GetTestURLLoaderFactory()->SimulateResponseForPendingRequest(
      url, std::string(), net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_TRUE(client_->GetTestURLLoaderFactory()->IsPending(url));
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  // Fail and retry.
  client_->GetTestURLLoaderFactory()->SimulateResponseForPendingRequest(
      url, std::string(), net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_TRUE(client_->GetTestURLLoaderFactory()->IsPending(url));
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  // Do not retry after third attempt.
  client_->GetTestURLLoaderFactory()->SimulateResponseForPendingRequest(
      url, std::string(), net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_FALSE(client_->GetTestURLLoaderFactory()->IsPending(url));
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // No retry after success.
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  oauth2_service_delegate_->RevokeCredentials(account_id);
  EXPECT_EQ(1u, oauth2_service_delegate_->server_revokes_.size());
  EXPECT_TRUE(client_->GetTestURLLoaderFactory()->IsPending(url));
  client_->GetTestURLLoaderFactory()->SimulateResponseForPendingRequest(
      url, std::string(), net::HTTP_OK);
  EXPECT_FALSE(client_->GetTestURLLoaderFactory()->IsPending(url));
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, UpdateInvalidToken) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  // Add the invalid token.
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  ASSERT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  oauth2_service_delegate_->UpdateCredentials(
      account_id, GaiaConstants::kInvalidRefreshToken);
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_EQ(1, auth_error_changed_count_);
  ExpectOneTokenAvailableNotification();

  // The account is in authentication error.
  EXPECT_EQ(GoogleServiceAuthError(
                GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                    GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                        CREDENTIALS_REJECTED_BY_CLIENT)),
            oauth2_service_delegate_->GetAuthError(account_id));

  // Update the token: authentication error is fixed, no actual server
  // revocation.
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_EQ(1, auth_error_changed_count_);
  ExpectOneTokenAvailableNotification();
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       InvalidateTokensForMultilogin) {
  class TokenServiceErrorObserver : public ProfileOAuth2TokenServiceObserver {
   public:
    MOCK_METHOD2(OnAuthErrorChanged,
                 void(const CoreAccountId&, const GoogleServiceAuthError&));
  };

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  TokenServiceErrorObserver observer;
  oauth2_service_delegate_->AddObserver(&observer);

  const CoreAccountId account_id1 = CoreAccountId::FromGaiaId("account_id1");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");

  // This will be fired from UpdateCredentials.
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(::testing::_, GoogleServiceAuthError::AuthErrorNone()))
      .Times(2);
  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");

  testing::Mock::VerifyAndClearExpectations(&observer);

  // This should be fired after error is set.
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(account_id1,
                         GoogleServiceAuthError(
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)))
      .Times(1);

  oauth2_service_delegate_->InvalidateTokenForMultilogin(account_id1);
  EXPECT_EQ(oauth2_service_delegate_->GetAuthError(account_id1).state(),
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_EQ(oauth2_service_delegate_->GetAuthError(account_id2).state(),
            GoogleServiceAuthError::NONE);

  oauth2_service_delegate_->RemoveObserver(&observer);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, LoadInvalidToken) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, std::string> tokens;
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  tokens["AccountId-account_id"] = GaiaConstants::kInvalidRefreshToken;

  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id));
  EXPECT_STREQ(GaiaConstants::kInvalidRefreshToken,
               oauth2_service_delegate_->GetRefreshToken(account_id).c_str());

  // The account is in authentication error.
  EXPECT_EQ(GoogleServiceAuthError(
                GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                    GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                        CREDENTIALS_REJECTED_BY_CLIENT)),
            oauth2_service_delegate_->GetAuthError(account_id));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GetTokenForMultilogin) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  const CoreAccountId account_id1 = CoreAccountId::FromGaiaId("account_id1");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");

  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");
  oauth2_service_delegate_->UpdateAuthError(
      account_id2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  EXPECT_EQ(oauth2_service_delegate_->GetTokenForMultilogin(account_id1),
            "refresh_token1");
  EXPECT_EQ(oauth2_service_delegate_->GetTokenForMultilogin(account_id2),
            std::string());
  EXPECT_EQ(oauth2_service_delegate_->GetTokenForMultilogin(
                CoreAccountId::FromGaiaId("unknown account")),
            std::string());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, PersistenceNotifications) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  ExpectNoNotifications();

  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->RevokeCredentials(account_id);
  ExpectOneTokenRevokedNotification();

  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token2");
  ExpectOneTokenAvailableNotification();

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GetAccounts) {
  const CoreAccountId account_id1 = CoreAccountId::FromGaiaId("account_id1");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  EXPECT_TRUE(oauth2_service_delegate_->GetAccounts().empty());

  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");
  std::vector<CoreAccountId> accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), account_id1));
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), account_id2));
  oauth2_service_delegate_->RevokeCredentials(account_id2);
  accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), account_id1));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, FetchPersistentError) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(account_id, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));

  GoogleServiceAuthError authfail(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  oauth2_service_delegate_->UpdateAuthError(account_id, authfail);
  EXPECT_NE(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));

  // Create a "success" fetch we don't expect to get called.
  AddSuccessfulOAuhTokenResponse();

  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this);
  fetcher->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, RetryBackoff) {
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  oauth2_service_delegate_->UpdateCredentials(account_id, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));

  GoogleServiceAuthError authfail(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  oauth2_service_delegate_->UpdateAuthError(account_id, authfail);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));

  // Create a "success" fetch we don't expect to get called just yet.
  AddSuccessfulOAuhTokenResponse();

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1 =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this);
  fetcher1->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
  // Expect a positive backoff time.
  EXPECT_GT(oauth2_service_delegate_->BackoffEntry()->GetTimeUntilRelease(),
            base::TimeDelta());

  // Pretend that backoff has expired and try again.
  oauth2_service_delegate_->backoff_entry_->SetCustomReleaseTime(
      base::TimeTicks());
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher2 =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this);
  fetcher2->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ResetBackoff) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  oauth2_service_delegate_->UpdateCredentials(account_id, "refreshToken");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));

  GoogleServiceAuthError authfail(GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  oauth2_service_delegate_->UpdateAuthError(account_id, authfail);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));

  // Create a "success" fetch we don't expect to get called just yet.
  AddSuccessfulOAuhTokenResponse();

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1 =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this);
  fetcher1->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);

  // Notify of network change and ensure that request now runs.
  oauth2_service_delegate_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher2 =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this);
  fetcher2->Start("foo", "bar", scope_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, CanonicalizeAccountId) {
  pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                           AccountTrackerService::MIGRATION_NOT_STARTED);
  pref_service_.SetBoolean(prefs::kTokenServiceDiceCompatible, true);
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, std::string> tokens;
  tokens["AccountId-user@gmail.com"] = "refresh_token";
  tokens["AccountId-Foo.Bar@gmail.com"] = "refresh_token";
  tokens["AccountId-12345"] = "refresh_token";

  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      CoreAccountId::FromEmail("user@gmail.com")));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      CoreAccountId::FromEmail("foobar@gmail.com")));
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      CoreAccountId::FromGaiaId("12345")));
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       CanonAndNonCanonAccountId) {
  pref_service_.SetBoolean(prefs::kTokenServiceDiceCompatible, true);
  pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                           AccountTrackerService::MIGRATION_NOT_STARTED);
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, std::string> tokens;
  tokens["AccountId-Foo.Bar@gmail.com"] = "bad_token";
  tokens["AccountId-foobar@gmail.com"] = "good_token";

  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);

  EXPECT_EQ(1u, oauth2_service_delegate_->GetAccounts().size());
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(
      CoreAccountId::FromEmail("foobar@gmail.com")));
  EXPECT_STREQ(
      "good_token",
      oauth2_service_delegate_
          ->GetRefreshToken(CoreAccountId::FromEmail("foobar@gmail.com"))
          .c_str());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ShutdownService) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  EXPECT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  const CoreAccountId account_id1("account_id1");
  const CoreAccountId account_id2("account_id2");

  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");
  std::vector<CoreAccountId> accounts = oauth2_service_delegate_->GetAccounts();
  EXPECT_EQ(2u, accounts.size());
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), account_id1));
  EXPECT_EQ(1, count(accounts.begin(), accounts.end(), account_id2));
  oauth2_service_delegate_->LoadCredentials(account_id1, /*is_syncing=*/false);
  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token3");
  oauth2_service_delegate_->Shutdown();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_TRUE(oauth2_service_delegate_->refresh_tokens_.empty());
  EXPECT_EQ(0, oauth2_service_delegate_->web_data_service_request_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GaiaIdMigration) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  if (account_tracker_service_.GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email = "foo@gmail.com";
    std::string gaia_id = "foo's gaia id";
    const CoreAccountId acc_id_email(email);
    const CoreAccountId acc_id_gaia_id(gaia_id);

    pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);

    ScopedListPrefUpdate update(&pref_service_, prefs::kAccountInfo);
    update->clear();
    update->Append(base::Value::Dict()
                       .Set("account_id", email)
                       .Set("email", email)
                       .Set("gaia", gaia_id));
    account_tracker_service_.ResetForTesting();

    AddAuthTokenManually("AccountId-" + email, "refresh_token");
    oauth2_service_delegate_->LoadCredentials(acc_id_gaia_id,
                                              /*is_syncing=*/false);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(1, end_batch_changes_);

    std::vector<CoreAccountId> accounts =
        oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(1u, accounts.size());

    EXPECT_FALSE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(acc_id_email));
    EXPECT_TRUE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(acc_id_gaia_id));

    account_tracker_service_.SetMigrationDone();
    oauth2_service_delegate_->Shutdown();
    ResetObserverCounts();

    oauth2_service_delegate_->LoadCredentials(acc_id_gaia_id,
                                              /*is_syncing=*/false);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_EQ(1, end_batch_changes_);

    EXPECT_FALSE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(acc_id_email));
    EXPECT_TRUE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(acc_id_gaia_id));
    accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(1u, accounts.size());
  }
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       GaiaIdMigrationCrashInTheMiddle) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  if (account_tracker_service_.GetMigrationState() !=
      AccountTrackerService::MIGRATION_NOT_STARTED) {
    std::string email1 = "foo@gmail.com";
    std::string gaia_id1 = "foo's gaia id";
    std::string email2 = "bar@gmail.com";
    std::string gaia_id2 = "bar's gaia id";
    const CoreAccountId acc_email1(email1);
    const CoreAccountId acc_email2(email2);
    const CoreAccountId acc_gaia1(gaia_id1);
    const CoreAccountId acc_gaia2(gaia_id2);

    pref_service_.SetInteger(prefs::kAccountIdMigrationState,
                             AccountTrackerService::MIGRATION_NOT_STARTED);

    ScopedListPrefUpdate update(&pref_service_, prefs::kAccountInfo);
    update->clear();
    base::Value::Dict account1;
    account1.Set("account_id", email1);
    account1.Set("email", email1);
    account1.Set("gaia", gaia_id1);
    update->Append(std::move(account1));
    base::Value::Dict account2;
    account2.Set("account_id", email2);
    account2.Set("email", email2);
    account2.Set("gaia", gaia_id2);
    update->Append(std::move(account2));
    account_tracker_service_.ResetForTesting();

    AddAuthTokenManually("AccountId-" + email1, "refresh_token");
    AddAuthTokenManually("AccountId-" + email2, "refresh_token");
    AddAuthTokenManually("AccountId-" + gaia_id1, "refresh_token");
    oauth2_service_delegate_->LoadCredentials(acc_gaia1, /*is_syncing=*/false);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(2, token_available_count_);
    EXPECT_EQ(1, end_batch_changes_);

    std::vector<CoreAccountId> accounts =
        oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(2u, accounts.size());

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_email1));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_gaia1));
    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_email2));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_gaia2));

    account_tracker_service_.SetMigrationDone();
    oauth2_service_delegate_->Shutdown();
    ResetObserverCounts();

    oauth2_service_delegate_->LoadCredentials(acc_gaia1, /*is_syncing=*/false);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(2, token_available_count_);
    EXPECT_EQ(1, end_batch_changes_);

    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_email1));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_gaia1));
    EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_email2));
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(acc_gaia2));
    accounts = oauth2_service_delegate_->GetAccounts();
    EXPECT_EQ(2u, accounts.size());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadPrimaryAccountOnlyWhenAccountConsistencyDisabled) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  CoreAccountId primary_account = CoreAccountId::FromGaiaId("primaryaccount");
  CoreAccountId secondary_account =
      CoreAccountId::FromGaiaId("secondaryaccount");

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.ToString(),
                       "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account.ToString(),
                       "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account,
                                            /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
}

// Regression test for https://crbug.com/823707
// Checks that OnAuthErrorChanged() is called during UpdateCredentials(), and
// that RefreshTokenIsAvailable() can be used at this time.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, OnAuthErrorChanged) {
  class TokenServiceErrorObserver : public ProfileOAuth2TokenServiceObserver {
   public:
    explicit TokenServiceErrorObserver(
        MutableProfileOAuth2TokenServiceDelegate* delegate)
        : delegate_(delegate) {}

    TokenServiceErrorObserver(const TokenServiceErrorObserver&) = delete;
    TokenServiceErrorObserver& operator=(const TokenServiceErrorObserver&) =
        delete;

    void OnAuthErrorChanged(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& auth_error) override {
      error_changed_ = true;
      EXPECT_EQ("account_id", account_id.ToString());
      EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), auth_error);
      EXPECT_TRUE(delegate_->RefreshTokenIsAvailable(account_id));
      EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
                delegate_->GetAuthError(account_id));
    }

    raw_ptr<MutableProfileOAuth2TokenServiceDelegate> delegate_;
    bool error_changed_ = false;
  };

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);

  // Start with the SigninErrorController in error state, so that it calls
  // OnErrorChanged() from AddProvider().
  oauth2_service_delegate_->UpdateCredentials(
      CoreAccountId::FromGaiaId("error_account_id"),
      GaiaConstants::kInvalidRefreshToken);

  TokenServiceErrorObserver token_service_observer(
      oauth2_service_delegate_.get());
  oauth2_service_delegate_->AddObserver(&token_service_observer);

  ASSERT_FALSE(token_service_observer.error_changed_);
  oauth2_service_delegate_->UpdateCredentials(
      CoreAccountId::FromGaiaId("account_id"), "token");
  EXPECT_TRUE(token_service_observer.error_changed_);

  oauth2_service_delegate_->RemoveObserver(&token_service_observer);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, GetAuthError) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  // Accounts have no error by default.
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");

  oauth2_service_delegate_->UpdateCredentials(account_id, "refresh_token");
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(account_id));
  // Update the error.
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  oauth2_service_delegate_->UpdateAuthError(account_id, error);
  EXPECT_EQ(error, oauth2_service_delegate_->GetAuthError(account_id));
  // Unknown account has no error.
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      oauth2_service_delegate_->GetAuthError(CoreAccountId::FromGaiaId("foo")));
  // Add account with invalid token.
  oauth2_service_delegate_->UpdateCredentials(
      account_id_2, GaiaConstants::kInvalidRefreshToken);
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT),
            oauth2_service_delegate_->GetAuthError(account_id_2));
}

// Checks that OnAuthErrorChanged() is called before OnRefreshTokenAvailable,
// and that the error state is correctly available from within both calls.
// Regression test for https://crbug.com/824791.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       InvalidTokenObserverCallsOrdering) {
  class TokenServiceErrorObserver : public ProfileOAuth2TokenServiceObserver {
   public:
    explicit TokenServiceErrorObserver(
        MutableProfileOAuth2TokenServiceDelegate* delegate)
        : delegate_(delegate) {}

    TokenServiceErrorObserver(const TokenServiceErrorObserver&) = delete;
    TokenServiceErrorObserver& operator=(const TokenServiceErrorObserver&) =
        delete;

    void OnAuthErrorChanged(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& auth_error) override {
      error_changed_ = true;
      EXPECT_FALSE(token_available_)
          << "OnAuthErrorChanged() should be called first";
      EXPECT_EQ(auth_error, delegate_->GetAuthError(account_id));
      CheckTokenState(account_id);
    }

    void OnRefreshTokenAvailable(const CoreAccountId& account_id) override {
      token_available_ = true;
      EXPECT_TRUE(error_changed_)
          << "OnAuthErrorChanged() should be called first";
      CheckTokenState(account_id);
    }

    void CheckTokenState(const CoreAccountId& account_id) {
      EXPECT_EQ("account_id", account_id.ToString());
      EXPECT_TRUE(delegate_->RefreshTokenIsAvailable(account_id));
      EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                    GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                        CREDENTIALS_REJECTED_BY_CLIENT),
                delegate_->GetAuthError(account_id));
    }

    raw_ptr<MutableProfileOAuth2TokenServiceDelegate> delegate_;
    bool error_changed_ = false;
    bool token_available_ = false;
  };

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  TokenServiceErrorObserver token_service_observer(
      oauth2_service_delegate_.get());
  oauth2_service_delegate_->AddObserver(&token_service_observer);
  oauth2_service_delegate_->UpdateCredentials(
      CoreAccountId::FromGaiaId("account_id"),
      GaiaConstants::kInvalidRefreshToken);
  EXPECT_TRUE(token_service_observer.token_available_);
  EXPECT_TRUE(token_service_observer.error_changed_);
  oauth2_service_delegate_->RemoveObserver(&token_service_observer);
}

// Checks that set_revoke_all_tokens_on_first_load() revokes the tokens,
// updates the database, and is applied only once.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ClearTokensOnStartup) {
  client_->SetNetworkCallsDelayed(true);
  revoke_all_tokens_on_load_ = true;
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  CoreAccountId primary_account = CoreAccountId::FromGaiaId("primaryaccount");
  CoreAccountId secondary_account =
      CoreAccountId::FromGaiaId("secondaryaccount");

  oauth2_service_delegate_->RevokeAllCredentials();
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.ToString(),
                       "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account.ToString(),
                       "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account,
                                            /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(1, token_available_count_);
  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
  EXPECT_STREQ(
      GaiaConstants::kInvalidRefreshToken,
      oauth2_service_delegate_->GetRefreshToken(primary_account).c_str());
  EXPECT_EQ(GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_CLIENT),
            oauth2_service_delegate_->GetAuthError(primary_account));

  // Tokens are revoked on the server.
  EXPECT_EQ(2u, oauth2_service_delegate_->server_revokes_.size());
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Check that the changes have been persisted in the database: tokens are not
  // revoked again on the server.
  client_->SetNetworkCallsDelayed(true);
  oauth2_service_delegate_->LoadCredentials(primary_account,
                                            /*is_syncing=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
  EXPECT_STREQ(
      GaiaConstants::kInvalidRefreshToken,
      oauth2_service_delegate_->GetRefreshToken(primary_account).c_str());
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

// Tests that ProfileOAuthTokenService refresh token operations correctly pass
// the source when used with a |MutableProfileOAuth2TokenServiceDelegate|
// delegate.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       SourceForRefreshTokenOperations) {
  using Source = signin_metrics::SourceForRefreshTokenOperation;
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  ProfileOAuth2TokenService token_service(
      &pref_service_,
      CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled));
  token_service.SetRefreshTokenAvailableFromSourceCallback(
      base::BindRepeating(&MutableProfileOAuth2TokenServiceDelegateTest::
                              OnRefreshTokenAvailableFromSource,
                          base::Unretained(this)));
  token_service.SetRefreshTokenRevokedFromSourceCallback(
      base::BindRepeating(&MutableProfileOAuth2TokenServiceDelegateTest::
                              OnRefreshTokenRevokedFromSource,
                          base::Unretained(this)));

  {
    base::HistogramTester h_tester;
    AddAuthTokenManually("account_id", "refresh_token");
    token_service.LoadCredentials(account_id, /*is_syncing=*/false);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ("TokenService::LoadCredentials",
              source_for_refresh_token_available_);
    h_tester.ExpectUniqueSample(
        "Signin.RefreshTokenUpdated.ToValidToken.Source",
        Source::kTokenService_LoadCredentials, 1);
  }

  {
    base::HistogramTester h_tester;
    token_service.UpdateCredentials(account_id, "refresh_token",
                                    Source::kInlineLoginHandler_Signin);
    EXPECT_EQ("InlineLoginHandler::Signin",
              source_for_refresh_token_available_);
    h_tester.ExpectUniqueSample(
        "Signin.RefreshTokenUpdated.ToValidToken.Source",
        Source::kInlineLoginHandler_Signin, 1);

    token_service.RevokeCredentials(
        account_id, Source::kAccountReconcilor_GaiaCookiesUpdated);
    EXPECT_EQ("AccountReconcilor::GaiaCookiesUpdated",
              source_for_refresh_token_revoked_);
    h_tester.ExpectUniqueSample("Signin.RefreshTokenRevoked.Source",
                                Source::kAccountReconcilor_GaiaCookiesUpdated,
                                1);
    base::RunLoop().RunUntilIdle();
  }

  {
    base::HistogramTester h_tester;
    token_service.UpdateCredentials(CoreAccountId::FromGaiaId("account_id_1"),
                                    "refresh_token",
                                    Source::kDiceResponseHandler_Signin);
    EXPECT_EQ("DiceResponseHandler::Signin",
              source_for_refresh_token_available_);
    h_tester.ExpectUniqueSample(
        "Signin.RefreshTokenUpdated.ToValidToken.Source",
        Source::kDiceResponseHandler_Signin, 1);

    token_service.UpdateCredentials(CoreAccountId::FromGaiaId("account_id_2"),
                                    GaiaConstants::kInvalidRefreshToken,
                                    Source::kDiceResponseHandler_Signin);
    EXPECT_EQ("DiceResponseHandler::Signin",
              source_for_refresh_token_available_);
    h_tester.ExpectUniqueSample(
        "Signin.RefreshTokenUpdated.ToInvalidToken.Source",
        Source::kDiceResponseHandler_Signin, 1);

    token_service.RevokeAllCredentials(Source::kDiceResponseHandler_Signout);
    EXPECT_EQ("DiceResponseHandler::Signout",
              source_for_refresh_token_revoked_);
    h_tester.ExpectUniqueSample("Signin.RefreshTokenRevoked.Source",
                                Source::kDiceResponseHandler_Signout, 2);
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ExtractCredentials) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");

  // Create another token service
  sync_preferences::TestingPrefServiceSyncable prefs;
  ProfileOAuth2TokenService::RegisterProfilePrefs(prefs.registry());
  std::unique_ptr<FakeProfileOAuth2TokenServiceDelegate> delegate =
      std::make_unique<FakeProfileOAuth2TokenServiceDelegate>();
  FakeProfileOAuth2TokenServiceDelegate* other_delegate = delegate.get();
  ProfileOAuth2TokenService other_token_service(&prefs, std::move(delegate));
  other_token_service.LoadCredentials(CoreAccountId(), /*is_syncing=*/false);

  // Add credentials to the first token service delegate.
  oauth2_service_delegate_->UpdateCredentials(account_id, "token");

  // Extract the credentials.
  ResetObserverCounts();
  oauth2_service_delegate_->ExtractCredentials(&other_token_service,
                                               account_id);

  EXPECT_EQ(1, token_revoked_count_);
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
  EXPECT_FALSE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id));
  EXPECT_TRUE(other_delegate->RefreshTokenIsAvailable(account_id));
  EXPECT_EQ("token", other_delegate->GetRefreshToken(account_id));
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, UpdateBoundToken) {
  unexportable_keys::FakeUnexportableKeyService fake_unexportable_key_service;
  auto token_binding_helper =
      std::make_unique<TokenBindingHelper>(fake_unexportable_key_service);
  std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate> delegate =
      CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled,
                                  std::move(token_binding_helper));
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  EXPECT_TRUE(delegate->GetWrappedBindingKey(account_id).empty());

  // Set bound refresh token.
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  delegate->UpdateCredentials(account_id, "refresh_token",
                              kFakeWrappedBindingKey);
  EXPECT_EQ(delegate->GetWrappedBindingKey(account_id), kFakeWrappedBindingKey);

  // Update bound refresh token.
  const std::vector<uint8_t> kFakeWrappedBindingKey2 = {4, 5, 6};
  delegate->UpdateCredentials(account_id, "refresh_token2",
                              kFakeWrappedBindingKey2);
  EXPECT_EQ(delegate->GetWrappedBindingKey(account_id),
            kFakeWrappedBindingKey2);

  // Invalidate bound refresh token.
  delegate->UpdateCredentials(account_id, GaiaConstants::kInvalidRefreshToken);
  EXPECT_TRUE(delegate->GetWrappedBindingKey(account_id).empty());
  delegate->Shutdown();
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, RevokeBoundToken) {
  unexportable_keys::FakeUnexportableKeyService fake_unexportable_key_service;
  auto token_binding_helper =
      std::make_unique<TokenBindingHelper>(fake_unexportable_key_service);
  std::unique_ptr<MutableProfileOAuth2TokenServiceDelegate> delegate =
      CreateOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled,
                                  std::move(token_binding_helper));
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  const std::vector<uint8_t> kFakeWrappedBindingKey2 = {4, 5, 6};
  delegate->UpdateCredentials(account_id, "refresh_token",
                              kFakeWrappedBindingKey);
  delegate->UpdateCredentials(account_id2, "refresh_token2",
                              kFakeWrappedBindingKey2);

  delegate->RevokeCredentials(account_id);
  EXPECT_TRUE(delegate->GetWrappedBindingKey(account_id).empty());
  EXPECT_EQ(delegate->GetWrappedBindingKey(account_id2),
            kFakeWrappedBindingKey2);
  delegate->Shutdown();
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

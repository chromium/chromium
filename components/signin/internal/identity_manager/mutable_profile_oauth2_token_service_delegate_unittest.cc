// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/mutable_profile_oauth2_token_service_delegate.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/mock_profile_oauth2_token_service_observer.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/webdata/token_service_table.h"
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
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/signin/internal/identity_manager/token_binding_helper.h"  // nogncheck
#include "components/unexportable_keys/fake_unexportable_key_service.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

using TokenWithBindingKey = TokenServiceTable::TokenWithBindingKey;

namespace {
constexpr char kTestTokenDatabase[] = "TestTokenDatabase";
constexpr char kNoBindingChallenge[] = "";
}

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
        os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)),
        access_token_success_count_(0),
        access_token_failure_count_(0),
        access_token_failure_(GoogleServiceAuthError::NONE),
        token_available_count_(0),
        token_revoked_count_(0),
        tokens_loaded_count_(0),
        end_batch_changes_(0),
        auth_error_changed_count_(0),
        revoke_all_tokens_on_load_(RevokeAllTokensOnLoad::kNo) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    PrimaryAccountManager::RegisterProfilePrefs(pref_service_.registry());
    client_ = std::make_unique<TestSigninClient>(&pref_service_);
    client_->GetTestURLLoaderFactory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_revoke_url().spec(), "");
    LoadTokenDatabase();
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());
  }

  void TearDown() override {
    UnloadTokenDatabase();
    if (oauth2_service_delegate_) {
      test_service_observation_.Reset();
      oauth2_service_delegate_->Shutdown();
    }
  }

  void UnloadTokenDatabase() {
    if (token_web_data_) {
      token_web_data_->ShutdownDatabase();
      token_web_data_.reset();
    }
    base::RunLoop().RunUntilIdle();
  }

  void LoadTokenDatabase() {
    scoped_refptr<WebDatabaseService> web_database = new WebDatabaseService(
        temp_dir_.GetPath().AppendASCII(kTestTokenDatabase),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    web_database->AddTable(std::make_unique<TokenServiceTable>());
    web_database->LoadDatabase(os_crypt_.get());
    token_web_data_ = new TokenWebData(
        web_database, base::SingleThreadTaskRunner::GetCurrentDefault());
    token_web_data_->Init(base::NullCallback());
  }

  void AddSuccessfulOAuthTokenResponse() {
    client_->GetTestURLLoaderFactory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        GetValidTokenResponse("token", 3600));
  }

  void AddSuccessfulBoundTokenResponse() {
    client_->GetTestURLLoaderFactory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_issue_token_url().spec(),
        GetValidBoundTokenResponse("access_token", base::Seconds(3600),
                                   {"scope"}));
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
    oauth2_service_delegate_->SetOnRefreshTokenRevokedNotified(
        base::DoNothing());
    test_service_observation_.Observe(oauth2_service_delegate_.get());
  }

  void AddAuthTokenManually(const std::string& service,
                            const std::string& value,
                            const std::vector<uint8_t>& binding_key = {}) {
    if (token_web_data_) {
      token_web_data_->SetTokenForService(service, value, binding_key);
    }
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
    get_token_completed_loop_->Quit();
  }

  void OnGetTokenFailure(const GoogleServiceAuthError& error) override {
    ++access_token_failure_count_;
    access_token_failure_ = error;
    get_token_completed_loop_->Quit();
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
  void OnRefreshTokensLoaded() override {
    ++tokens_loaded_count_;
    refresh_tokens_loaded_loop_->Quit();
  }

  void OnEndBatchChanges() override { ++end_batch_changes_; }

  void OnAuthErrorChanged(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& auth_error,
      signin_metrics::SourceForRefreshTokenOperation source) override {
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

  void WaitForRefreshTokensLoaded() {
    refresh_tokens_loaded_loop_->Run();
    refresh_tokens_loaded_loop_ = std::make_unique<base::RunLoop>();
  }

  void WaitForGetTokenCompleted() {
    get_token_completed_loop_->Run();
    get_token_completed_loop_ = std::make_unique<base::RunLoop>();
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
  base::ScopedObservation<ProfileOAuth2TokenServiceDelegate,
                          ProfileOAuth2TokenServiceObserver>
      test_service_observation_{this};
  TestingOAuth2AccessTokenManagerConsumer consumer_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_service_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
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
  RevokeAllTokensOnLoad revoke_all_tokens_on_load_;
  std::unique_ptr<base::RunLoop> refresh_tokens_loaded_loop_{
      std::make_unique<base::RunLoop>()};
  std::unique_ptr<base::RunLoop> get_token_completed_loop_{
      std::make_unique<base::RunLoop>()};
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
  WaitForRefreshTokensLoaded();

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
  // Verify DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());

  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();
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
  WaitForRefreshTokensLoaded();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(0, token_available_count_);
  EXPECT_EQ(2, token_revoked_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_NE(RevokeAllTokensOnLoad::kNo,
            oauth2_service_delegate_->revoke_all_tokens_on_load_);
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

  // Verify DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  ResetObserverCounts();

  // Perform a load from an empty DB.
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials(account_id, /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  WaitForRefreshTokensLoaded();
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
  oauth2_service_delegate_->ClearAuthError(std::nullopt);
  EXPECT_EQ(2, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  oauth2_service_delegate_->LoadCredentials(account_id, /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  WaitForRefreshTokensLoaded();
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

  // Verify DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  ResetObserverCounts();
  // Perform a load from an empty DB.
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            oauth2_service_delegate_->load_credentials_state());
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  WaitForRefreshTokensLoaded();
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
  oauth2_service_delegate_->ClearAuthError(std::nullopt);
  EXPECT_EQ(2, end_batch_changes_);
  EXPECT_EQ(2, auth_error_changed_count_);
  ResetObserverCounts();

  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  EXPECT_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            oauth2_service_delegate_->load_credentials_state());
  WaitForRefreshTokensLoaded();
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
  WaitForRefreshTokensLoaded();

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
    MOCK_METHOD3(OnAuthErrorChanged,
                 void(const CoreAccountId&,
                      const GoogleServiceAuthError&,
                      signin_metrics::SourceForRefreshTokenOperation source));
  };

  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  TokenServiceErrorObserver observer;
  oauth2_service_delegate_->AddObserver(&observer);

  const CoreAccountId account_id1 = CoreAccountId::FromGaiaId("account_id1");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");

  // This will be fired from UpdateCredentials.
  EXPECT_CALL(observer,
              OnAuthErrorChanged(
                  ::testing::_, GoogleServiceAuthError::AuthErrorNone(),
                  signin_metrics::SourceForRefreshTokenOperation::kUnknown))
      .Times(2);
  oauth2_service_delegate_->UpdateCredentials(account_id1, "refresh_token1");
  oauth2_service_delegate_->UpdateCredentials(account_id2, "refresh_token2");

  testing::Mock::VerifyAndClearExpectations(&observer);

  // This should be fired after error is set.
  EXPECT_CALL(observer,
              OnAuthErrorChanged(
                  account_id1,
                  GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                          CREDENTIALS_REJECTED_BY_SERVER),
                  signin_metrics::SourceForRefreshTokenOperation::kUnknown));

  oauth2_service_delegate_->InvalidateTokenForMultilogin(account_id1);
  EXPECT_EQ(oauth2_service_delegate_->GetAuthError(account_id1).state(),
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_EQ(oauth2_service_delegate_->GetAuthError(account_id2).state(),
            GoogleServiceAuthError::NONE);

  oauth2_service_delegate_->RemoveObserver(&observer);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, LoadInvalidToken) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, TokenWithBindingKey> tokens;
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  tokens["AccountId-account_id"] =
      TokenWithBindingKey(GaiaConstants::kInvalidRefreshToken);

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

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadAllCredentialsIntoMemoryAccountAvailabilityPrimaryAvailable) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, TokenWithBindingKey> tokens;
  const std::string gaia_id = "gaia_id";
  const CoreAccountId account_id = CoreAccountId::FromGaiaId(gaia_id);
  tokens["AccountId-gaia_id"] = TokenWithBindingKey("refresh_token");

  // Primary account is available in account tracker service.
  account_tracker_service_.SeedAccountInfo(gaia_id, "test@google.com");
  oauth2_service_delegate_->loading_primary_account_id_ = account_id;

  base::HistogramTester histogram_tester;
  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountInPref.StartupState.Primary",
      AccountStartupState::kKnownValidToken, 1);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadAllCredentialsIntoMemoryAccountAvailabilityPrimaryNotAvailable) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, TokenWithBindingKey> tokens;
  const std::string gaia_id = "gaia_id";
  const CoreAccountId account_id = CoreAccountId::FromGaiaId(gaia_id);
  tokens["AccountId-gaia_id"] =
      TokenWithBindingKey(GaiaConstants::kInvalidRefreshToken);

  // Primary account is not seeded in the account tracker service.
  oauth2_service_delegate_->loading_primary_account_id_ = account_id;

  base::HistogramTester histogram_tester;
  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountInPref.StartupState.Primary",
      AccountStartupState::kUnknownInvalidToken, 1);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadAllCredentialsIntoMemoryAccountAvailabilitySecondaryAvailable) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, TokenWithBindingKey> tokens;
  const std::string gaia_id = "gaia_id";
  const CoreAccountId account_id = CoreAccountId::FromGaiaId(gaia_id);
  tokens["AccountId-gaia_id"] =
      TokenWithBindingKey(GaiaConstants::kInvalidRefreshToken);

  // Secondary account is available in account tracker service.
  account_tracker_service_.SeedAccountInfo(gaia_id, "test@google.com");

  base::HistogramTester histogram_tester;
  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountInPref.StartupState.Secondary",
      AccountStartupState::kKnownInvalidToken, 1);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadAllCredentialsIntoMemoryAccountAvailabilitySecondaryNotAvailable) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  std::map<std::string, TokenWithBindingKey> tokens;
  const std::string gaia_id = "gaia_id";
  const CoreAccountId account_id = CoreAccountId::FromGaiaId(gaia_id);
  tokens["AccountId-gaia_id"] = TokenWithBindingKey("refresh_token");

  // Secondary account is not seeded in the account tracker service.
  base::HistogramTester histogram_tester;
  oauth2_service_delegate_->LoadAllCredentialsIntoMemory(tokens);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountInPref.StartupState.Secondary",
      AccountStartupState::kUnknownValidToken, 1);
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
  AddSuccessfulOAuthTokenResponse();

  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this,
          kNoBindingChallenge);
  fetcher->Start("foo", "bar", scope_list);
  WaitForGetTokenCompleted();
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
  AddSuccessfulOAuthTokenResponse();

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1 =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this,
          kNoBindingChallenge);
  fetcher1->Start("foo", "bar", scope_list);
  WaitForGetTokenCompleted();
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
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this,
          kNoBindingChallenge);
  fetcher2->Start("foo", "bar", scope_list);
  WaitForGetTokenCompleted();
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
  AddSuccessfulOAuthTokenResponse();

  // Transient error will repeat until backoff period expires.
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::vector<std::string> scope_list;
  scope_list.push_back("scope");
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher1 =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this,
          kNoBindingChallenge);
  fetcher1->Start("foo", "bar", scope_list);
  WaitForGetTokenCompleted();
  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);

  // Notify of network change and ensure that request now runs.
  oauth2_service_delegate_->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher2 =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this,
          kNoBindingChallenge);
  fetcher2->Start("foo", "bar", scope_list);
  WaitForGetTokenCompleted();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(1, access_token_failure_count_);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       LoadPrimaryAccountOnlyWhenAccountConsistencyDisabled) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  CoreAccountId primary_account = CoreAccountId::FromGaiaId("primaryaccount");
  CoreAccountId secondary_account =
      CoreAccountId::FromGaiaId("secondaryaccount");

  // Verify DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.ToString(),
                       "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account.ToString(),
                       "refresh_token");
  oauth2_service_delegate_->LoadCredentials(primary_account,
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();

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

    void OnAuthErrorChanged(
        const CoreAccountId& account_id,
        const GoogleServiceAuthError& auth_error,
        signin_metrics::SourceForRefreshTokenOperation source) override {
      error_changed_ = true;
      EXPECT_EQ("account_id", account_id.ToString());
      EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), auth_error);
      EXPECT_TRUE(delegate_->RefreshTokenIsAvailable(account_id));
      EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
                delegate_->GetAuthError(account_id));
      EXPECT_EQ(signin_metrics::SourceForRefreshTokenOperation::
                    kAccountReconcilor_GaiaCookiesUpdated,
                source);
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
      CoreAccountId::FromGaiaId("account_id"), "token",
      signin_metrics::SourceForRefreshTokenOperation::
          kAccountReconcilor_GaiaCookiesUpdated);
  EXPECT_TRUE(token_service_observer.error_changed_);

  oauth2_service_delegate_->RemoveObserver(&token_service_observer);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest,
       OnAuthErrorChangedAfterUpdatingCredentials) {
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  CoreAccountId account_id = CoreAccountId::FromGaiaId("gaia_id");
  testing::StrictMock<signin::MockProfileOAuth2TokenServiceObserver> observer(
      oauth2_service_delegate_.get());

  {
    testing::InSequence sequence;
    // `OnAuthErrorChanged()` is called *before* `OnRefreshTokenAvailable()`
    // after adding a new account on Desktop.
    EXPECT_CALL(
        observer,
        OnAuthErrorChanged(account_id, GoogleServiceAuthError::AuthErrorNone(),
                           testing::_));
    EXPECT_CALL(observer, OnRefreshTokenAvailable(account_id));
    EXPECT_CALL(observer, OnEndBatchChanges());
    oauth2_service_delegate_->UpdateCredentials(account_id, "first_token");
    testing::Mock::VerifyAndClearExpectations(&observer);
  }

  {
    testing::InSequence sequence;
    // `OnAuthErrorChanged()` is not called when a token is updated without
    // changing its error state.
    EXPECT_CALL(observer, OnAuthErrorChanged).Times(0);
    EXPECT_CALL(observer, OnRefreshTokenAvailable(account_id));
    EXPECT_CALL(observer, OnEndBatchChanges());

    oauth2_service_delegate_->UpdateCredentials(account_id, "second_token");
    testing::Mock::VerifyAndClearExpectations(&observer);
  }
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

    void OnAuthErrorChanged(
        const CoreAccountId& account_id,
        const GoogleServiceAuthError& auth_error,
        signin_metrics::SourceForRefreshTokenOperation source) override {
      error_changed_ = true;
      EXPECT_FALSE(token_available_)
          << "OnAuthErrorChanged() should be called first";
      EXPECT_EQ(auth_error, delegate_->GetAuthError(account_id));
      CheckTokenState(account_id);
      EXPECT_EQ(signin_metrics::SourceForRefreshTokenOperation::
                    kDiceResponseHandler_Signout,
                source);
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
      GaiaConstants::kInvalidRefreshToken,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signout);
  EXPECT_TRUE(token_service_observer.token_available_);
  EXPECT_TRUE(token_service_observer.error_changed_);
  oauth2_service_delegate_->RemoveObserver(&token_service_observer);
}

// Checks that set_revoke_all_tokens_on_first_load() revokes the tokens,
// updates the database, and is applied only once.
TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, ClearTokensOnStartup) {
  client_->SetNetworkCallsDelayed(true);
  revoke_all_tokens_on_load_ = RevokeAllTokensOnLoad::kDeleteSiteDataOnExit;
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDisabled);
  CoreAccountId primary_account = CoreAccountId::FromGaiaId("primaryaccount");
  CoreAccountId secondary_account =
      CoreAccountId::FromGaiaId("secondaryaccount");

  // Verify DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.ToString(),
                       "refresh_token");
  AddAuthTokenManually("AccountId-" + secondary_account.ToString(),
                       "refresh_token");
  // With explicit signin, tokens are only cleared at startup for syncing users.
  oauth2_service_delegate_->LoadCredentials(primary_account,
                                            /*is_syncing=*/true);
  WaitForRefreshTokensLoaded();

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
                                            /*is_syncing=*/true);
  WaitForRefreshTokensLoaded();
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

TEST_F(MutableProfileOAuth2TokenServiceDelegateTest, TokenReencryption) {
  const CoreAccountId primary_account =
      CoreAccountId::FromGaiaId("primaryaccount");

  // Initial clean-up, since SetUp initializes a database. Make sure that's
  // closed so this test has full control over feature configuration.
  UnloadTokenDatabase();

  // This closure sets up the environment for the test, and also creates a
  // cleanup at the end of the scope of the test.
  auto SetUpTestAndReturnScopedCleanup =
      [this](bool new_encryption_enabled, bool expect_reencrypt,
             std::string_view key_prefix,
             base::HistogramBase::Count expected_writes)
      -> base::ScopedClosureRunner {
    auto histograms = std::make_unique<base::HistogramTester>();
    auto features = std::make_unique<base::test::ScopedFeatureList>();
    features->InitWithFeatureState(features::kUseNewEncryptionKeyForWebData,
                                   new_encryption_enabled);

    LoadTokenDatabase();
    InitializeOAuth2ServiceDelegate(
        signin::AccountConsistencyMethod::kDisabled);

    return base::ScopedClosureRunner(base::BindLambdaForTesting(
        [this, key_prefix, expect_reencrypt, expected_writes,
         histograms = std::move(histograms), feature = std::move(features)]() {
          UnloadTokenDatabase();
          {
            // The APIs available via WebData always return plaintext data so
            // the only way to verify the ciphertext is to inspect the database
            // manually. This is safe to do here because the database has been
            // unloaded above.
            sql::Database db;
            ASSERT_TRUE(
                db.Open(temp_dir_.GetPath().AppendASCII(kTestTokenDatabase)));
            sql::Statement s(db.GetUniqueStatement(
                "SELECT encrypted_token FROM token_service"));
            ASSERT_TRUE(s.Step());
            std::string encrypted_data;
            ASSERT_TRUE(s.ColumnBlobAsString(0, &encrypted_data));
            EXPECT_TRUE(base::StartsWith(encrypted_data, key_prefix,
                                         base::CompareCase::SENSITIVE));
            // Should only be one row, the "invalid-token" should be deleted by
            // the time the database is unloaded, and never re-encrypted.
            ASSERT_FALSE(s.Step());
          }
          test_service_observation_.Reset();
          oauth2_service_delegate_->Shutdown();
          histograms->ExpectUniqueSample("Signin.ReencryptTokensInDb",
                                         expect_reencrypt, 1u);
          histograms->ExpectUniqueSample("Signin.TokenTable.SetTokenResult",
                                         /*kSuccess*/ 0, expected_writes);
        }));
  };

  {
    // Expect two writes. They are both from the calls to `AddAuthTokenManually`
    // to set up the database.
    auto cleanup = SetUpTestAndReturnScopedCleanup(
        /*new_encryption_enabled=*/false, /*expect_reencrypt=*/false,
        os_crypt_async::kOsCryptSyncCompatibleTestKeyPrefix,
        /*expected_writes=*/2);

    // Verify DB is clean.
    ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
    AddAuthTokenManually("AccountId-" + primary_account.ToString(),
                         "refresh_token");
    // Add an invalid token. This will be removed during
    // LoadAllCredentialsIntoMemory.
    AddAuthTokenManually("invalid-token", "foo");

    ResetObserverCounts();
    oauth2_service_delegate_->LoadCredentials(primary_account,
                                              /*is_syncing=*/false);
    WaitForRefreshTokensLoaded();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_TRUE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  }
  // In the second part of the test, load the same database, but with new
  // encryption enabled. This should migrate the data to the new key.
  {
    // Expect two writes. First one is from `AddAuthTokenManually` for the
    // invalid token, and the second is from the re-encryption of the valid
    // token. Two writes only indicates that the invalid token was not
    // re-encrypted to the database, as expected.
    auto cleanup = SetUpTestAndReturnScopedCleanup(
        /*new_encryption_enabled=*/true, /*expect_reencrypt=*/true,
        os_crypt_async::kDefaultTestKeyPrefix, /*expected_writes=*/2);

    // Add another invalid token. This will be not re-encrypted, and removed
    // during LoadAllCredentialsIntoMemory.
    AddAuthTokenManually("invalid-token", "foo");

    ResetObserverCounts();
    oauth2_service_delegate_->LoadCredentials(primary_account,
                                              /*is_syncing=*/false);
    WaitForRefreshTokensLoaded();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_TRUE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  }
  // In the third part of the test, load the same database again, but with the
  // new encryption still enabled. Since the data has already been migrated to
  // the new key, it won't be migrated again.
  {
    // Expect no writes. The tokens have already been migrated to the new key so
    // no writes are needed.
    auto cleanup = SetUpTestAndReturnScopedCleanup(
        /*new_encryption_enabled=*/true, /*expect_reencrypt=*/false,
        os_crypt_async::kDefaultTestKeyPrefix, /*expected_writes=*/0);

    ResetObserverCounts();
    oauth2_service_delegate_->LoadCredentials(primary_account,
                                              /*is_syncing=*/false);
    WaitForRefreshTokensLoaded();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_TRUE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  }
  // Verify also that if the feature state is rolled back, then the encryption
  // goes back to as it was before.
  {
    // One write is expected here. The single token is migrated back to the old
    // key.
    auto cleanup = SetUpTestAndReturnScopedCleanup(
        /*new_encryption_enabled=*/false, /*expect_reencrypt=*/true,
        os_crypt_async::kOsCryptSyncCompatibleTestKeyPrefix,
        /*expected_writes=*/1);

    ResetObserverCounts();
    oauth2_service_delegate_->LoadCredentials(primary_account,
                                              /*is_syncing=*/false);
    WaitForRefreshTokensLoaded();

    EXPECT_EQ(1, tokens_loaded_count_);
    EXPECT_EQ(1, token_available_count_);
    EXPECT_TRUE(
        oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  }
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
class MutableProfileOAuth2TokenServiceDelegateBoundTokensTest
    : public MutableProfileOAuth2TokenServiceDelegateTest {
 public:
  void InitializeOAuth2ServiceDelegateWithTokenBinding() {
    oauth2_service_delegate_ = CreateOAuth2ServiceDelegate(
        signin::AccountConsistencyMethod::kDice,
        std::make_unique<TokenBindingHelper>(fake_unexportable_key_service_));
    oauth2_service_delegate_->SetOnRefreshTokenRevokedNotified(
        base::DoNothing());
    test_service_observation_.Observe(oauth2_service_delegate_.get());
  }

  void ShutdownOAuth2ServiceDelegate() {
    test_service_observation_.Reset();
    oauth2_service_delegate_->Shutdown();
  }

 private:
  unexportable_keys::FakeUnexportableKeyService fake_unexportable_key_service_;
};

TEST_F(MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
       UpdateBoundToken) {
  InitializeOAuth2ServiceDelegateWithTokenBinding();
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  EXPECT_TRUE(
      oauth2_service_delegate_->GetWrappedBindingKey(account_id).empty());

  // Set bound refresh token.
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  oauth2_service_delegate_->UpdateCredentials(
      account_id, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  EXPECT_EQ(oauth2_service_delegate_->GetWrappedBindingKey(account_id),
            kFakeWrappedBindingKey);

  // Update bound refresh token.
  const std::vector<uint8_t> kFakeWrappedBindingKey2 = {4, 5, 6};
  oauth2_service_delegate_->UpdateCredentials(
      account_id, "refresh_token2",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey2);
  EXPECT_EQ(oauth2_service_delegate_->GetWrappedBindingKey(account_id),
            kFakeWrappedBindingKey2);

  // Invalidate bound refresh token.
  oauth2_service_delegate_->UpdateCredentials(
      account_id, GaiaConstants::kInvalidRefreshToken);
  EXPECT_TRUE(
      oauth2_service_delegate_->GetWrappedBindingKey(account_id).empty());
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
       RevokeBoundToken) {
  InitializeOAuth2ServiceDelegateWithTokenBinding();
  const CoreAccountId account_id = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId account_id2 = CoreAccountId::FromGaiaId("account_id2");
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  const std::vector<uint8_t> kFakeWrappedBindingKey2 = {4, 5, 6};
  oauth2_service_delegate_->UpdateCredentials(
      account_id, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  oauth2_service_delegate_->UpdateCredentials(
      account_id2, "refresh_token2",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey2);

  oauth2_service_delegate_->RevokeCredentials(account_id);
  EXPECT_TRUE(
      oauth2_service_delegate_->GetWrappedBindingKey(account_id).empty());
  EXPECT_EQ(oauth2_service_delegate_->GetWrappedBindingKey(account_id2),
            kFakeWrappedBindingKey2);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
       PersistenceLoadOneBoundToken) {
  InitializeOAuth2ServiceDelegateWithTokenBinding();
  const CoreAccountId kAccountId = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId kAccountId2 = CoreAccountId::FromGaiaId("account_id_2");
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  // Ensure DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());

  oauth2_service_delegate_->UpdateCredentials(
      kAccountId, "bound_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  oauth2_service_delegate_->UpdateCredentials(kAccountId2, "non_bound_token");

  // Re-initialize the delegate and re-load tokens from disk.
  ShutdownOAuth2ServiceDelegate();
  InitializeOAuth2ServiceDelegateWithTokenBinding();
  base::HistogramTester histogram_tester;
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();

  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(kAccountId));
  EXPECT_FALSE(
      oauth2_service_delegate_->GetWrappedBindingKey(kAccountId).empty());
  EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(kAccountId2));
  EXPECT_TRUE(
      oauth2_service_delegate_->GetWrappedBindingKey(kAccountId2).empty());

  histogram_tester.ExpectUniqueSample(
      "Signin.TokenBinding.BoundTokenPrevalence",
      /*kSomeTokensBoundSomeUnbound*/ 2, /*expected_bucket_count=*/1);
  // The following histogram is not recorded because there is only one bound
  // token.
  histogram_tester.ExpectTotalCount("Signin.TokenBinding.BoundToTheSameKey", 0);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
       PersistenceLoadMultipleBoundTokens) {
  InitializeOAuth2ServiceDelegateWithTokenBinding();
  const CoreAccountId kAccountId = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId kAccountId2 = CoreAccountId::FromGaiaId("account_id_2");
  const CoreAccountId kAccountId3 = CoreAccountId::FromGaiaId("account_id_3");
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  // Ensure DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());

  oauth2_service_delegate_->UpdateCredentials(
      kAccountId, "bound_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  oauth2_service_delegate_->UpdateCredentials(
      kAccountId2, "bound_token_2",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  oauth2_service_delegate_->UpdateCredentials(
      kAccountId3, "bound_token_3",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);

  // Re-initialize the delegate and re-load tokens from disk.
  ShutdownOAuth2ServiceDelegate();
  InitializeOAuth2ServiceDelegateWithTokenBinding();
  base::HistogramTester histogram_tester;
  oauth2_service_delegate_->LoadCredentials(CoreAccountId(),
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();

  for (const CoreAccountId& account_id :
       {kAccountId, kAccountId2, kAccountId3}) {
    EXPECT_TRUE(oauth2_service_delegate_->RefreshTokenIsAvailable(account_id));
    EXPECT_EQ(oauth2_service_delegate_->GetWrappedBindingKey(account_id),
              kFakeWrappedBindingKey);
  }

  histogram_tester.ExpectUniqueSample(
      "Signin.TokenBinding.BoundTokenPrevalence",
      /*kAllTokensBound*/ 3, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Signin.TokenBinding.BoundToTheSameKey",
                                      true, /*expected_bucket_count=*/1);
}

TEST_F(MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
       ClearBoundTokenOnStartup) {
  client_->SetNetworkCallsDelayed(true);
  revoke_all_tokens_on_load_ = RevokeAllTokensOnLoad::kExplicitRevoke;
  InitializeOAuth2ServiceDelegateWithTokenBinding();
  const CoreAccountId kPrimaryAccount =
      CoreAccountId::FromGaiaId("primaryaccount");
  const CoreAccountId kSecondaryAccount =
      CoreAccountId::FromGaiaId("secondaryaccount");
  const std::vector<uint8_t> kFakePrimaryWrappedBindingKey = {1, 2, 3};
  const std::vector<uint8_t> kFakeSecondaryWrappedBindingKey = {4, 5, 6};

  // Verify DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  AddAuthTokenManually("AccountId-" + kPrimaryAccount.ToString(),
                       "refresh_token", kFakePrimaryWrappedBindingKey);
  AddAuthTokenManually("AccountId-" + kSecondaryAccount.ToString(),
                       "refresh_token", kFakeSecondaryWrappedBindingKey);
  oauth2_service_delegate_->LoadCredentials(kPrimaryAccount,
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();

  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(kPrimaryAccount));
  EXPECT_EQ(oauth2_service_delegate_->GetRefreshTokenForTest(kPrimaryAccount),
            GaiaConstants::kInvalidRefreshToken);
  EXPECT_TRUE(
      oauth2_service_delegate_->GetWrappedBindingKey(kPrimaryAccount).empty());
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(kSecondaryAccount));

  // Tokens are revoked on the server.
  EXPECT_EQ(2u, oauth2_service_delegate_->server_revokes_.size());
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Check that the changes have been persisted in the database: tokens are not
  // revoked again on the server.
  client_->SetNetworkCallsDelayed(true);
  oauth2_service_delegate_->LoadCredentials(kPrimaryAccount,
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(kPrimaryAccount));
  EXPECT_EQ(oauth2_service_delegate_->GetRefreshToken(kPrimaryAccount),
            GaiaConstants::kInvalidRefreshToken);
  EXPECT_TRUE(
      oauth2_service_delegate_->GetWrappedBindingKey(kPrimaryAccount).empty());
  EXPECT_FALSE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(kSecondaryAccount));
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

class MutableProfileOAuth2TokenServiceDelegateWithChallengeParamTest
    : public MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
      public testing::WithParamInterface<std::string> {};

TEST_P(MutableProfileOAuth2TokenServiceDelegateWithChallengeParamTest,
       FetchWithBoundToken) {
  ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
  InitializeOAuth2ServiceDelegateWithTokenBinding();

  const CoreAccountId account_id =
      account_tracker_service_.SeedAccountInfo("account_id", "test@google.com");
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};

  oauth2_service_delegate_->UpdateCredentials(
      account_id, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);

  AddSuccessfulBoundTokenResponse();

  EXPECT_EQ(0, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
  std::unique_ptr<OAuth2AccessTokenFetcher> fetcher =
      oauth2_service_delegate_->CreateAccessTokenFetcher(
          account_id, oauth2_service_delegate_->GetURLLoaderFactory(), this,
          GetParam());
  fetcher->Start("foo", "bar", {"scope"});
  WaitForGetTokenCompleted();
  EXPECT_EQ(1, access_token_success_count_);
  EXPECT_EQ(0, access_token_failure_count_);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MutableProfileOAuth2TokenServiceDelegateWithChallengeParamTest,
    testing::Values(kNoBindingChallenge, "test_challenge"),
    [](const auto& info) {
      return info.param.empty() ? "NoChallenge" : "HasChallenge";
    });
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

class MutableProfileOAuth2TokenServiceDelegateWithUnoDesktopTest
    : public MutableProfileOAuth2TokenServiceDelegateTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

// Checks that, for a signed in non-syncing account in UNO with clear on exit,
// set_revoke_all_tokens_on_first_load() keeps the tokens for the primary and
// secondary accounts, updates the database, and is applied only once.
TEST_F(MutableProfileOAuth2TokenServiceDelegateWithUnoDesktopTest,
       KeepPrimaryAccountTokenOnStartupWithClearOnExit) {
  client_->SetNetworkCallsDelayed(true);
  revoke_all_tokens_on_load_ = RevokeAllTokensOnLoad::kDeleteSiteDataOnExit;
  InitializeOAuth2ServiceDelegate(signin::AccountConsistencyMethod::kDice);
  CoreAccountId primary_account = CoreAccountId::FromGaiaId("primary_account");
  char refresh_token_primary[] = "refresh_token_primary";
  CoreAccountId secondary_account =
      CoreAccountId::FromGaiaId("secondary_account");
  char refresh_token_secondary[] = "refresh_token_secondary";

  // Verify DB is clean.
  ASSERT_TRUE(oauth2_service_delegate_->GetAccounts().empty());
  ResetObserverCounts();
  AddAuthTokenManually("AccountId-" + primary_account.ToString(),
                       refresh_token_primary);
  AddAuthTokenManually("AccountId-" + secondary_account.ToString(),
                       refresh_token_secondary);
  oauth2_service_delegate_->LoadCredentials(primary_account,
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();

  EXPECT_EQ(1, tokens_loaded_count_);
  EXPECT_EQ(2, token_available_count_);
  EXPECT_EQ(0, token_revoked_count_);
  EXPECT_EQ(1, end_batch_changes_);
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
  EXPECT_STREQ(
      refresh_token_primary,
      oauth2_service_delegate_->GetRefreshToken(primary_account).c_str());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            oauth2_service_delegate_->GetAuthError(primary_account));

  // No token is revoked on the server.
  EXPECT_EQ(0u, oauth2_service_delegate_->server_revokes_.size());
  client_->SetNetworkCallsDelayed(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());

  // Check that the changes have been persisted in the database: tokens are not
  // revoked again on the server.
  client_->SetNetworkCallsDelayed(true);
  oauth2_service_delegate_->LoadCredentials(primary_account,
                                            /*is_syncing=*/false);
  WaitForRefreshTokensLoaded();
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(primary_account));
  EXPECT_TRUE(
      oauth2_service_delegate_->RefreshTokenIsAvailable(secondary_account));
  EXPECT_STREQ(
      refresh_token_primary,
      oauth2_service_delegate_->GetRefreshToken(primary_account).c_str());
  EXPECT_TRUE(oauth2_service_delegate_->server_revokes_.empty());
}

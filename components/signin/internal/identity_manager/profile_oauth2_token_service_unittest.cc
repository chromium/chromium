// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/mock_profile_oauth2_token_service_observer.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A testing consumer that retries on error.
class RetryingTestingOAuth2AccessTokenManagerConsumer
    : public TestingOAuth2AccessTokenManagerConsumer {
 public:
  RetryingTestingOAuth2AccessTokenManagerConsumer(
      ProfileOAuth2TokenService* oauth2_service,
      const CoreAccountId& account_id)
      : oauth2_service_(oauth2_service), account_id_(account_id) {}
  ~RetryingTestingOAuth2AccessTokenManagerConsumer() override = default;

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    if (retry_counter_ <= 0)
      return;
    retry_counter_--;
    TestingOAuth2AccessTokenManagerConsumer::OnGetTokenFailure(request, error);
    request_ = oauth2_service_->StartRequest(
        account_id_, OAuth2AccessTokenManager::ScopeSet(), this);
  }

  int retry_counter_ = 2;
  raw_ptr<ProfileOAuth2TokenService> oauth2_service_;
  CoreAccountId account_id_;
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_;
};

class MockOAuth2AccessTokenConsumer
    : public TestingOAuth2AccessTokenManagerConsumer {
 public:
  MockOAuth2AccessTokenConsumer() = default;

  MockOAuth2AccessTokenConsumer(const MockOAuth2AccessTokenConsumer&) = delete;
  MockOAuth2AccessTokenConsumer& operator=(
      const MockOAuth2AccessTokenConsumer&) = delete;

  ~MockOAuth2AccessTokenConsumer() override = default;

  MOCK_METHOD2(
      OnGetTokenSuccess,
      void(const OAuth2AccessTokenManager::Request* request,
           const OAuth2AccessTokenConsumer::TokenResponse& token_response));

  MOCK_METHOD2(OnGetTokenFailure,
               void(const OAuth2AccessTokenManager::Request* request,
                    const GoogleServiceAuthError& error));
};

// This class fakes the behaviour of a MutableProfileOAuth2TokenServiceDelegate
// used on Desktop.
class FakeProfileOAuth2TokenServiceDelegateDesktop
    : public FakeProfileOAuth2TokenServiceDelegate {
  std::string GetTokenForMultilogin(
      const CoreAccountId& account_id) const override {
    if (GetAuthError(account_id) == GoogleServiceAuthError::AuthErrorNone())
      return GetRefreshToken(account_id);
    return std::string();
  }

  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override {
    if (GetAuthError(account_id).IsPersistentError()) {
      return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
          consumer, GetAuthError(account_id));
    }
    return FakeProfileOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
        account_id, url_loader_factory, consumer, token_binding_challenge);
  }
  void InvalidateTokenForMultilogin(
      const CoreAccountId& failed_account) override {
    UpdateAuthError(failed_account,
                    GoogleServiceAuthError(
                        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }
};

}  // namespace

class ProfileOAuth2TokenServiceTest : public testing::Test {
 public:
  void SetUp() override {
    auto delegate = std::make_unique<FakeProfileOAuth2TokenServiceDelegate>();
    // Save raw delegate pointer for later.
    delegate_ptr_ = delegate.get();

    oauth2_service_ = std::make_unique<ProfileOAuth2TokenService>(
        &prefs_, std::move(delegate));
    account_id_ = CoreAccountId::FromGaiaId("test_user");
  }

  void SimulateOAuthTokenResponse(const std::string& token,
                                  net::HttpStatusCode status = net::HTTP_OK) {
    test_url_loader_factory()->AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(), token, status);
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return delegate_ptr_->test_url_loader_factory();
  }

  void ResetTokenService() {
    delegate_ptr_ = nullptr;
    oauth2_service_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  TestingPrefServiceSimple prefs_;
  std::unique_ptr<ProfileOAuth2TokenService> oauth2_service_;
  raw_ptr<FakeProfileOAuth2TokenServiceDelegate> delegate_ptr_ = nullptr;
  CoreAccountId account_id_;
  TestingOAuth2AccessTokenManagerConsumer consumer_;
};

TEST_F(ProfileOAuth2TokenServiceTest, NoOAuth2RefreshToken) {
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest, GetAccounts) {
  // Accounts should start off empty.
  auto accounts = oauth2_service_->GetAccounts();
  EXPECT_TRUE(accounts.empty());

  // Add an account.
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  // Accounts should still be empty as tokens have not yet been loaded from
  // disk.
  accounts = oauth2_service_->GetAccounts();
  EXPECT_TRUE(accounts.empty());

  // Load tokens from disk.
  oauth2_service_->GetDelegate()->LoadCredentials(CoreAccountId(),
                                                  /*is_syncing=*/false);

  // |account_id_| should now be visible in the accounts.
  accounts = oauth2_service_->GetAccounts();
  EXPECT_THAT(accounts, testing::ElementsAre(account_id_));
}

TEST_F(ProfileOAuth2TokenServiceTest, FailureShouldNotRetry) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  SimulateOAuthTokenResponse("", net::HTTP_UNAUTHORIZED);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
  EXPECT_EQ(0, test_url_loader_factory()->NumPending());
}

TEST_F(ProfileOAuth2TokenServiceTest, SuccessWithoutCaching) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

TEST_F(ProfileOAuth2TokenServiceTest, SuccessWithCaching) {
  OAuth2AccessTokenManager::ScopeSet scopes1;
  scopes1.insert("s1");
  scopes1.insert("s2");
  OAuth2AccessTokenManager::ScopeSet scopes1_same;
  scopes1_same.insert("s2");
  scopes1_same.insert("s1");
  OAuth2AccessTokenManager::ScopeSet scopes2;
  scopes2.insert("s3");

  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  // First request.
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(account_id_, scopes1, &consumer_));
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request to the same set of scopes, should return the same token
  // without needing a network request.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequest(account_id_, scopes1_same, &consumer_));
  base::RunLoop().RunUntilIdle();

  // No new network fetcher.
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Third request to a new set of scopes, should return another token.
  SimulateOAuthTokenResponse(GetValidTokenResponse("token2", 3600));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      oauth2_service_->StartRequest(account_id_, scopes2, &consumer_));
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token2", consumer_.last_token_);
}

TEST_F(ProfileOAuth2TokenServiceTest, SuccessAndExpirationAndFailure) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  // First request.
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 0));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  SimulateOAuthTokenResponse("", net::HTTP_UNAUTHORIZED);  // Network failure.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest, SuccessAndExpirationAndSuccess) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  // First request.
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 0));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  SimulateOAuthTokenResponse(GetValidTokenResponse("another token", 0));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
}

TEST_F(ProfileOAuth2TokenServiceTest, RequestDeletedBeforeCompletion) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());

  request.reset();

  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest, RequestDeletedAfterCompletion) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  request.reset();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

TEST_F(ProfileOAuth2TokenServiceTest,
       MultipleRequestsForTheSameScopesWithOneDeleted) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();

  request.reset();

  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest,
       ClearedRefreshTokenFailsSubsequentRequests) {
  // We have a valid refresh token; the first request is successful.
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // The refresh token is no longer available; subsequent requests fail.
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_, "");
  request = oauth2_service_->StartRequest(
      account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest, NotificationOrderOnRefreshTokenAdded) {
  std::unique_ptr<
      testing::StrictMock<signin::MockProfileOAuth2TokenServiceObserver>>
      observers[5];
  for (auto& observer : observers) {
    observer = std::make_unique<
        testing::StrictMock<signin::MockProfileOAuth2TokenServiceObserver>>(
        oauth2_service_.get());
  }

  // `OnAuthErrorChanged()` is not called after adding a new account in tests.
  testing::InSequence sequence;
  // First, all observers will receive `OnRefreshTokenAvailable()` notification.
  for (auto& observer : observers) {
    EXPECT_CALL(*observer, OnRefreshTokenAvailable(account_id_));
  }
  // Then, `OnEndBatchChanges()` is called.
  for (auto& observer : observers) {
    EXPECT_CALL(*observer, OnEndBatchChanges());
  }

  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "first refreshToken");
}

TEST_F(ProfileOAuth2TokenServiceTest, NotificationOrderOnRefreshTokenRevoked) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "first refreshToken");
  // `OnAuthErrorChanged()` shouldn't be called if the refresh token is revoked.
  std::unique_ptr<
      testing::StrictMock<signin::MockProfileOAuth2TokenServiceObserver>>
      observers[5];
  for (auto& observer : observers) {
    observer = std::make_unique<
        testing::StrictMock<signin::MockProfileOAuth2TokenServiceObserver>>(
        oauth2_service_.get());
  }

  MockOAuth2AccessTokenConsumer consumer;
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(account_id_, {"s1", "s2"}, &consumer));
  testing::InSequence sequence;
  // First, all observers will receive `OnRefreshTokenAvailable()` notification.
  for (auto& observer : observers) {
    EXPECT_CALL(*observer, OnRefreshTokenRevoked(account_id_));
  }
  // Then, all ongoing requests get cancelled.
  EXPECT_CALL(
      consumer,
      OnGetTokenFailure(
          ::testing::_,
          GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP)))
      .Times(1);
  // Finally, `OnEndBatchChanges()` is called.
  for (auto& observer : observers) {
    EXPECT_CALL(*observer, OnEndBatchChanges());
  }

  oauth2_service_->RevokeCredentials(account_id_);
}

TEST_F(ProfileOAuth2TokenServiceTest,
       ChangedRefreshTokenCancelsInFlightRequests) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "first refreshToken");
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert("s1");
  scopes.insert("s2");
  OAuth2AccessTokenManager::ScopeSet scopes1;
  scopes.insert("s3");
  scopes.insert("s4");

  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, test_url_loader_factory()->NumPending());

  // Note |request| is still pending when the refresh token changes.
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "second refreshToken");

  // UpdateCredentials() triggers OnRefreshTokenAvailable() which causes
  // CancelRequestsForAccount(). |consumer_| should have an error and the
  // request pending should be 0 since it's canceled at
  // OnRefreshTokenAvailable().
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
  ASSERT_EQ(0, test_url_loader_factory()->NumPending());

  // Verify that an access token request started after the refresh token was
  // updated can complete successfully.
  TestingOAuth2AccessTokenManagerConsumer consumer2;
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequest(account_id_, scopes1, &consumer2));
  base::RunLoop().RunUntilIdle();

  network::URLLoaderCompletionStatus ok_status(net::OK);
  auto response_head = network::CreateURLResponseHead(net::HTTP_OK);
  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url(), ok_status,
      std::move(response_head), GetValidTokenResponse("second token", 3600),
      network::TestURLLoaderFactory::kMostRecentMatch));
  EXPECT_EQ(1, consumer2.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer2.number_of_errors_);
  EXPECT_EQ("second token", consumer2.last_token_);
}

TEST_F(ProfileOAuth2TokenServiceTest, StartRequestForMultiloginDesktop) {
  ProfileOAuth2TokenService token_service(
      &prefs_,
      std::make_unique<FakeProfileOAuth2TokenServiceDelegateDesktop>());

  token_service.GetDelegate()->UpdateCredentials(account_id_, "refreshToken");
  const CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  token_service.GetDelegate()->UpdateCredentials(account_id_2, "refreshToken");
  token_service.GetDelegate()->UpdateAuthError(
      account_id_2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  MockOAuth2AccessTokenConsumer consumer;

  EXPECT_CALL(consumer, OnGetTokenSuccess(::testing::_, ::testing::_)).Times(1);
  EXPECT_CALL(
      consumer,
      OnGetTokenFailure(
          ::testing::_,
          GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP)))
      .Times(1);
  EXPECT_CALL(
      consumer,
      OnGetTokenFailure(::testing::_,
                        GoogleServiceAuthError(
                            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)))
      .Times(1);

  std::unique_ptr<OAuth2AccessTokenManager::Request> request1(
      token_service.StartRequestForMultilogin(account_id_, &consumer));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      token_service.StartRequestForMultilogin(account_id_2, &consumer));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      token_service.StartRequestForMultilogin(
          CoreAccountId::FromGaiaId("unknown_account"), &consumer));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ProfileOAuth2TokenServiceTest, StartRequestForMultiloginMobile) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequestForMultilogin(account_id_, &consumer_));

  base::RunLoop().RunUntilIdle();
  network::URLLoaderCompletionStatus ok_status(net::OK);
  auto response_head = network::CreateURLResponseHead(net::HTTP_OK);
  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url(), ok_status,
      std::move(response_head), GetValidTokenResponse("second token", 3600),
      network::TestURLLoaderFactory::kMostRecentMatch));
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest, ServiceShutDownBeforeFetchComplete) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  // The destructor should cancel all in-flight fetchers.
  ResetTokenService();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest, RetryingConsumer) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  RetryingTestingOAuth2AccessTokenManagerConsumer consumer(
      oauth2_service_.get(), account_id_);
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer));
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer.number_of_errors_);

  SimulateOAuthTokenResponse("", net::HTTP_UNAUTHORIZED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceTest, InvalidateTokensForMultiloginDesktop) {
  auto delegate =
      std::make_unique<FakeProfileOAuth2TokenServiceDelegateDesktop>();
  ProfileOAuth2TokenService token_service(&prefs_, std::move(delegate));
  signin::MockProfileOAuth2TokenServiceObserver observer(&token_service);
  EXPECT_CALL(observer,
              OnAuthErrorChanged(
                  account_id_,
                  GoogleServiceAuthError(
                      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS),
                  signin_metrics::SourceForRefreshTokenOperation::kUnknown))
      .Times(1);

  token_service.GetDelegate()->UpdateCredentials(
      account_id_, "refreshToken",
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);
  const CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  token_service.GetDelegate()->UpdateCredentials(
      account_id_2, "refreshToken2",
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);
  token_service.InvalidateTokenForMultilogin(account_id_, "refreshToken");
  // Check that refresh tokens for failed accounts are set in error.
  EXPECT_EQ(token_service.GetDelegate()->GetAuthError(account_id_).state(),
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_EQ(token_service.GetDelegate()->GetAuthError(account_id_2).state(),
            GoogleServiceAuthError::NONE);
}

TEST_F(ProfileOAuth2TokenServiceTest, InvalidateTokensForMultiloginMobile) {
  signin::MockProfileOAuth2TokenServiceObserver observer(oauth2_service_.get());
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(account_id_,
                         GoogleServiceAuthError(
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS),
                         testing::_))
      .Times(0);

  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  const CoreAccountId account_id_2 = CoreAccountId::FromGaiaId("account_id_2");
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_2,
                                                    "refreshToken2");
  ;
  oauth2_service_->InvalidateTokenForMultilogin(account_id_, "refreshToken");
  // Check that refresh tokens are not affected.
  EXPECT_EQ(oauth2_service_->GetDelegate()->GetAuthError(account_id_).state(),
            GoogleServiceAuthError::NONE);
  EXPECT_EQ(oauth2_service_->GetDelegate()->GetAuthError(account_id_2).state(),
            GoogleServiceAuthError::NONE);
}

TEST_F(ProfileOAuth2TokenServiceTest, InvalidateToken) {
  OAuth2AccessTokenManager::ScopeSet scopes;
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");

  // First request.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request, should return the same token without needing a network
  // request.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequest(account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();

  // No new network fetcher.
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Clear previous response so the token request will be pending and we can
  // simulate a response after it started.
  test_url_loader_factory()->ClearResponses();

  // Invalidating the token should return a new token on the next request.
  oauth2_service_->InvalidateAccessToken(account_id_, scopes,
                                         consumer_.last_token_);
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      oauth2_service_->StartRequest(account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  SimulateOAuthTokenResponse(GetValidTokenResponse("token2", 3600));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token2", consumer_.last_token_);
}

TEST_F(ProfileOAuth2TokenServiceTest, SameScopesRequestedForDifferentClients) {
  std::string client_id_1("client1");
  std::string client_secret_1("secret1");
  std::string client_id_2("client2");
  std::string client_secret_2("secret2");
  std::set<std::string> scope_set;
  scope_set.insert("scope1");
  scope_set.insert("scope2");

  std::string refresh_token("refreshToken");
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_, refresh_token);

  std::unique_ptr<OAuth2AccessTokenManager::Request> request1(
      oauth2_service_->StartRequestForClient(
          account_id_, client_id_1, client_secret_1, scope_set, &consumer_));
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequestForClient(
          account_id_, client_id_2, client_secret_2, scope_set, &consumer_));
  // Start a request that should be duplicate of |request1|.
  std::unique_ptr<OAuth2AccessTokenManager::Request> request3(
      oauth2_service_->StartRequestForClient(
          account_id_, client_id_1, client_secret_1, scope_set, &consumer_));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2U,
            oauth2_service_->token_manager_->GetNumPendingRequestsForTesting(
                client_id_1, account_id_, scope_set));
  ASSERT_EQ(1U,
            oauth2_service_->token_manager_->GetNumPendingRequestsForTesting(
                client_id_2, account_id_, scope_set));
}

TEST_F(ProfileOAuth2TokenServiceTest, RequestParametersOrderTest) {
  OAuth2AccessTokenManager::ScopeSet set_0;
  OAuth2AccessTokenManager::ScopeSet set_1;
  set_1.insert("1");

  const CoreAccountId account_id0 = CoreAccountId::FromGaiaId("0");
  const CoreAccountId account_id1 = CoreAccountId::FromGaiaId("1");
  OAuth2AccessTokenManager::RequestParameters params[] = {
      OAuth2AccessTokenManager::RequestParameters("0", account_id0, set_0),
      OAuth2AccessTokenManager::RequestParameters("0", account_id0, set_1),
      OAuth2AccessTokenManager::RequestParameters("0", account_id1, set_0),
      OAuth2AccessTokenManager::RequestParameters("0", account_id1, set_1),
      OAuth2AccessTokenManager::RequestParameters("1", account_id0, set_0),
      OAuth2AccessTokenManager::RequestParameters("1", account_id0, set_1),
      OAuth2AccessTokenManager::RequestParameters("1", account_id1, set_0),
      OAuth2AccessTokenManager::RequestParameters("1", account_id1, set_1),
  };

  for (size_t i = 0; i < std::size(params); i++) {
    for (size_t j = 0; j < std::size(params); j++) {
      if (i == j) {
        EXPECT_FALSE(params[i] < params[j]) << " i=" << i << ", j=" << j;
        EXPECT_FALSE(params[j] < params[i]) << " i=" << i << ", j=" << j;
      } else if (i < j) {
        EXPECT_TRUE(params[i] < params[j]) << " i=" << i << ", j=" << j;
        EXPECT_FALSE(params[j] < params[i]) << " i=" << i << ", j=" << j;
      } else {
        EXPECT_TRUE(params[j] < params[i]) << " i=" << i << ", j=" << j;
        EXPECT_FALSE(params[i] < params[j]) << " i=" << i << ", j=" << j;
      }
    }
  }
}

TEST_F(ProfileOAuth2TokenServiceTest, FixAccountErrorIfPossible) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  auto callback = base::BindRepeating(
      [](ProfileOAuth2TokenService* service,
         const CoreAccountId& account_id) -> bool {
        service->GetDelegate()->UpdateCredentials(account_id,
                                                  "validRefreshToken");
        return true;
      },
      oauth2_service_.get(), account_id_);
  delegate_ptr_->set_fix_request_if_possible(std::move(callback));

  signin::MockProfileOAuth2TokenServiceObserver observer(oauth2_service_.get());
  EXPECT_CALL(observer,
              OnAuthErrorChanged(account_id_,
                                 GoogleServiceAuthError::FromServiceError(""),
                                 testing::_))
      .Times(1);
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(account_id_, GoogleServiceAuthError::AuthErrorNone(),
                         testing::_))
      .Times(1);
  SimulateOAuthTokenResponse("", net::HTTP_UNAUTHORIZED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

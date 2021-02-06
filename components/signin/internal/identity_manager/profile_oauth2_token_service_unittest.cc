// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"

#include <stddef.h>
#include <string>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_access_token_manager_test_util.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// A testing consumer that retries on error.
class RetryingTestingOAuth2AccessTokenManagerConsumer
    : public TestingOAuth2AccessTokenManagerConsumer {
 public:
  RetryingTestingOAuth2AccessTokenManagerConsumer(
      ProfileOAuth2TokenService* oauth2_service,
      const CoreAccountId& account_id)
      : oauth2_service_(oauth2_service), account_id_(account_id) {}
  ~RetryingTestingOAuth2AccessTokenManagerConsumer() override {}

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
  ProfileOAuth2TokenService* oauth2_service_;
  CoreAccountId account_id_;
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_;
};

class FakeOAuth2TokenServiceObserver
    : public ProfileOAuth2TokenServiceObserver {
 public:
  MOCK_METHOD2(OnAuthErrorChanged,
               void(const CoreAccountId&, const GoogleServiceAuthError&));
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
      OAuth2AccessTokenConsumer* consumer) override {
    if (GetAuthError(account_id).IsPersistentError()) {
      return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
          consumer, GetAuthError(account_id));
    }
    return FakeProfileOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
        account_id, url_loader_factory, consumer);
  }
  void InvalidateTokenForMultilogin(
      const CoreAccountId& failed_account) override {
    UpdateAuthError(failed_account,
                    GoogleServiceAuthError(
                        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }
};

class ProfileOAuth2TokenServiceTest : public testing::Test {
 public:
  void SetUp() override {
    auto delegate = std::make_unique<FakeProfileOAuth2TokenServiceDelegate>();
    // Save raw delegate pointer for later.
    delegate_ptr_ = delegate.get();

    test_url_loader_factory_ = delegate->test_url_loader_factory();
    oauth2_service_ = std::make_unique<ProfileOAuth2TokenService>(
        &prefs_, std::move(delegate));
    account_id_ = CoreAccountId("test_user@gmail.com");
  }

  void TearDown() override {
    // Makes sure that all the clean up tasks are run.
    base::RunLoop().RunUntilIdle();
  }

  void SimulateOAuthTokenResponse(const std::string& token,
                                  net::HttpStatusCode status = net::HTTP_OK) {
    test_url_loader_factory_->AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(), token, status);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::
          IO};  // net:: stuff needs IO
                // message loop.
  network::TestURLLoaderFactory* test_url_loader_factory_ = nullptr;
  FakeProfileOAuth2TokenServiceDelegate* delegate_ptr_ = nullptr;  // Not owned.
  std::unique_ptr<ProfileOAuth2TokenService> oauth2_service_;
  CoreAccountId account_id_;
  TestingOAuth2AccessTokenManagerConsumer consumer_;
  TestingPrefServiceSimple prefs_;
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
  oauth2_service_->GetDelegate()->LoadCredentials(CoreAccountId());

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
  EXPECT_EQ(0, test_url_loader_factory_->NumPending());
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
  EXPECT_EQ(1, test_url_loader_factory_->NumPending());

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
  ASSERT_EQ(1, test_url_loader_factory_->NumPending());

  // Note |request| is still pending when the refresh token changes.
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "second refreshToken");

  // UpdateCredentials() triggers OnRefreshTokenAvailable() which causes
  // CancelRequestsForAccount(). |consumer_| should have an error and the
  // request pending should be 0 since it's canceled at
  // OnRefreshTokenAvailable().
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
  ASSERT_EQ(0, test_url_loader_factory_->NumPending());

  // Verify that an access token request started after the refresh token was
  // updated can complete successfully.
  TestingOAuth2AccessTokenManagerConsumer consumer2;
  std::unique_ptr<OAuth2AccessTokenManager::Request> request2(
      oauth2_service_->StartRequest(account_id_, scopes1, &consumer2));
  base::RunLoop().RunUntilIdle();

  network::URLLoaderCompletionStatus ok_status(net::OK);
  auto response_head = network::CreateURLResponseHead(net::HTTP_OK);
  EXPECT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      GaiaUrls::GetInstance()->oauth2_token_url(), ok_status,
      std::move(response_head), GetValidTokenResponse("second token", 3600),
      network::TestURLLoaderFactory::kMostRecentMatch));
  EXPECT_EQ(1, consumer2.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer2.number_of_errors_);
  EXPECT_EQ("second token", consumer2.last_token_);
}

TEST_F(ProfileOAuth2TokenServiceTest, StartRequestForMultiloginDesktop) {
  class MockOAuth2AccessTokenConsumer
      : public TestingOAuth2AccessTokenManagerConsumer {
   public:
    MockOAuth2AccessTokenConsumer() = default;
    ~MockOAuth2AccessTokenConsumer() = default;

    MOCK_METHOD2(
        OnGetTokenSuccess,
        void(const OAuth2AccessTokenManager::Request* request,
             const OAuth2AccessTokenConsumer::TokenResponse& token_response));

    MOCK_METHOD2(OnGetTokenFailure,
                 void(const OAuth2AccessTokenManager::Request* request,
                      const GoogleServiceAuthError& error));

    DISALLOW_COPY_AND_ASSIGN(MockOAuth2AccessTokenConsumer);
  };
  ProfileOAuth2TokenService token_service(
      &prefs_,
      std::make_unique<FakeProfileOAuth2TokenServiceDelegateDesktop>());

  token_service.GetDelegate()->UpdateCredentials(account_id_, "refreshToken");
  const CoreAccountId account_id_2("account_id_2");
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
      token_service.StartRequestForMultilogin(CoreAccountId("unknown_account"),
                                              &consumer));
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
  EXPECT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
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
  oauth2_service_.reset(nullptr);

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
  FakeOAuth2TokenServiceObserver observer;
  token_service.GetDelegate()->AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(account_id_,
                         GoogleServiceAuthError(
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)))
      .Times(1);

  token_service.GetDelegate()->UpdateCredentials(account_id_, "refreshToken");
  const CoreAccountId account_id_2("account_id_2");
  token_service.GetDelegate()->UpdateCredentials(account_id_2, "refreshToken2");
  token_service.InvalidateTokenForMultilogin(account_id_, "refreshToken");
  // Check that refresh tokens for failed accounts are set in error.
  EXPECT_EQ(token_service.GetDelegate()->GetAuthError(account_id_).state(),
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_EQ(token_service.GetDelegate()->GetAuthError(account_id_2).state(),
            GoogleServiceAuthError::NONE);

  token_service.GetDelegate()->RemoveObserver(&observer);
}

TEST_F(ProfileOAuth2TokenServiceTest, InvalidateTokensForMultiloginMobile) {
  FakeOAuth2TokenServiceObserver observer;
  oauth2_service_->GetDelegate()->AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnAuthErrorChanged(account_id_,
                         GoogleServiceAuthError(
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)))
      .Times(0);

  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  const CoreAccountId account_id_2("account_id_2");
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_2,
                                                    "refreshToken2");
  ;
  oauth2_service_->InvalidateTokenForMultilogin(account_id_, "refreshToken");
  // Check that refresh tokens are not affected.
  EXPECT_EQ(oauth2_service_->GetDelegate()->GetAuthError(account_id_).state(),
            GoogleServiceAuthError::NONE);
  EXPECT_EQ(oauth2_service_->GetDelegate()->GetAuthError(account_id_2).state(),
            GoogleServiceAuthError::NONE);

  oauth2_service_->GetDelegate()->RemoveObserver(&observer);
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
  test_url_loader_factory_->ClearResponses();

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

  const CoreAccountId account_id0("0");
  const CoreAccountId account_id1("1");
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

  for (size_t i = 0; i < base::size(params); i++) {
    for (size_t j = 0; j < base::size(params); j++) {
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

TEST_F(ProfileOAuth2TokenServiceTest, FixRequestErrorIfPossible) {
  oauth2_service_->GetDelegate()->UpdateCredentials(account_id_,
                                                    "refreshToken");
  std::unique_ptr<OAuth2AccessTokenManager::Request> request(
      oauth2_service_->StartRequest(
          account_id_, OAuth2AccessTokenManager::ScopeSet(), &consumer_));
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  delegate_ptr_->set_fix_request_if_possible(true);
  SimulateOAuthTokenResponse("", net::HTTP_UNAUTHORIZED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  SimulateOAuthTokenResponse(GetValidTokenResponse("token", 3600));
  base::RunLoop().RunUntilIdle();
  for (int max_reties = 5;
       max_reties >= 0 && consumer_.number_of_successful_tokens_ != 1;
       --max_reties) {
    base::RunLoop().RunUntilIdle();
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  }

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

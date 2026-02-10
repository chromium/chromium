// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/access_token_restriction.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MockCallback;
using sync_preferences::TestingPrefServiceSyncable;
using testing::_;
using testing::StrictMock;

namespace signin {

namespace {

using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;

constexpr GaiaId::Literal kTestGaiaId("dummyId");
constexpr GaiaId::Literal kTestGaiaId2("dummyId2");
constexpr char kTestEmail[] = "me@gmail.com";
constexpr char kTestEmail2[] = "me2@gmail.com";

// Used just to check that the id_token is passed along.
constexpr char kIdTokenEmptyServices[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".dummy-signature";

}  // namespace

class AccessTokenFetcherTest
    : public testing::Test,
      public OAuth2AccessTokenManager::DiagnosticsObserver {
 public:
  using TestTokenCallback =
      StrictMock<MockCallback<AccessTokenFetcher::TokenCallback>>;

  AccessTokenFetcherTest()
      : signin_client_(&pref_service_),
        token_service_(&pref_service_),
        access_token_info_("access token",
                           base::Time::Now() + base::Hours(1),
                           std::string(kIdTokenEmptyServices)),
        account_tracker_(CreateAccountTrackerService()) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
    PrimaryAccountManager::RegisterProfilePrefs(pref_service_.registry());
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());

    account_tracker_->Initialize(&pref_service_, base::FilePath());
    primary_account_manager_ = std::make_unique<PrimaryAccountManager>(
        &signin_client_, &token_service_, account_tracker_.get());
    token_service_.AddAccessTokenDiagnosticsObserver(this);
  }

  ~AccessTokenFetcherTest() override {
    token_service_.RemoveAccessTokenDiagnosticsObserver(this);
  }

  CoreAccountId SetPrimaryAccount(const GaiaId& gaia_id,
                                  const std::string& email) {
    CoreAccountInfo account_info = AddAccount(gaia_id, email);
    primary_account_manager_->SetPrimaryAccountInfo(
        account_info, ConsentLevel::kSignin,
        signin_metrics::AccessPoint::kStartPage);

    return account_info.account_id;
  }

  CoreAccountInfo AddAccount(const GaiaId& gaia_id, const std::string& email) {
    account_tracker()->SeedAccountInfo(gaia_id, email);
    return account_tracker()->FindAccountInfoByGaiaId(gaia_id);
  }

  // Verifies that the consumer_id has the appropriate consent level for the
  // scopes it requests to access.
  void VerifyScopeAccess(CoreAccountId account_id,
                         OAuthConsumerId consumer_id) {
    VerifyScopeAccess(account_id, consumer_id,
                      GetOAuthConsumerFromId(consumer_id));
  }

  // Verifies that the consumer_id has the appropriate consent level for the
  // scopes it requests to access.
  void VerifyScopeAccess(CoreAccountId account_id,
                         OAuthConsumerId consumer_id,
                         const OAuthConsumer& consumer) {
    TestTokenCallback callback;

    base::RunLoop run_loop;
    set_on_access_token_request_callback(run_loop.QuitClosure());

    token_service()->UpdateCredentials(account_id, "refresh token");

    // Since the refresh token is already available, this should result in an
    // immediate request for an access token.
    auto fetcher = CreateFetcher(
        account_id, consumer_id, consumer, callback.Get(),
        AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);

    run_loop.Run();

    // Once the access token request is fulfilled, we should get called back
    // with the access token.
    EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                              access_token_info()));

    token_service()->IssueAllTokensForAccount(
        account_id, TokenResponseBuilder()
                        .WithAccessToken(access_token_info().token)
                        .WithExpirationTime(access_token_info().expiration_time)
                        .WithIdToken(access_token_info().id_token)
                        .build());
  }

  std::unique_ptr<AccessTokenFetcher> CreateFetcher(
      const CoreAccountId& account_id,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode) {
    auto consumer_id = OAuthConsumerId::kSync;
    return CreateFetcher(account_id, consumer_id,
                         signin_client_.GetOAuthConsumerFromId(consumer_id),
                         std::move(callback), mode);
  }

  std::unique_ptr<AccessTokenFetcher> CreateFetcher(
      const CoreAccountId& account_id,
      OAuthConsumerId consumer_id,
      const OAuthConsumer& consumer,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode) {
    return std::make_unique<AccessTokenFetcher>(
        account_id, consumer_id, consumer, &token_service_,
        primary_account_manager_.get(), std::move(callback), mode);
  }

  std::unique_ptr<AccessTokenFetcher> CreateFetcherWithURLLoaderFactory(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode) {
    auto consumer_id = OAuthConsumerId::kSync;
    return std::make_unique<AccessTokenFetcher>(
        account_id, consumer_id,
        signin_client_.GetOAuthConsumerFromId(consumer_id), &token_service_,
        primary_account_manager_.get(), url_loader_factory, std::move(callback),
        mode);
  }

  AccountTrackerService* account_tracker() { return account_tracker_.get(); }

  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

  void set_on_access_token_request_callback(base::OnceClosure callback) {
    on_access_token_request_callback_ = std::move(callback);
  }

  // Returns an AccessTokenInfo with valid information that can be used for
  // completing access token requests.
  AccessTokenInfo access_token_info() { return access_token_info_; }

  const PrimaryAccountManager& primary_account_manager() {
    return *primary_account_manager_;
  }

  OAuthConsumer GetOAuthConsumerFromId(OAuthConsumerId consumer_id) {
    return signin_client_.GetOAuthConsumerFromId(consumer_id);
  }

 private:
  std::unique_ptr<AccountTrackerService> CreateAccountTrackerService() {
#if BUILDFLAG(IS_ANDROID)
    SetUpFakeAccountManagerFacade();
#endif
    return std::make_unique<AccountTrackerService>();
  }

  // OAuth2AccessTokenManager::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const CoreAccountId& account_id,
      const std::string& consumer_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override {
    if (on_access_token_request_callback_) {
      std::move(on_access_token_request_callback_).Run();
    }
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  AccessTokenInfo access_token_info_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
  std::unique_ptr<PrimaryAccountManager> primary_account_manager_;
  base::OnceClosure on_access_token_request_callback_;
};

TEST_F(AccessTokenFetcherTest, EmptyAccountFailsButDoesNotCrash) {
  TestTokenCallback callback;

  base::RunLoop run_loop;

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(CoreAccountId(), callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  // Fetching access tokens for an empty account id should respond with
  // ACCOUNT_NOT_FOUND error.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError(
                      GoogleServiceAuthError::State::ACCOUNT_NOT_FOUND),
                  AccessTokenInfo()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  run_loop.Run();
}

TEST_F(AccessTokenFetcherTest, OneShotShouldCallBackOnFulfilledRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));

  token_service()->IssueAllTokensForAccount(
      account_id, TokenResponseBuilder()
                      .WithAccessToken(access_token_info().token)
                      .WithExpirationTime(access_token_info().expiration_time)
                      .WithIdToken(access_token_info().id_token)
                      .build());
}

TEST_F(AccessTokenFetcherTest,
       WaitUntilAvailableShouldCallBackOnFulfilledRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // Since the refresh token is already available, this should result in an
  // immediate request for an access token.
  auto fetcher =
      CreateFetcher(account_id, callback.Get(),
                    AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);

  run_loop.Run();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));

  token_service()->IssueAllTokensForAccount(
      account_id, TokenResponseBuilder()
                      .WithAccessToken(access_token_info().token)
                      .WithExpirationTime(access_token_info().expiration_time)
                      .WithIdToken(access_token_info().id_token)
                      .build());
}

TEST_F(AccessTokenFetcherTest,
       WaitUntilAvailableShouldCallBackOnFulfilledRequestAfterTokenAvailable) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);

  // Since the refresh token is not available yet, this should just start
  // waiting for it.
  auto fetcher =
      CreateFetcher(account_id, callback.Get(),
                    AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);

  // Before the refresh token is available, the callback shouldn't get called.
  EXPECT_CALL(callback, Run(_, _)).Times(0);
  {
    token_service()->IssueAllTokensForAccount(
        account_id, TokenResponseBuilder()
                        .WithAccessToken(access_token_info().token)
                        .WithExpirationTime(access_token_info().expiration_time)
                        .WithIdToken(access_token_info().id_token)
                        .build());
  }

  // Once the refresh token becomes available, we should get an access token
  // request.
  token_service()->UpdateCredentials(account_id, "refresh token");

  run_loop.Run();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));

  {
    token_service()->IssueAllTokensForAccount(
        account_id, TokenResponseBuilder()
                        .WithAccessToken(access_token_info().token)
                        .WithExpirationTime(access_token_info().expiration_time)
                        .WithIdToken(access_token_info().id_token)
                        .build());
  }
}

TEST_F(AccessTokenFetcherTest,
       WaitUntilAvailableShouldIgnoreRefreshTokenForDifferentAccount) {
  TestTokenCallback callback;

  MockCallback<base::OnceClosure> access_token_request_callback;
  set_on_access_token_request_callback(access_token_request_callback.Get());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  CoreAccountId other_account_id =
      AddAccount(kTestGaiaId2, kTestEmail2).account_id;

  // Since the refresh token is not available yet, this should just start
  // waiting for it.
  auto fetcher =
      CreateFetcher(account_id, callback.Get(),
                    AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);

  // A refresh token for a different account should make no difference.
  EXPECT_CALL(callback, Run(_, _)).Times(0);
  EXPECT_CALL(access_token_request_callback, Run()).Times(0);
  token_service()->UpdateCredentials(other_account_id, "refresh token");

  base::RunLoop().RunUntilIdle();
}

TEST_F(AccessTokenFetcherTest, ShouldNotReplyIfDestroyed) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // Destroy the fetcher before the access token request is fulfilled.
  fetcher.reset();

  // Now fulfilling the access token request should have no effect.
  token_service()->IssueAllTokensForAccount(
      account_id, TokenResponseBuilder()
                      .WithAccessToken(access_token_info().token)
                      .WithExpirationTime(access_token_info().expiration_time)
                      .WithIdToken(access_token_info().id_token)
                      .build());
}

TEST_F(AccessTokenFetcherTest, ReturnsErrorWhenAccountHasNoRefreshToken) {
  TestTokenCallback callback;

  base::RunLoop run_loop;

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);

  // Account has no refresh token -> we should get called back.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError(
                      GoogleServiceAuthError::State::ACCOUNT_NOT_FOUND),
                  AccessTokenInfo()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  run_loop.Run();
}

TEST_F(AccessTokenFetcherTest, CanceledAccessTokenRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  base::RunLoop run_loop2;
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop2, &base::RunLoop::Quit));

  // A canceled access token request should result in a callback.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  run_loop2.Run();
}

TEST_F(AccessTokenFetcherTest, RefreshTokenRevoked) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  TestTokenCallback callback;

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // Revoke the refresh token, which should cancel all pending requests. The
  // fetcher should *not* retry.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_NOT_FOUND),
          AccessTokenInfo()));
  token_service()->RevokeCredentials(account_id);
}

TEST_F(AccessTokenFetcherTest, FailedAccessTokenRequest) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  TestTokenCallback callback;

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // We should immediately get called back with an empty access token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
          AccessTokenInfo()));
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

TEST_F(AccessTokenFetcherTest, MultipleRequestsForSameAccountFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // This should also result in a request for an access token.
  TestTokenCallback callback2;
  base::RunLoop run_loop2;
  set_on_access_token_request_callback(run_loop2.QuitClosure());
  auto fetcher2 = CreateFetcher(account_id, callback2.Get(),
                                AccessTokenFetcher::Mode::kImmediate);
  run_loop2.Run();

  // Once the access token request is fulfilled, both requests should get
  // called back with the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  EXPECT_CALL(callback2, Run(GoogleServiceAuthError::AuthErrorNone(),
                             access_token_info()));
  token_service()->IssueAllTokensForAccount(
      account_id, TokenResponseBuilder()
                      .WithAccessToken(access_token_info().token)
                      .WithExpirationTime(access_token_info().expiration_time)
                      .WithIdToken(access_token_info().id_token)
                      .build());
}

TEST_F(AccessTokenFetcherTest, MultipleRequestsForDifferentAccountsFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // Add a second account and request an access token for it.
  CoreAccountId account_id2 = AddAccount(kTestGaiaId2, kTestEmail2).account_id;
  token_service()->UpdateCredentials(account_id2, "refresh token");
  TestTokenCallback callback2;
  base::RunLoop run_loop2;
  set_on_access_token_request_callback(run_loop2.QuitClosure());
  auto fetcher2 = CreateFetcher(account_id2, callback2.Get(),
                                AccessTokenFetcher::Mode::kImmediate);
  run_loop2.Run();

  // Once the first access token request is fulfilled, it should get
  // called back with the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  {
    token_service()->IssueAllTokensForAccount(
        account_id, TokenResponseBuilder()
                        .WithAccessToken(access_token_info().token)
                        .WithExpirationTime(access_token_info().expiration_time)
                        .WithIdToken(access_token_info().id_token)
                        .build());
  }

  // Once the second access token request is fulfilled, it should get
  // called back with the access token.
  EXPECT_CALL(callback2, Run(GoogleServiceAuthError::AuthErrorNone(),
                             access_token_info()));
  {
    token_service()->IssueAllTokensForAccount(
        account_id2,
        TokenResponseBuilder()
            .WithAccessToken(access_token_info().token)
            .WithExpirationTime(access_token_info().expiration_time)
            .WithIdToken(access_token_info().id_token)
            .build());
  }
}

TEST_F(AccessTokenFetcherTest,
       MultipleRequestsForDifferentAccountsCanceledAndFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);
  run_loop.Run();

  // Add a second account and request an access token for it.
  CoreAccountId account_id2 = AddAccount(kTestGaiaId2, kTestEmail2).account_id;
  token_service()->UpdateCredentials(account_id2, "refresh token");

  base::RunLoop run_loop2;
  set_on_access_token_request_callback(run_loop2.QuitClosure());

  TestTokenCallback callback2;
  auto fetcher2 = CreateFetcher(account_id2, callback2.Get(),
                                AccessTokenFetcher::Mode::kImmediate);
  run_loop2.Run();

  // Cancel the first access token request: This should result in a callback
  // for the first fetcher.
  base::RunLoop run_loop3;
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop3, &base::RunLoop::Quit));

  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  run_loop3.Run();

  // Once the second access token request is fulfilled, it should get
  // called back with the access token.
  base::RunLoop run_loop4;
  EXPECT_CALL(callback2,
              Run(GoogleServiceAuthError::AuthErrorNone(), access_token_info()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop4, &base::RunLoop::Quit));
  token_service()->IssueAllTokensForAccount(
      account_id2, TokenResponseBuilder()
                       .WithAccessToken(access_token_info().token)
                       .WithExpirationTime(access_token_info().expiration_time)
                       .WithIdToken(access_token_info().id_token)
                       .build());

  run_loop4.Run();
}

TEST_F(AccessTokenFetcherTest, FetcherWithCustomURLLoaderFactory) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_url_loader_factory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  // This should result in a request for an access token.
  TestTokenCallback callback;
  auto fetcher = CreateFetcherWithURLLoaderFactory(
      account_id, test_shared_url_loader_factory, callback.Get(),
      AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // The URLLoaderFactory present in the pending request should match
  // the one we specified when creating the AccessTokenFetcher.
  std::vector<FakeOAuth2AccessTokenManager::PendingRequest> pending_requests =
      token_service()->GetPendingRequests();

  EXPECT_EQ(pending_requests.size(), 1U);
  EXPECT_EQ(pending_requests[0].url_loader_factory,
            test_shared_url_loader_factory);

  // Once the access token request is fulfilled, we should get called back
  // with the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  token_service()->IssueAllTokensForAccount(
      account_id, TokenResponseBuilder()
                      .WithAccessToken(access_token_info().token)
                      .WithExpirationTime(access_token_info().expiration_time)
                      .WithIdToken(access_token_info().id_token)
                      .build());

  // Now add a second account and request an access token for it to test
  // that the default URLLoaderFactory is used if none is specified.
  base::RunLoop run_loop2;
  TestTokenCallback callback2;

  set_on_access_token_request_callback(run_loop2.QuitClosure());
  CoreAccountId account_id2 = AddAccount(kTestGaiaId2, kTestEmail2).account_id;
  token_service()->UpdateCredentials(account_id2, "refresh token");

  // CreateFetcher will create an AccessTokenFetcher without specifying
  // any URLLoaderFactory, so that the default one will be used.
  auto fetcher2 = CreateFetcher(account_id2, callback2.Get(),
                                AccessTokenFetcher::Mode::kImmediate);

  run_loop2.Run();

  // There should be one pending request in this case too.
  std::vector<FakeOAuth2AccessTokenManager::PendingRequest> pending_requests2 =
      token_service()->GetPendingRequests();
  EXPECT_EQ(pending_requests2.size(), 1U);

  // The URLLoaderFactory present in the pending request should match
  // the one created by default for the token service's delegate.
  ProfileOAuth2TokenServiceDelegate* service_delegate =
      token_service()->GetDelegate();
  EXPECT_EQ(pending_requests2[0].url_loader_factory,
            service_delegate->GetURLLoaderFactory());

  // Check that everything worked as expected in this case as well.
  EXPECT_CALL(callback2, Run(GoogleServiceAuthError::AuthErrorNone(),
                             access_token_info()));
  token_service()->IssueAllTokensForAccount(
      account_id2, TokenResponseBuilder()
                       .WithAccessToken(access_token_info().token)
                       .WithExpirationTime(access_token_info().expiration_time)
                       .WithIdToken(access_token_info().id_token)
                       .build());
}

// APIs consent tests.

TEST_F(AccessTokenFetcherTest, FetcherWithUnrestrictedOAuth2Scope) {
  CoreAccountInfo account = AddAccount(kTestGaiaId, kTestEmail);
  EXPECT_FALSE(primary_account_manager().HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  VerifyScopeAccess(account.account_id, OAuthConsumerId::kProfileDownloader);
}

// Tests that a request with a signed-in client accessing an OAuth2 API
// that requires sign-in is fulfilled.
TEST_F(AccessTokenFetcherTest, FetcherWithSignedInClientAccessToConsentAPI) {
  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  VerifyScopeAccess(account_id, OAuthConsumerId::kTokenHandleService);
}

// Tests that a request with a privileged client accessing a privileged OAuth2
// API is fulfilled.
TEST_F(AccessTokenFetcherTest,
       FetcherWithPriviledgedClientAccessToPriveledgedAPI) {
  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  VerifyScopeAccess(account_id, OAuthConsumerId::kExtensionsIdentityAPI);
}

// Tests that a request with a privileged client accessing an OAuth2 API
// that requires sign-in is fulfilled.
TEST_F(AccessTokenFetcherTest, FetcherWithPriveledgedClientAccessToConsentAPI) {
  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  VerifyScopeAccess(
      account_id, OAuthConsumerId::kExtensionsIdentityAPI,
      GetOAuthConsumerFromId(OAuthConsumerId::kTokenHandleService));
}

// Tests that a request with a signed-in client accessing a privileged OAuth2
// API fails.
TEST_F(AccessTokenFetcherTest, FetcherWithSignedInClientAccessToPrivilegedAPI) {
  CoreAccountId account_id = SetPrimaryAccount(kTestGaiaId, kTestEmail);
  EXPECT_CHECK_DEATH_WITH(
      VerifyScopeAccess(
          account_id, OAuthConsumerId::kTokenHandleService,
          GetOAuthConsumerFromId(OAuthConsumerId::kExtensionsIdentityAPI)),
      "You are attempting to access a privileged scope");
}

}  // namespace signin

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
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/access_token_restriction.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
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

const char kTestGaiaId[] = "dummyId";
const char kTestGaiaId2[] = "dummyId2";
const char kTestEmail[] = "me@gmail.com";
const char kTestEmail2[] = "me2@gmail.com";

// Used just to check that the id_token is passed along.
const char kIdTokenEmptyServices[] =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".dummy-signature";

const char kPriviligedConsumerName[] = "extensions_identity_api";

}  // namespace

class AccessTokenFetcherTest
    : public testing::Test,
      public testing::WithParamInterface<bool>,
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

    account_tracker_->Initialize(&pref_service_, base::FilePath());
    primary_account_manager_ = std::make_unique<PrimaryAccountManager>(
        &signin_client_, &token_service_, account_tracker_.get());
    token_service_.AddAccessTokenDiagnosticsObserver(this);
  }

  ~AccessTokenFetcherTest() override {
    token_service_.RemoveAccessTokenDiagnosticsObserver(this);
  }

  CoreAccountId SetPrimaryAccount(const std::string& gaia_id,
                                  const std::string& email,
                                  ConsentLevel consent_level) {
    CoreAccountInfo account_info = AddAccount(gaia_id, email);
    primary_account_manager_->SetPrimaryAccountInfo(
        account_info, consent_level,
        signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

    return account_info.account_id;
  }

  CoreAccountInfo AddAccount(const std::string& gaia_id,
                             const std::string& email) {
    account_tracker()->SeedAccountInfo(gaia_id, email);
    return account_tracker()->FindAccountInfoByGaiaId(gaia_id);
  }

  // Verifies that the consumer_name has the appropriate consent level for the
  // scopes it requests to access.
  void VerifyScopeAccess(CoreAccountId account_id,
                         const std::string consumer_name,
                         std::set<std::string> scopes) {
    TestTokenCallback callback;

    base::RunLoop run_loop;
    set_on_access_token_request_callback(run_loop.QuitClosure());

    token_service()->UpdateCredentials(account_id, "refresh token");

    // Since the refresh token is already available, this should result in an
    // immediate request for an access token.
    auto fetcher =
        CreateFetcher(account_id, callback.Get(),
                      AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable,
                      scopes, consumer_name);

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
    std::set<std::string> scopes = {"scope"};
    return CreateFetcher(account_id, std::move(callback), mode, scopes);
  }

  std::unique_ptr<AccessTokenFetcher> CreateFetcherWithURLLoaderFactory(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode) {
    std::set<std::string> scopes{"scope"};
    return std::make_unique<AccessTokenFetcher>(
        account_id, "test_consumer", &token_service_,
        primary_account_manager_.get(), url_loader_factory, scopes,
        std::move(callback), mode, RequireSyncConsentForScopeVerification());
  }

  AccountTrackerService* account_tracker() { return account_tracker_.get(); }

  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

  void set_on_access_token_request_callback(base::OnceClosure callback) {
    on_access_token_request_callback_ = std::move(callback);
  }

  // Returns an AccessTokenInfo with valid information that can be used for
  // completing access token requests.
  AccessTokenInfo access_token_info() { return access_token_info_; }

  bool RequireSyncConsentForScopeVerification() const { return GetParam(); }

  ConsentLevel GetTargetConsentLevel() const {
    // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is
    // deleted. See ConsentLevel::kSync documentation for details.
    return RequireSyncConsentForScopeVerification() ? ConsentLevel::kSync
                                                    : ConsentLevel::kSignin;
  }

  const PrimaryAccountManager& primary_account_manager() {
    return *primary_account_manager_;
  }

 private:
  std::unique_ptr<AccountTrackerService> CreateAccountTrackerService() {
#if BUILDFLAG(IS_ANDROID)
    SetUpMockAccountManagerFacade();
#endif
    return std::make_unique<AccountTrackerService>();
  }

  std::unique_ptr<AccessTokenFetcher> CreateFetcher(
      const CoreAccountId& account_id,
      AccessTokenFetcher::TokenCallback callback,
      AccessTokenFetcher::Mode mode,
      std::set<std::string> scopes,
      std::string oauth_consumer_name = "test_consumer") {
    return std::make_unique<AccessTokenFetcher>(
        account_id, oauth_consumer_name, &token_service_,
        primary_account_manager_.get(), scopes, std::move(callback), mode,
        RequireSyncConsentForScopeVerification());
  }

  // OAuth2AccessTokenManager::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const CoreAccountId& account_id,
      const std::string& consumer_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override {
    if (on_access_token_request_callback_)
      std::move(on_access_token_request_callback_).Run();
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

INSTANTIATE_TEST_SUITE_P(All, AccessTokenFetcherTest, testing::Bool());

TEST_P(AccessTokenFetcherTest, EmptyAccountFailsButDoesNotCrash) {
  TestTokenCallback callback;

  base::RunLoop run_loop;

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(CoreAccountId(), callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  // Fetching access tokens for an empty account id should respond with
  // USER_NOT_SIGNED_UP error.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError(
                      GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                  AccessTokenInfo()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  run_loop.Run();
}

TEST_P(AccessTokenFetcherTest, OneShotShouldCallBackOnFulfilledRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest,
       WaitUntilAvailableShouldCallBackOnFulfilledRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest,
       WaitUntilAvailableShouldCallBackOnFulfilledRequestAfterTokenAvailable) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());

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

TEST_P(AccessTokenFetcherTest,
       WaitUntilAvailableShouldIgnoreRefreshTokenForDifferentAccount) {
  TestTokenCallback callback;

  MockCallback<base::OnceClosure> access_token_request_callback;
  set_on_access_token_request_callback(access_token_request_callback.Get());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest, ShouldNotReplyIfDestroyed) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest, ReturnsErrorWhenAccountHasNoRefreshToken) {
  TestTokenCallback callback;

  base::RunLoop run_loop;

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());

  // Account has no refresh token -> we should get called back.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError(
                      GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                  AccessTokenInfo()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  run_loop.Run();
}

TEST_P(AccessTokenFetcherTest, CanceledAccessTokenRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest, RefreshTokenRevoked) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  TestTokenCallback callback;

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get(),
                               AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // Revoke the refresh token, which should cancel all pending requests. The
  // fetcher should *not* retry.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP),
          AccessTokenInfo()));
  token_service()->RevokeCredentials(account_id);
}

TEST_P(AccessTokenFetcherTest, FailedAccessTokenRequest) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  TestTokenCallback callback;

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest, MultipleRequestsForSameAccountFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest, MultipleRequestsForDifferentAccountsFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest,
       MultipleRequestsForDifferentAccountsCanceledAndFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest, FetcherWithCustomURLLoaderFactory) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
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

TEST_P(AccessTokenFetcherTest, FetcherWithUnrestrictedOAuth2Scope) {
  CoreAccountInfo account = AddAccount(kTestGaiaId, kTestEmail);
  EXPECT_FALSE(primary_account_manager().HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  VerifyScopeAccess(account.account_id, "test_consumer",
                    {GaiaConstants::kGoogleUserInfoEmail});
}

// Tests that a request with a consented client accessing an OAuth2 API
// that requires sync consent is fulfilled.
TEST_P(AccessTokenFetcherTest, FetcherWithConsentedClientAccessToConsentAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
  VerifyScopeAccess(account_id, "test_consumer",
                    {GaiaConstants::kOAuth1LoginScope});
}

// Tests that a request with a consented client accessing an OAuth2 API
// that does not require sync consent is fulfilled.
TEST_P(AccessTokenFetcherTest,
       FetcherWithConsentedClientAccessToUnconsentedAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
  VerifyScopeAccess(account_id, "test_consumer",
                    {GaiaConstants::kChromeSafeBrowsingOAuth2Scope});
}

// Tests that a request with an unconsented client accessing an OAuth2 API
// that does not require consent is fulfilled.
TEST_P(AccessTokenFetcherTest,
       FetcherWithUnconsentedClientAccessToUnconsentedAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, ConsentLevel::kSignin);
  VerifyScopeAccess(account_id, "test_consumer",
                    {GaiaConstants::kChromeSafeBrowsingOAuth2Scope});
}

// Tests that a request with a privileged client accessing a privileged OAuth2
// API is fulfilled.
TEST_P(AccessTokenFetcherTest,
       FetcherWithPriviledgedClientAccessToPriveledgedAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, ConsentLevel::kSignin);
  VerifyScopeAccess(account_id, kPriviligedConsumerName,
                    {GaiaConstants::kAnyApiOAuth2Scope});
}

// Tests that a request with a privileged client accessing an OAuth2 API
// that requires sync consent is fulfilled.
TEST_P(AccessTokenFetcherTest, FetcherWithPriveledgedClientAccessToConsentAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, ConsentLevel::kSignin);
  VerifyScopeAccess(account_id, kPriviligedConsumerName,
                    {GaiaConstants::kOAuth1LoginScope});
}

// Tests that a request with a privileged client accessing an OAuth2 API
// that does not require consent is fulfilled.
TEST_P(AccessTokenFetcherTest,
       FetcherWithPriveledgedClientAccessToUnconsentedAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, ConsentLevel::kSignin);
  VerifyScopeAccess(account_id, kPriviligedConsumerName,
                    {GaiaConstants::kChromeSafeBrowsingOAuth2Scope});
}

// Tests that a request with an unconsented client accessing an OAuth2 API
// that requires privileged access fails.
TEST_P(AccessTokenFetcherTest,
       FetcherWithUnconsentedClientAccessToPrivelegedAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, ConsentLevel::kSignin);
  EXPECT_CHECK_DEATH_WITH(
      VerifyScopeAccess(account_id, "test_consumer",
                        {GaiaConstants::kAnyApiOAuth2Scope}),
      "You are attempting to access a privileged scope");
}

// Tests that a request with a consented client accessing a privileged OAuth2
// API fails.
TEST_P(AccessTokenFetcherTest,
       FetcherWithConsentedClientAccessToPrivilegedAPI) {
  CoreAccountId account_id =
      SetPrimaryAccount(kTestGaiaId, kTestEmail, GetTargetConsentLevel());
  EXPECT_CHECK_DEATH_WITH(
      VerifyScopeAccess(account_id, "test_consumer",
                        {GaiaConstants::kAnyApiOAuth2Scope}),
      "You are attempting to access a privileged scope");
}

}  // namespace signin

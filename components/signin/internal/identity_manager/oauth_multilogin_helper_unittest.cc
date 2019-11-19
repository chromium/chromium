// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

const CoreAccountId kAccountId("account_id_1");
const CoreAccountId kAccountId2("account_id_2");
const char kGaiaId[] = "gaia_id_1";
const char kGaiaId2[] = "gaia_id_2";
const char kAccessToken[] = "access_token_1";
const char kAccessToken2[] = "access_token_2";

const char kExternalCcResult[] = "youtube:OK";

constexpr int kMaxFetcherRetries = 3;

const char kMultiloginSuccessResponse[] =
    R"()]}'
       {
         "status": "OK",
         "cookies":[
           {
             "name":"SID",
             "value":"SID_value",
             "domain":".google.fr",
             "path":"/",
             "isSecure":true,
             "isHttpOnly":false,
             "priority":"HIGH",
             "maxAge":63070000
           }
         ]
       }
      )";

const char kMultiloginSuccessResponseTwoCookies[] =
    R"()]}'
       {
         "status": "OK",
         "cookies":[
           {
             "name":"SID",
             "value":"SID_value",
             "domain":".google.fr",
             "path":"/",
             "isSecure":true,
             "isHttpOnly":false,
             "priority":"HIGH",
             "maxAge":63070000
           },
           {
             "name":"FOO",
             "value":"FOO_value",
             "domain":".google.com",
             "path":"/",
             "isSecure":true,
             "isHttpOnly":false,
             "priority":"HIGH",
             "maxAge":63070000
           }
         ]
       }
      )";

const char kMultiloginSuccessResponseWithSecondaryDomain[] =
    R"()]}'
       {
         "status": "OK",
         "cookies":[
           {
             "name":"SID",
             "value":"SID_value",
             "domain":".youtube.com",
             "path":"/",
             "isSecure":true,
             "isHttpOnly":false,
             "priority":"HIGH",
             "maxAge":63070000
           },
           {
             "name":"FOO",
             "value":"FOO_value",
             "domain":".google.com",
             "path":"/",
             "isSecure":true,
             "isHttpOnly":false,
             "priority":"HIGH",
             "maxAge":63070000
           }
         ]
       }
      )";

const char kMultiloginRetryResponse[] =
    R"()]}'
       {
         "status": "RETRY"
       }
      )";

const char kMultiloginInvalidTokenResponse[] =
    R"()]}'
       {
         "status": "INVALID_TOKENS",
         "failed_accounts": [
           { "obfuscated_id": "gaia_id_1", "status": "RECOVERABLE" },
           { "obfuscated_id": "gaia_id_2", "status": "OK" }
         ]
       }
      )";

// GMock matcher that checks that the cookie has the expected parameters.
MATCHER_P3(CookieMatcher, name, value, domain, "") {
  return arg.Name() == name && arg.Value() == value && arg.Domain() == domain &&
         arg.Path() == "/" && arg.IsSecure() && !arg.IsHttpOnly();
}

void RunSetCookieCallbackWithSuccess(
    const net::CanonicalCookie&,
    const std::string&,
    const net::CookieOptions&,
    network::mojom::CookieManager::SetCanonicalCookieCallback callback) {
  std::move(callback).Run(net::CanonicalCookie::CookieInclusionStatus());
}

class MockCookieManager
    : public ::testing::StrictMock<network::TestCookieManager> {
 public:
  MOCK_METHOD4(SetCanonicalCookie,
               void(const net::CanonicalCookie& cookie,
                    const std::string& source_scheme,
                    const net::CookieOptions& cookie_options,
                    SetCanonicalCookieCallback callback));
};

class MockTokenService : public FakeProfileOAuth2TokenService {
 public:
  MockTokenService(PrefService* prefs) : FakeProfileOAuth2TokenService(prefs) {}

  MOCK_METHOD2(InvalidateTokenForMultilogin,
               void(const CoreAccountId& account_id, const std::string& token));
};

}  // namespace

class OAuthMultiloginHelperTest : public testing::Test {
 public:
  OAuthMultiloginHelperTest()
      : test_signin_client_(&pref_service_),
        mock_token_service_(&pref_service_) {
    std::unique_ptr<MockCookieManager> cookie_manager =
        std::make_unique<MockCookieManager>();
    mock_cookie_manager_ = cookie_manager.get();
    test_signin_client_.set_cookie_manager(std::move(cookie_manager));
  }

  ~OAuthMultiloginHelperTest() override = default;

  std::unique_ptr<OAuthMultiloginHelper> CreateHelper(
      const std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair>
          accounts) {
    return std::make_unique<OAuthMultiloginHelper>(
        &test_signin_client_, token_service(),
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER, accounts,
        std::string(),
        base::BindOnce(&OAuthMultiloginHelperTest::OnOAuthMultiloginFinished,
                       base::Unretained(this)));
  }

  std::unique_ptr<OAuthMultiloginHelper> CreateHelperWithExternalCcResult(
      const std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair>
          accounts) {
    return std::make_unique<OAuthMultiloginHelper>(
        &test_signin_client_, token_service(),
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER, accounts,
        kExternalCcResult,
        base::BindOnce(&OAuthMultiloginHelperTest::OnOAuthMultiloginFinished,
                       base::Unretained(this)));
  }

  network::TestURLLoaderFactory* url_loader() {
    return test_signin_client_.GetTestURLLoaderFactory();
  }

  std::string multilogin_url() const {
    return GaiaUrls::GetInstance()->oauth_multilogin_url().spec() +
           "?source=ChromiumBrowser&mlreuse=0";
  }

  std::string multilogin_url_with_external_cc_result() const {
    return GaiaUrls::GetInstance()->oauth_multilogin_url().spec() +
           "?source=ChromiumBrowser&mlreuse=0&externalCcResult=" +
           kExternalCcResult;
  }

  MockCookieManager* cookie_manager() { return mock_cookie_manager_; }
  MockTokenService* token_service() { return &mock_token_service_; }

 protected:
  void OnOAuthMultiloginFinished(SetAccountsInCookieResult result) {
    DCHECK(!callback_called_);
    callback_called_ = true;
    result_ = result;
  }

  base::test::TaskEnvironment task_environment_;

  bool callback_called_ = false;
  SetAccountsInCookieResult result_;

  TestingPrefServiceSimple pref_service_;
  MockCookieManager* mock_cookie_manager_;  // Owned by test_signin_client_
  TestSigninClient test_signin_client_;
  MockTokenService mock_token_service_;
};

// Everything succeeds.
TEST_F(OAuthMultiloginHelperTest, Success) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         "https", testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Multiple cookies in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, MultipleCookies) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         "https", testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("FOO", "FOO_value", ".google.com"),
                         "https", testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(),
                            kMultiloginSuccessResponseTwoCookies);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Multiple cookies in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, SuccessWithExternalCcResult) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelperWithExternalCcResult({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".youtube.com"),
                         "https", testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("FOO", "FOO_value", ".google.com"),
                         "https", testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(
      url_loader()->IsPending(multilogin_url_with_external_cc_result()));
  url_loader()->AddResponse(multilogin_url_with_external_cc_result(),
                            kMultiloginSuccessResponseWithSecondaryDomain);
  EXPECT_FALSE(
      url_loader()->IsPending(multilogin_url_with_external_cc_result()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Failure to get the access token.
TEST_F(OAuthMultiloginHelperTest, OneAccountAccessTokenFailure) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}});

  token_service()->IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kPersistentError, result_);
}

// Retry on transient errors in the multilogin call.
TEST_F(OAuthMultiloginHelperTest, OneAccountTransientMultiloginError) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         "https", testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call fails with transient error.
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->SimulateResponseForPendingRequest(multilogin_url(),
                                                  kMultiloginRetryResponse);

  // Call is retried and succeeds.
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Stop retrying after too many transient errors in the multilogin call.
TEST_F(OAuthMultiloginHelperTest,
       OneAccountTransientMultiloginErrorMaxRetries) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}});

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;

  // Multilogin call fails with transient error, retry many times.
  for (int i = 0; i < kMaxFetcherRetries; ++i) {
    token_service()->IssueAllTokensForAccount(kAccountId, success_response);
    EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
    EXPECT_FALSE(callback_called_);
    url_loader()->SimulateResponseForPendingRequest(multilogin_url(),
                                                    kMultiloginRetryResponse);
  }

  // Failure after exceeding the maximum number of retries.
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kTransientError, result_);
}

// Persistent error in the multilogin call.
TEST_F(OAuthMultiloginHelperTest, OneAccountPersistentMultiloginError) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}});

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call fails with persistent error.
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(), "blah");  // Unexpected response.
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kPersistentError, result_);
}

// Retry on "invalid token" in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, InvalidTokenError) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  token_service()->UpdateCredentials(kAccountId2, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}, {kAccountId2, kGaiaId2}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         "https", testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // The failed access token should be invalidated.
  EXPECT_CALL(*token_service(),
              InvalidateTokenForMultilogin(kAccountId, kAccessToken));

  // Issue access tokens.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);
  OAuth2AccessTokenConsumer::TokenResponse success_response_2;
  success_response_2.access_token = kAccessToken2;
  token_service()->IssueAllTokensForAccount(kAccountId2, success_response_2);

  // Multilogin call fails with invalid token for kAccountId.
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->SimulateResponseForPendingRequest(
      multilogin_url(), kMultiloginInvalidTokenResponse);

  // Both tokens are retried.
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);
  EXPECT_FALSE(callback_called_);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  token_service()->IssueAllTokensForAccount(kAccountId2, success_response);

  // Multilogin succeeds the second time.
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Retry on "invalid token" in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, InvalidTokenErrorMaxRetries) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  token_service()->UpdateCredentials(kAccountId2, "refresh_token");
  std::unique_ptr<OAuthMultiloginHelper> helper =
      CreateHelper({{kAccountId, kGaiaId}, {kAccountId2, kGaiaId2}});

  // The failed access token should be invalidated.
  EXPECT_CALL(*token_service(),
              InvalidateTokenForMultilogin(kAccountId, kAccessToken))
      .Times(kMaxFetcherRetries);

  // Issue access tokens.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  OAuth2AccessTokenConsumer::TokenResponse success_response_2;
  success_response_2.access_token = kAccessToken2;

  // Multilogin call fails with invalid token for kAccountId. Retry many times.
  for (int i = 0; i < kMaxFetcherRetries; ++i) {
    token_service()->IssueAllTokensForAccount(kAccountId, success_response);
    EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
    token_service()->IssueAllTokensForAccount(kAccountId2, success_response_2);

    EXPECT_FALSE(callback_called_);
    EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));

    url_loader()->SimulateResponseForPendingRequest(
        multilogin_url(), kMultiloginInvalidTokenResponse);
  }

  // The maximum number of retries is reached, fail.
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kTransientError, result_);
}

}  // namespace signin

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"

#include <memory>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace signin {

namespace {

constexpr GaiaId::Literal kGaiaId("gaia_id_1");
constexpr GaiaId::Literal kGaiaId2("gaia_id_2");
constexpr char kAccessToken[] = "access_token_1";
constexpr char kAccessToken2[] = "access_token_2";
constexpr char kNoAssertion[] = "";

const char kExternalCcResult[] = "youtube:OK";

constexpr int kMaxFetcherRetries = 3;

constexpr char kAuthorizationHeaderName[] = "Authorization";

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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
const char kMultiloginRetryWithTokenBindingAssertionResponseFormat[] =
    R"()]}'
       {
         "status": "RETRY",
         "failed_accounts": [
           {
             "obfuscated_id": "%s",
             "status": "RECOVERABLE",
             "token_binding_retry_response": {
               "challenge": "%s"
             }
           }
         ]
       }
      )";

const char kMultiloginSuccessWithEncryptedCookieResponseFormat[] =
    R"()]}'
       {
         "status": "OK",
         "token_binding_directed_response": {},
         "cookies":[
           {
             "name":"SID",
             "value":"%s",
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
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

// GMock matcher that checks that the cookie has the expected parameters.
MATCHER_P3(CookieMatcher, name, value, domain, "") {
  return arg.Name() == name && arg.Value() == value && arg.Domain() == domain &&
         arg.Path() == "/" && arg.SecureAttribute() && !arg.IsHttpOnly();
}

// Checks that the argument (a GURL) is secure and has the given hostname.
MATCHER_P(CookieSourceMatcher, cookie_host, "") {
  return arg.is_valid() && arg.scheme() == "https" && arg.host() == cookie_host;
}

void RunSetCookieCallbackWithSuccess(
    const net::CanonicalCookie&,
    const GURL&,
    const net::CookieOptions&,
    network::mojom::CookieManager::SetCanonicalCookieCallback callback) {
  std::move(callback).Run(net::CookieAccessResult());
}

std::string CreateMultiBearerAuthorizationHeader(
    const std::vector<gaia::MultiloginAccountAuthCredentials>& accounts) {
  std::vector<std::string> token_id_pairs = base::ToVector(
      accounts, [](const gaia::MultiloginAccountAuthCredentials& credentials) {
        return base::StrCat(
            {credentials.token, ":", credentials.gaia_id.ToString()});
      });
  return base::StrCat({"MultiBearer ", base::JoinString(token_id_pairs, ",")});
}

class MockCookieManager
    : public ::testing::StrictMock<network::TestCookieManager> {
 public:
  MOCK_METHOD4(SetCanonicalCookie,
               void(const net::CanonicalCookie& cookie,
                    const GURL& source_url,
                    const net::CookieOptions& cookie_options,
                    SetCanonicalCookieCallback callback));
};

class MockTokenService : public FakeProfileOAuth2TokenService {
 public:
  explicit MockTokenService(PrefService* prefs)
      : FakeProfileOAuth2TokenService(prefs) {}
  MockTokenService(PrefService* prefs,
                   std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate)
      : FakeProfileOAuth2TokenService(prefs, std::move(delegate)) {}

  MOCK_METHOD2(MockInvalidateTokenForMultilogin,
               void(const CoreAccountId& account_id, const std::string& token));

  // Notifies the mock and then calls the base class method.
  void InvalidateTokenForMultilogin(const CoreAccountId& account_id,
                                    const std::string& token) override {
    MockInvalidateTokenForMultilogin(account_id, token);
    FakeProfileOAuth2TokenService::InvalidateTokenForMultilogin(account_id,
                                                                token);
  }
};

// This class enables using refresh tokens in Multilogin calls, which is the
// default behaviour on Desktop platforms.
class FakeProfileOAuth2TokenServiceDelegateDesktop
    : public FakeProfileOAuth2TokenServiceDelegate {
  std::string GetTokenForMultilogin(
      const CoreAccountId& account_id) const override {
    if (GetAuthError(account_id) == GoogleServiceAuthError::AuthErrorNone()) {
      return GetRefreshToken(account_id);
    }
    return std::string();
  }
  void InvalidateTokenForMultilogin(
      const CoreAccountId& failed_account) override {
    UpdateAuthError(failed_account,
                    GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                        GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                            CREDENTIALS_REJECTED_BY_SERVER));
  }
};

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
class MockBoundSessionOAuthMultiLoginDelegate
    : public ::testing::StrictMock<BoundSessionOAuthMultiLoginDelegate> {
 public:
  MOCK_METHOD(void,
              BeforeSetCookies,
              (const OAuthMultiloginResult&),
              (override));
  MOCK_METHOD(void, OnCookiesSet, (), (override));
};

std::string CreateMultiOAuthAuthorizationHeader(
    const std::vector<gaia::MultiloginAccountAuthCredentials>& accounts) {
  return base::StrCat({"MultiOAuth ", gaia::CreateMultiOAuthHeader(accounts)});
}
#endif
}  // namespace

class OAuthMultiloginHelperTest
    : public testing::Test,
      public AccountsCookieMutator::PartitionDelegate {
 public:
  OAuthMultiloginHelperTest()
      : kAccountId(CoreAccountId::FromGaiaId(kGaiaId)),
        kAccountId2(CoreAccountId::FromGaiaId(kGaiaId2)),
        test_signin_client_(&pref_service_),
        mock_token_service_(
            std::make_unique<MockTokenService>(&pref_service_)) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    test_signin_client_.SetBoundSessionOauthMultiloginDelegateFactory(
        base::BindRepeating(&OAuthMultiloginHelperTest::
                                CreateMockBoundSessionOAuthMultiLoginDelegate,
                            base::Unretained(this)));
#endif
  }

  ~OAuthMultiloginHelperTest() override = default;

  OAuthMultiloginHelper* CreateHelper(
      const std::vector<OAuthMultiloginHelper::AccountIdGaiaIdPair> accounts,
      bool set_external_cc_result = false) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    // `bound_session_delegate_` is owned by `OAuthMultiloginHelper`, ensures it
    // resets before creating a new helper to avoid dangling pointers.
    bound_session_delegate_ = nullptr;
#endif
    helper_ = std::make_unique<OAuthMultiloginHelper>(
        &test_signin_client_, this, token_service(),
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER, accounts,
        set_external_cc_result ? kExternalCcResult : std::string(),
        gaia::GaiaSource::kChrome,
        base::BindOnce(&OAuthMultiloginHelperTest::OnOAuthMultiloginFinished,
                       base::Unretained(this)));
    return helper_.get();
  }

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::unique_ptr<BoundSessionOAuthMultiLoginDelegate>
  CreateMockBoundSessionOAuthMultiLoginDelegate() {
    auto delegate = std::make_unique<MockBoundSessionOAuthMultiLoginDelegate>();
    bound_session_delegate_ = delegate.get();
    return delegate;
  }

  MockBoundSessionOAuthMultiLoginDelegate* bound_session_delegate() {
    return bound_session_delegate_;
  }
#endif

  network::TestURLLoaderFactory* url_loader() {
    return test_signin_client_.GetTestURLLoaderFactory();
  }

  std::string multilogin_url() const {
    return GaiaUrls::GetInstance()->oauth_multilogin_url().spec() +
           "?source=ChromiumBrowser&reuseCookies=0";
  }

  std::string multilogin_url_with_external_cc_result() const {
    return GaiaUrls::GetInstance()->oauth_multilogin_url().spec() +
           "?source=ChromiumBrowser&reuseCookies=0&externalCcResult=" +
           kExternalCcResult;
  }

  MockCookieManager* cookie_manager() { return &mock_cookie_manager_; }
  MockTokenService* token_service() { return mock_token_service_.get(); }

  void ReplaceTokenService(bool use_refresh_tokens_for_multilogin) {
    if (use_refresh_tokens_for_multilogin) {
      mock_token_service_ = std::make_unique<MockTokenService>(
          &pref_service_,
          std::make_unique<FakeProfileOAuth2TokenServiceDelegateDesktop>());
    } else {
      mock_token_service_ = std::make_unique<MockTokenService>(&pref_service_);
    }
  }

 protected:
  void OnOAuthMultiloginFinished(SetAccountsInCookieResult result) {
    DCHECK(!callback_called_);
    callback_called_ = true;
    result_ = result;
  }

  // AccountsCookieMuator::PartitionDelegate:
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
      GaiaAuthConsumer* consumer,
      const gaia::GaiaSource& source) override {
    return test_signin_client_.CreateGaiaAuthFetcher(consumer, source);
  }

  network::mojom::CookieManager* GetCookieManagerForPartition() override {
    return &mock_cookie_manager_;
  }

  const CoreAccountId kAccountId;
  const CoreAccountId kAccountId2;
  base::test::TaskEnvironment task_environment_;

  bool callback_called_ = false;
  SetAccountsInCookieResult result_;

  TestingPrefServiceSimple pref_service_;
  MockCookieManager mock_cookie_manager_;
  TestSigninClient test_signin_client_;
  std::unique_ptr<MockTokenService> mock_token_service_;
  std::unique_ptr<OAuthMultiloginHelper> helper_;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  raw_ptr<MockBoundSessionOAuthMultiLoginDelegate> bound_session_delegate_ =
      nullptr;
#endif
};

// Everything succeeds.
TEST_F(OAuthMultiloginHelperTest, Success) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiBearerAuthorizationHeader(
                {gaia::MultiloginAccountAuthCredentials(kGaiaId, kAccessToken,
                                                        kNoAssertion)}));
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

TEST_F(OAuthMultiloginHelperTest, SuccessWithRefreshToken) {
  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiBearerAuthorizationHeader(
                {gaia::MultiloginAccountAuthCredentials(
                    kGaiaId, "refresh_token", kNoAssertion)}));
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Multilogin request for multiple accounts.
TEST_F(OAuthMultiloginHelperTest, MultipleAccounts) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  token_service()->UpdateCredentials(kAccountId2, "refresh_token_2");
  // The order of accounts must be respected.
  CreateHelper({{kAccountId2, kGaiaId2}, {kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access tokens.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);
  OAuth2AccessTokenConsumer::TokenResponse success_response_2;
  success_response_2.access_token = kAccessToken2;
  token_service()->IssueAllTokensForAccount(kAccountId2, success_response_2);

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiBearerAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(kGaiaId2, kAccessToken2,
                                                       kNoAssertion),
                gaia::MultiloginAccountAuthCredentials(kGaiaId, kAccessToken,
                                                       kNoAssertion),
            }));
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Multiple cookies in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, MultipleCookies) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("FOO", "FOO_value", ".google.com"),
                  CookieSourceMatcher("google.com"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif
  url_loader()->AddResponse(multilogin_url(),
                            kMultiloginSuccessResponseTwoCookies);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Multiple cookies in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, SuccessWithExternalCcResult) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}},
               /*set_external_cc_result=*/true);

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".youtube.com"),
                  CookieSourceMatcher("youtube.com"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("FOO", "FOO_value", ".google.com"),
                  CookieSourceMatcher("google.com"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  EXPECT_TRUE(
      url_loader()->IsPending(multilogin_url_with_external_cc_result()));
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif
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
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
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
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

// Stop retrying after too many transient errors in the multilogin call.
TEST_F(OAuthMultiloginHelperTest,
       OneAccountTransientMultiloginErrorMaxRetries) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
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
  CreateHelper({{kAccountId, kGaiaId}, {kAccountId2, kGaiaId2}});

  // The failed access token should be invalidated.
  EXPECT_CALL(*token_service(),
              MockInvalidateTokenForMultilogin(kAccountId, kAccessToken));

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
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif
  // Configure mock cookie manager: check that the cookie is the expected one.
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

TEST_F(OAuthMultiloginHelperTest, InvalidTokenErrorWithRefreshTokens) {
  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  token_service()->UpdateCredentials(kAccountId2, "refresh_token2");
  CreateHelper({{kAccountId, kGaiaId}, {kAccountId2, kGaiaId2}});

  // The failed refresh token should be invalidated.
  EXPECT_CALL(*token_service(),
              MockInvalidateTokenForMultilogin(kAccountId, "refresh_token"));

  // Multilogin call fails with invalid token for kAccountId.
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->SimulateResponseForPendingRequest(
      multilogin_url(), kMultiloginInvalidTokenResponse);

  // kAccountId is retried with an access token which is supposed to fail
  // because the refresh token was revoked.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(token_service()->RefreshTokenHasError(kAccountId));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kPersistentError, result_);
}

// Retry on "invalid token" in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, InvalidTokenErrorMaxRetries) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  token_service()->UpdateCredentials(kAccountId2, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}, {kAccountId2, kGaiaId2}});

  // The failed access token should be invalidated.
  EXPECT_CALL(*token_service(),
              MockInvalidateTokenForMultilogin(kAccountId, kAccessToken))
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

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
TEST_F(OAuthMultiloginHelperTest, BoundTokenSuccessNoChallenge) {
  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  // Multilogin call.
  EXPECT_FALSE(callback_called_);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiOAuthAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(
                    kGaiaId, "refresh_token", "DBSC_CHALLENGE_IF_REQUIRED"),
            }));
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

TEST_F(OAuthMultiloginHelperTest, BoundTokenSuccessWithChallenge) {
  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  CreateHelper({{kAccountId, kGaiaId}});

  // First Multilogin call returns a token binding challenge.
  EXPECT_FALSE(callback_called_);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiOAuthAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(
                    kGaiaId, "refresh_token", "DBSC_CHALLENGE_IF_REQUIRED"),
            }));
  url_loader()->SimulateResponseForPendingRequest(
      multilogin_url(),
      base::StringPrintf(
          kMultiloginRetryWithTokenBindingAssertionResponseFormat,
          kGaiaId.ToString(), "test_challenge"),
      net::HTTP_BAD_REQUEST);

  // The second Multilogin request should be issued shortly after this.
  EXPECT_FALSE(callback_called_);
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiOAuthAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(kGaiaId, "refresh_token",
                                                       "test_challenge.signed"),
            }));

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", "SID_value", ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);

  url_loader()->SimulateResponseForPendingRequest(multilogin_url(),
                                                  kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

TEST_F(OAuthMultiloginHelperTest,
       BoundTokenSuccessWithChallengeAndEncryptedCookies) {
  // Do not use char[] because `base::as_byte_span()` will include '\0' in the
  // encrypted string.
  static constexpr std::string_view kCookieValue = "SID_value";
  HybridEncryptionKey ephemeral_key = CreateHybridEncryptionKeyForTesting();
  std::string base64_encrypted_cookie =
      EncryptValueWithEphemeralKey(ephemeral_key, kCookieValue);

  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  const std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  base::RunLoop wait_for_request_loop;
  url_loader()->SetInterceptor(
      base::IgnoreArgs<const network::ResourceRequest&>(
          wait_for_request_loop.QuitClosure()));
  OAuthMultiloginHelper* helper = CreateHelper({{kAccountId, kGaiaId}});
  // Ephemeral key must be set after the first request is sent. Otherwise, the
  // ephemeral key would be consumed by the first request.
  wait_for_request_loop.Run();
  helper->SetEphemeralKeyForTesting(std::move(ephemeral_key));

  // First Multilogin call returns a token binding challenge.
  url_loader()->SimulateResponseForPendingRequest(
      multilogin_url(),
      base::StringPrintf(
          kMultiloginRetryWithTokenBindingAssertionResponseFormat,
          kGaiaId.ToString(), "test_challenge"),
      net::HTTP_BAD_REQUEST);

  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(*cookie_manager(),
              SetCanonicalCookie(
                  CookieMatcher("SID", kCookieValue, ".google.fr"),
                  CookieSourceMatcher("google.fr"), testing::_, testing::_))
      .WillOnce(::testing::Invoke(RunSetCookieCallbackWithSuccess));

  std::string response =
      base::StringPrintf(kMultiloginSuccessWithEncryptedCookieResponseFormat,
                         base64_encrypted_cookie);
  url_loader()->SimulateResponseForPendingRequest(multilogin_url(), response);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}

TEST_F(OAuthMultiloginHelperTest, BoundTokenFailureChallengedTwice) {
  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  std::vector<uint8_t> kFakeWrappedBindingKey = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      kFakeWrappedBindingKey);
  CreateHelper({{kAccountId, kGaiaId}});

  // First Multilogin call returns a token binding challenge.
  EXPECT_FALSE(callback_called_);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiOAuthAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(
                    kGaiaId, "refresh_token", "DBSC_CHALLENGE_IF_REQUIRED"),
            }));
  url_loader()->SimulateResponseForPendingRequest(
      multilogin_url(),
      base::StringPrintf(
          kMultiloginRetryWithTokenBindingAssertionResponseFormat,
          kGaiaId.ToString(), "test_challenge"),
      net::HTTP_BAD_REQUEST);

  // The second Multilogin request should be issued shortly after this. The
  // refresh token should be invalidated after receiving the second challenge
  // for the same account.
  EXPECT_FALSE(callback_called_);
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiOAuthAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(kGaiaId, "refresh_token",
                                                       "test_challenge.signed"),
            }));
  EXPECT_CALL(*token_service(),
              MockInvalidateTokenForMultilogin(kAccountId, "refresh_token"));
  url_loader()->SimulateResponseForPendingRequest(
      multilogin_url(),
      base::StringPrintf(
          kMultiloginRetryWithTokenBindingAssertionResponseFormat,
          kGaiaId.ToString(), "other_test_challenge"),
      net::HTTP_BAD_REQUEST);

  // Helper will try to fallback to an access token, which should fail because
  // the refresh token was invalidated.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_TRUE(token_service()->RefreshTokenHasError(kAccountId));
  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(SetAccountsInCookieResult::kPersistentError, result_);
}

TEST_F(OAuthMultiloginHelperTest, BoundSessionHelperCalled) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  {
    testing::InSequence seq;

    EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies(testing::_));
    EXPECT_CALL(*cookie_manager(),
                SetCanonicalCookie(
                    CookieMatcher("SID", "SID_value", ".google.fr"),
                    CookieSourceMatcher("google.fr"), testing::_, testing::_));
    EXPECT_CALL(*cookie_manager(),
                SetCanonicalCookie(
                    CookieMatcher("FOO", "FOO_value", ".google.com"),
                    CookieSourceMatcher("google.com"), testing::_, testing::_));
    EXPECT_CALL(*bound_session_delegate(), OnCookiesSet());
  }

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(),
                            kMultiloginSuccessResponseTwoCookies);
  // All set cookie calls must be sent before adding any mock expectation,
  // otherwise the test will fail.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
}
#endif
}  // namespace signin

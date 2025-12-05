// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"
#include "components/signin/public/base/session_binding_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key_test_utils.h"
#include "services/network/test/mock_device_bound_session_manager.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace signin {

namespace {

using ::testing::_;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
using ::base::test::RunOnceCallback;
using ::net::device_bound_sessions::SessionParams;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using DeviceBoundSessionCreateSessionsResult =
    OAuthMultiloginHelper::DeviceBoundSessionCreateSessionsResult;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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

const char kMultiloginSuccessResponseNoCookies[] =
    R"()]}'
       {
         "status": "OK",
         "cookies":[]
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
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

const char
    kMultiloginSuccessWithStandardDeviceBoundSessionCredentialsResponse[] =
        R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": false,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true,
              "register_session_payload": {
                "session_identifier": "id",
                "refresh_url": "/RotateBoundCookies",
                "scope": {
                  "origin": "https://google.com",
                  "include_site": true,
                  "scope_specification" : [
                    {
                      "type": "include",
                      "domain": ".google.com",
                      "path": "/"
                    }
                  ]
                },
                "credentials": [{
                  "type": "cookie",
                  "name": "__Secure-1PSIDTS",
                  "scope": {
                    "domain": ".google.com",
                    "path": "/"
                  },
                  "attributes": "Domain=.google.com; Path=/; Secure"
                }],
                "allowed_refresh_initiators": ["https://google.com"]
              }
            }
          ]
        }
      )";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// GMock matcher that checks that the cookie has the expected parameters.
MATCHER_P3(CookieMatcher, name, value, domain, "") {
  return arg.Name() == name && arg.Value() == value && arg.Domain() == domain &&
         arg.Path() == "/" && arg.SecureAttribute() && !arg.IsHttpOnly();
}

// Checks that the argument (a GURL) is secure and has the given hostname.
MATCHER_P(CookieSourceMatcher, cookie_host, "") {
  return arg.is_valid() && arg.GetScheme() == "https" &&
         arg.GetHost() == cookie_host;
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
struct MultiloginCookieBindingTestParam {
  std::vector<base::test::FeatureRefAndParams> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  bool should_return_bound_session_delegate = false;
  bool should_return_device_bound_session_manager = false;
  std::string expected_url_param;
  std::string test_suffix;
};

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
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}  // namespace

class OAuthMultiloginHelperTest
    : public testing::Test,
      public AccountsCookieMutator::PartitionDelegate {
 public:
  explicit OAuthMultiloginHelperTest(
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      const std::vector<base::test::FeatureRefAndParams>& enabled_features =
          {{switches::kEnableOAuthMultiloginCookiesBinding, {}}},
      const std::vector<base::test::FeatureRef>& disabled_features =
          {switches::kEnableOAuthMultiloginStandardCookiesBinding}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
      )
      : kAccountId(CoreAccountId::FromGaiaId(kGaiaId)),
        kAccountId2(CoreAccountId::FromGaiaId(kGaiaId2)),
        test_signin_client_(&pref_service_),
        mock_token_service_(
            std::make_unique<MockTokenService>(&pref_service_)) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  }

  ~OAuthMultiloginHelperTest() override = default;

  OAuthMultiloginHelper* CreateHelper(
      const std::vector<OAuthMultiloginHelper::AccountIdGaiaIdPair> accounts,
      bool set_external_cc_result = false,
      bool wait_on_connectivity = true) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    // `bound_session_delegate_` is owned by `OAuthMultiloginHelper`, ensures it
    // resets before creating a new helper to avoid dangling pointers.
    bound_session_delegate_ = nullptr;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
    helper_ = std::make_unique<OAuthMultiloginHelper>(
        &test_signin_client_, this, token_service(),
        gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
        wait_on_connectivity, accounts,
        set_external_cc_result ? kExternalCcResult : std::string(),
        gaia::GaiaSource::kChrome,
        base::BindOnce(&OAuthMultiloginHelperTest::OnOAuthMultiloginFinished,
                       base::Unretained(this)));
    return helper_.get();
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  MockBoundSessionOAuthMultiLoginDelegate* bound_session_delegate() {
    return bound_session_delegate_;
  }

  void SetShouldReturnBoundSessionDelegate(bool value) {
    should_return_bound_session_delegate_ = value;
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

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
           base::EscapeQueryParamValue(kExternalCcResult, true);
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
    CHECK(!result_.has_value());
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
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  network::MockDeviceBoundSessionManager& mock_device_bound_session_manager() {
    return mock_device_bound_session_manager_;
  }

  network::mojom::DeviceBoundSessionManager*
  GetDeviceBoundSessionManagerForPartition() override {
    if (should_return_device_bound_session_manager_) {
      return &mock_device_bound_session_manager_;
    }
    return nullptr;
  }

  void SetShouldReturnDeviceBoundSessionManager(bool value) {
    should_return_device_bound_session_manager_ = value;
  }

  std::unique_ptr<BoundSessionOAuthMultiLoginDelegate>
  CreateBoundSessionOAuthMultiLoginDelegateForPartition() override {
    if (should_return_bound_session_delegate_) {
      auto delegate =
          std::make_unique<MockBoundSessionOAuthMultiLoginDelegate>();
      bound_session_delegate_ = delegate.get();
      return delegate;
    }
    return nullptr;
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  const CoreAccountId kAccountId;
  const CoreAccountId kAccountId2;
  base::test::TaskEnvironment task_environment_;

  std::optional<SetAccountsInCookieResult> result_;

  TestingPrefServiceSimple pref_service_;
  MockCookieManager mock_cookie_manager_;
  TestSigninClient test_signin_client_;
  std::unique_ptr<MockTokenService> mock_token_service_;
  std::unique_ptr<OAuthMultiloginHelper> helper_;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  bool should_return_device_bound_session_manager_ = true;
  network::MockDeviceBoundSessionManager mock_device_bound_session_manager_;
  bool should_return_bound_session_delegate_ = true;
  raw_ptr<MockBoundSessionOAuthMultiLoginDelegate> bound_session_delegate_ =
      nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
};

// Everything succeeds.
TEST_F(OAuthMultiloginHelperTest, Success) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiBearerAuthorizationHeader(
                {gaia::MultiloginAccountAuthCredentials(kGaiaId, kAccessToken,
                                                        kNoAssertion)}));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

// Same as Success, but simulates making a request while offline and
// wait_on_connectivity=false, which allows sending the request anyway.
TEST_F(OAuthMultiloginHelperTest, SuccessOffline) {
  test_signin_client_.SetNetworkCallsDelayed(true);
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}}, false, /*wait_on_connectivity=*/false);

  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiBearerAuthorizationHeader(
                {gaia::MultiloginAccountAuthCredentials(kGaiaId, kAccessToken,
                                                        kNoAssertion)}));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

// Success, but the response does not contain any cookies.
TEST_F(OAuthMultiloginHelperTest, SuccessWithNoCookies) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url()));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url(),
                            kMultiloginSuccessResponseNoCookies);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

TEST_F(OAuthMultiloginHelperTest, SuccessWithRefreshToken) {
  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiBearerAuthorizationHeader(
                {gaia::MultiloginAccountAuthCredentials(
                    kGaiaId, "refresh_token", kNoAssertion)}));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
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
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  // Issue access tokens.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);
  OAuth2AccessTokenConsumer::TokenResponse success_response_2;
  success_response_2.access_token = kAccessToken2;
  token_service()->IssueAllTokensForAccount(kAccountId2, success_response_2);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
  const network::ResourceRequest* multilogin_request = nullptr;
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiBearerAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(kGaiaId2, kAccessToken2,
                                                       kNoAssertion),
                gaia::MultiloginAccountAuthCredentials(kGaiaId, kAccessToken,
                                                       kNoAssertion),
            }));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

// Multiple cookies in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, MultipleCookies) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("FOO", "FOO_value", ".google.com"),
                         CookieSourceMatcher("google.com"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url(),
                            kMultiloginSuccessResponseTwoCookies);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

// Multiple cookies in the multilogin response.
TEST_F(OAuthMultiloginHelperTest, SuccessWithExternalCcResult) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}},
               /*set_external_cc_result=*/true);

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".youtube.com"),
                         CookieSourceMatcher("youtube.com"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("FOO", "FOO_value", ".google.com"),
                         CookieSourceMatcher("google.com"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
  EXPECT_TRUE(
      url_loader()->IsPending(multilogin_url_with_external_cc_result()));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url_with_external_cc_result(),
                            kMultiloginSuccessResponseWithSecondaryDomain);
  EXPECT_FALSE(
      url_loader()->IsPending(multilogin_url_with_external_cc_result()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class OAuthMultiloginHelperCookieBindingTest
    : public OAuthMultiloginHelperTest,
      public testing::WithParamInterface<MultiloginCookieBindingTestParam> {
 public:
  OAuthMultiloginHelperCookieBindingTest()
      : OAuthMultiloginHelperTest(GetParam().enabled_features,
                                  GetParam().disabled_features) {
    SetShouldReturnBoundSessionDelegate(
        GetParam().should_return_bound_session_delegate);
    SetShouldReturnDeviceBoundSessionManager(
        GetParam().should_return_device_bound_session_manager);
  }
};

TEST_P(OAuthMultiloginHelperCookieBindingTest, RequestUrlParameter) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  EXPECT_TRUE(
      url_loader()->IsPending(multilogin_url() + GetParam().expected_url_param,
                              /*request_out=*/nullptr));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    OAuthMultiloginHelperCookieBindingTest,
    testing::Values(
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginCookiesBinding, {}},
             {switches::kEnableOAuthMultiloginCookiesBindingServerExperiment,
              {{"enforced", "false"}}}},
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginStandardCookiesBinding},
            /*should_return_bound_session_delegate=*/true,
            /*should_return_device_bound_session_manager=*/false,
            /*expected_url_param=*/"&cookie_binding=1",
            /*test_suffix=*/"Unenforced"},
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginCookiesBinding, {}},
             {switches::kEnableOAuthMultiloginCookiesBindingServerExperiment,
              {{"enforced", "true"}}}},
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginStandardCookiesBinding},
            /*should_return_bound_session_delegate=*/true,
            /*should_return_device_bound_session_manager=*/false,
            /*expected_url_param=*/"&cookie_binding=2",
            /*test_suffix=*/"Enforced"},
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginCookiesBinding, {}},
             {switches::kEnableOAuthMultiloginCookiesBindingServerExperiment,
              {}}},
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginStandardCookiesBinding},
            /*should_return_bound_session_delegate=*/true,
            /*should_return_device_bound_session_manager=*/false,
            /*expected_url_param=*/"&cookie_binding=2",
            /*test_suffix=*/"Default"},
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {
                {switches::kEnableOAuthMultiloginCookiesBinding, {}},
            },
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginCookiesBindingServerExperiment,
             switches::kEnableOAuthMultiloginStandardCookiesBinding},
            /*should_return_bound_session_delegate=*/true,
            /*should_return_device_bound_session_manager=*/false,
            /*expected_url_param=*/"",
            /*test_suffix=*/"Disabled"},
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginCookiesBinding, {}},
             {switches::kEnableOAuthMultiloginCookiesBindingServerExperiment,
              {{"enforced", "false"}}}},
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginStandardCookiesBinding},
            /*should_return_bound_session_delegate=*/false,
            /*should_return_device_bound_session_manager=*/false,
            /*expected_url_param=*/"",
            /*test_suffix=*/"UnenforcedButDisabledForPartition"},
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginCookiesBinding, {}},
             {switches::kEnableOAuthMultiloginCookiesBindingServerExperiment,
              {{"enforced", "true"}}}},
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginStandardCookiesBinding},
            /*should_return_bound_session_delegate=*/false,
            /*should_return_device_bound_session_manager=*/false,
            /*expected_url_param=*/"",
            /*test_suffix=*/"EnforcedButDisabledForPartition"},
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginStandardCookiesBinding, {}}},
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginCookiesBinding,
             switches::kEnableOAuthMultiloginCookiesBindingServerExperiment},
            /*should_return_bound_session_delegate=*/false,
            /*should_return_device_bound_session_manager=*/true,
            /*expected_url_param=*/"&cookie_binding=2",
            /*test_suffix=*/"StandardEnabled"},
        MultiloginCookieBindingTestParam{
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginStandardCookiesBinding, {}}},
            /*disabled_features=*/
            {switches::kEnableOAuthMultiloginCookiesBinding,
             switches::kEnableOAuthMultiloginCookiesBindingServerExperiment},
            /*should_return_bound_session_delegate=*/false,
            /*should_return_device_bound_session_manager=*/false,
            /*expected_url_param=*/"",
            /*test_suffix=*/"StandardEnabledButDisabledForPartition"}),
    [](const testing::TestParamInfo<MultiloginCookieBindingTestParam>& info) {
      return info.param.test_suffix;
    });
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Failure to get the access token.
TEST_F(OAuthMultiloginHelperTest, OneAccountAccessTokenFailure) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  token_service()->IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kPersistentError);
}

// Retry on transient errors in the multilogin call.
TEST_F(OAuthMultiloginHelperTest, OneAccountTransientMultiloginError) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

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
  EXPECT_EQ(result_, std::nullopt);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
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
    EXPECT_EQ(result_, std::nullopt);
    url_loader()->SimulateResponseForPendingRequest(multilogin_url(),
                                                    kMultiloginRetryResponse);
  }

  // Failure after exceeding the maximum number of retries.
  EXPECT_EQ(result_, SetAccountsInCookieResult::kTransientError);
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
  EXPECT_EQ(result_, std::nullopt);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(), "blah");  // Unexpected response.
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kPersistentError);
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
  EXPECT_EQ(result_, std::nullopt);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  token_service()->IssueAllTokensForAccount(kAccountId2, success_response);

  // Multilogin succeeds the second time.
  EXPECT_EQ(result_, std::nullopt);
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Configure mock cookie manager: check that the cookie is the expected one.
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
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
  EXPECT_NE(token_service()->GetAuthError(kAccountId),
            GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(result_, SetAccountsInCookieResult::kPersistentError);
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

    EXPECT_EQ(result_, std::nullopt);
    EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));

    url_loader()->SimulateResponseForPendingRequest(
        multilogin_url(), kMultiloginInvalidTokenResponse);
  }

  // The maximum number of retries is reached, fail.
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kTransientError);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
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
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  // Multilogin call.
  EXPECT_EQ(result_, std::nullopt);
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
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

TEST_F(OAuthMultiloginHelperTest, BoundTokenSuccessNoBoundSessionDelegate) {
  SetShouldReturnBoundSessionDelegate(false);
  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown,
      /*wrapped_binding_key=*/{1, 2, 3});
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // No bound session delegate is created (no mock is created either).
  MockBoundSessionOAuthMultiLoginDelegate* mock_bound_session_delegate =
      bound_session_delegate();
  ASSERT_EQ(mock_bound_session_delegate, nullptr);
  // Make sure the cookies are still set despite the missing bound session
  // delegate.
  MockCookieManager* mock_cookie_manager = cookie_manager();
  ASSERT_NE(mock_cookie_manager, nullptr);
  EXPECT_CALL(*mock_cookie_manager, SetCanonicalCookie)
      .WillOnce(RunSetCookieCallbackWithSuccess);

  ASSERT_TRUE(url_loader()->IsPending(multilogin_url()));

  url_loader()->AddResponse(multilogin_url(), kMultiloginSuccessResponse);

  ASSERT_FALSE(url_loader()->IsPending(multilogin_url()));
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
  EXPECT_EQ(result_, std::nullopt);
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
  EXPECT_EQ(result_, std::nullopt);
  ASSERT_TRUE(url_loader()->IsPending(multilogin_url(), &multilogin_request));
  EXPECT_EQ(multilogin_request->headers.GetHeader(kAuthorizationHeaderName),
            CreateMultiOAuthAuthorizationHeader({
                gaia::MultiloginAccountAuthCredentials(kGaiaId, "refresh_token",
                                                       "test_challenge.signed"),
            }));

  // Configure mock cookie manager:
  // - check that the cookie is the expected one
  // - immediately invoke the callback
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies);
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet);

  url_loader()->SimulateResponseForPendingRequest(multilogin_url(),
                                                  kMultiloginSuccessResponse);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
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
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", kCookieValue, ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  std::string response =
      base::StringPrintf(kMultiloginSuccessWithEncryptedCookieResponseFormat,
                         base64_encrypted_cookie);
  url_loader()->SimulateResponseForPendingRequest(multilogin_url(), response);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
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
  EXPECT_EQ(result_, std::nullopt);
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
  EXPECT_EQ(result_, std::nullopt);
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
  EXPECT_NE(token_service()->GetAuthError(kAccountId),
            GoogleServiceAuthError::AuthErrorNone());
  EXPECT_EQ(result_, SetAccountsInCookieResult::kPersistentError);
}

TEST_F(OAuthMultiloginHelperTest, BoundSessionHelperCalled) {
  token_service()->UpdateCredentials(kAccountId, "refresh_token");
  CreateHelper({{kAccountId, kGaiaId}});

  testing::Sequence s1, s2;
  // `BeforeSetCookies()` must be called the first.
  EXPECT_CALL(*bound_session_delegate(), BeforeSetCookies).InSequence(s1, s2);
  // Cookies can be set in any order.
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("SID", "SID_value", ".google.fr"),
                         CookieSourceMatcher("google.fr"), _, _))
      .InSequence(s1);
  EXPECT_CALL(
      *cookie_manager(),
      SetCanonicalCookie(CookieMatcher("FOO", "FOO_value", ".google.com"),
                         CookieSourceMatcher("google.com"), _, _))
      .InSequence(s2);
  // `OnCookiesSet()` must be called the last.
  EXPECT_CALL(*bound_session_delegate(), OnCookiesSet).InSequence(s1, s2);

  // Issue access token.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service()->IssueAllTokensForAccount(kAccountId, success_response);

  // Multilogin call.
  EXPECT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(multilogin_url(),
                            kMultiloginSuccessResponseTwoCookies);
  EXPECT_FALSE(url_loader()->IsPending(multilogin_url()));
  EXPECT_EQ(result_, SetAccountsInCookieResult::kSuccess);
}

class OAuthMultiloginHelperStandardBoundSessionsEnabledTest
    : public OAuthMultiloginHelperTest {
 public:
  OAuthMultiloginHelperStandardBoundSessionsEnabledTest()
      : OAuthMultiloginHelperTest(
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginStandardCookiesBinding, {}},
             {switches::kEnableOAuthMultiloginCookiesBinding, {}}},
            /*disabled_features=*/{}) {}

 protected:
  std::string multilogin_url_with_cookie_enforcement() {
    return multilogin_url() + "&cookie_binding=2";
  }
};

TEST_F(OAuthMultiloginHelperStandardBoundSessionsEnabledTest,
       SetCookiesViaDeviceBoundSessionManager) {
  base::HistogramTester histogram_tester;

  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  const std::vector<uint8_t> binding_key = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown, binding_key);
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // No cookies are set via `CookieManager` if standard DBSC is enabled.
  MockCookieManager* mock_cookie_manager = cookie_manager();
  ASSERT_NE(mock_cookie_manager, nullptr);
  EXPECT_CALL(*mock_cookie_manager, SetCanonicalCookie).Times(0);

  // No sessions are created via `BoundSessionOAuthMultiLoginDelegate` if
  // standard DBSC is enabled.
  MockBoundSessionOAuthMultiLoginDelegate* mock_bound_session_delegate =
      bound_session_delegate();
  ASSERT_NE(mock_bound_session_delegate, nullptr);
  EXPECT_CALL(*mock_bound_session_delegate, BeforeSetCookies).Times(0);
  EXPECT_CALL(*mock_bound_session_delegate, OnCookiesSet).Times(0);

  EXPECT_CALL(
      mock_device_bound_session_manager(),
      CreateBoundSessions(
          UnorderedElementsAre(AllOf(
              Field(&SessionParams::session_id, "id"),
              Field(&SessionParams::fetcher_url,
                    GaiaUrls::GetInstance()->oauth_multilogin_url()),
              Field(&SessionParams::refresh_url, "/RotateBoundCookies"),
              Field(
                  &SessionParams::scope,
                  AllOf(
                      Field(&SessionParams::Scope::origin,
                            "https://google.com"),
                      Field(&SessionParams::Scope::include_site, true),
                      Field(
                          &SessionParams::Scope::specifications,
                          UnorderedElementsAre(AllOf(
                              Field(&SessionParams::Scope::Specification::type,
                                    SessionParams::Scope::Specification::Type::
                                        kInclude),
                              Field(
                                  &SessionParams::Scope::Specification::domain,
                                  ".google.com"),
                              Field(&SessionParams::Scope::Specification::path,
                                    "/")))))),
              Field(&SessionParams::credentials,
                    UnorderedElementsAre(
                        AllOf(Field(&SessionParams::Credential::name,
                                    "__Secure-1PSIDTS"),
                              Field(&SessionParams::Credential::attributes,
                                    "Domain=.google.com; Path=/; Secure")))),
              Field(&SessionParams::allowed_refresh_initiators,
                    UnorderedElementsAre("https://google.com")))),
          binding_key,
          UnorderedElementsAre(CookieMatcher(
              "__Secure-1PSIDTS", "secure-1p-sidts-value", ".google.com")),
          _, _))
      .WillOnce(base::test::RunOnceCallback<4>(
          std::vector<net::device_bound_sessions::SessionError::ErrorType>{
              net::device_bound_sessions::SessionError::ErrorType::kSuccess},
          std::vector<net::CookieInclusionStatus>()));

  ASSERT_TRUE(
      url_loader()->IsPending(multilogin_url_with_cookie_enforcement()));
  url_loader()->AddResponse(
      multilogin_url_with_cookie_enforcement(),
      kMultiloginSuccessWithStandardDeviceBoundSessionCredentialsResponse);
  ASSERT_FALSE(
      url_loader()->IsPending(multilogin_url_with_cookie_enforcement()));

  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
  histogram_tester.ExpectUniqueSample(
      "Signin.DeviceBoundSessions.OAuthMultilogin.CreateSessionsResult",
      OAuthMultiloginHelper::DeviceBoundSessionCreateSessionsResult::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(OAuthMultiloginHelperStandardBoundSessionsEnabledTest,
       FallbackToLegacySetCookiesIfBindingKeyMissing) {
  base::HistogramTester histogram_tester;

  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // No sessions are created via `BoundSessionOAuthMultiLoginDelegate` if
  // standard DBSC is enabled.
  MockBoundSessionOAuthMultiLoginDelegate* mock_bound_session_delegate =
      bound_session_delegate();
  ASSERT_NE(mock_bound_session_delegate, nullptr);
  EXPECT_CALL(*mock_bound_session_delegate, BeforeSetCookies).Times(0);
  EXPECT_CALL(*mock_bound_session_delegate, OnCookiesSet).Times(0);

  // No session are created and no cookies are set via
  // `DeviceBoundSessionManager` if the binding key is missing.
  EXPECT_CALL(mock_device_bound_session_manager(), CreateBoundSessions)
      .Times(0);

  // Falling back to setting cookies via `CookieManager`.
  MockCookieManager* mock_cookie_manager = cookie_manager();
  ASSERT_NE(mock_cookie_manager, nullptr);
  EXPECT_CALL(
      *mock_cookie_manager,
      SetCanonicalCookie(CookieMatcher("__Secure-1PSIDTS",
                                       "secure-1p-sidts-value", ".google.com"),
                         _, _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  ASSERT_TRUE(
      url_loader()->IsPending(multilogin_url_with_cookie_enforcement()));
  url_loader()->AddResponse(
      multilogin_url_with_cookie_enforcement(),
      kMultiloginSuccessWithStandardDeviceBoundSessionCredentialsResponse);
  ASSERT_FALSE(
      url_loader()->IsPending(multilogin_url_with_cookie_enforcement()));

  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
  histogram_tester.ExpectUniqueSample(
      "Signin.DeviceBoundSessions.OAuthMultilogin.CreateSessionsResult",
      OAuthMultiloginHelper::DeviceBoundSessionCreateSessionsResult::
          kFallbackNoBindingKey,
      /*expected_bucket_count=*/1);
}

TEST_F(OAuthMultiloginHelperStandardBoundSessionsEnabledTest,
       FallbackToLegacySetCookiesIfNoSessionsToRegister) {
  base::HistogramTester histogram_tester;

  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  const std::vector<uint8_t> binding_key = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown, binding_key);
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // No sessions are created via `BoundSessionOAuthMultiLoginDelegate` if
  // standard DBSC is enabled.
  MockBoundSessionOAuthMultiLoginDelegate* mock_bound_session_delegate =
      bound_session_delegate();
  ASSERT_NE(mock_bound_session_delegate, nullptr);
  EXPECT_CALL(*mock_bound_session_delegate, BeforeSetCookies).Times(0);
  EXPECT_CALL(*mock_bound_session_delegate, OnCookiesSet).Times(0);

  // No session are created and no cookies are set via
  // `DeviceBoundSessionManager` if there are no sessions to register.
  EXPECT_CALL(mock_device_bound_session_manager(), CreateBoundSessions)
      .Times(0);

  // Falling back to setting cookies via `CookieManager`.
  MockCookieManager* mock_cookie_manager = cookie_manager();
  ASSERT_NE(mock_cookie_manager, nullptr);
  EXPECT_CALL(
      *mock_cookie_manager,
      SetCanonicalCookie(CookieMatcher("__Secure-1PSIDTS",
                                       "secure-1p-sidts-value", ".google.com"),
                         _, _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  ASSERT_TRUE(
      url_loader()->IsPending(multilogin_url_with_cookie_enforcement()));
  const std::string response_data =
      R"()]}'
        {
          "status": "OK",
          "cookies":[
            {
              "name": "__Secure-1PSIDTS",
              "value": "secure-1p-sidts-value",
              "domain": ".google.com",
              "path": "/",
              "isSecure": true,
              "isHttpOnly": false,
              "maxAge": 31536000,
              "priority": "HIGH",
              "sameParty": "1"
            }
          ],
          "device_bound_session_info": [
            {
              "domain": "GOOGLE_COM",
              "is_device_bound": true
            }
          ]
        }
      )";
  url_loader()->AddResponse(multilogin_url_with_cookie_enforcement(),
                            response_data);
  ASSERT_FALSE(
      url_loader()->IsPending(multilogin_url_with_cookie_enforcement()));

  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
  histogram_tester.ExpectUniqueSample(
      "Signin.DeviceBoundSessions.OAuthMultilogin.CreateSessionsResult",
      OAuthMultiloginHelper::DeviceBoundSessionCreateSessionsResult::
          kFallbackNoBoundSessions,
      /*expected_bucket_count=*/1);
}

TEST_F(
    OAuthMultiloginHelperStandardBoundSessionsEnabledTest,
    FallbackToLegacySetCookiesAndPrototypeIfDeviceBoundSessionManagerMissing) {
  SetShouldReturnDeviceBoundSessionManager(false);

  base::HistogramTester histogram_tester;

  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  const std::vector<uint8_t> binding_key = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown, binding_key);
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // No sessions are created and no cookies are set via
  // `DeviceBoundSessionManager` if the device bound session manager is missing
  // (e.g. might be disabled for a given partition).
  EXPECT_CALL(mock_device_bound_session_manager(), CreateBoundSessions)
      .Times(0);

  // Falling back to prototype device bound sessions flow.
  MockBoundSessionOAuthMultiLoginDelegate* mock_bound_session_delegate =
      bound_session_delegate();
  ASSERT_NE(mock_bound_session_delegate, nullptr);
  EXPECT_CALL(*mock_bound_session_delegate, BeforeSetCookies);
  EXPECT_CALL(*mock_bound_session_delegate, OnCookiesSet);

  MockCookieManager* mock_cookie_manager = cookie_manager();
  ASSERT_NE(mock_cookie_manager, nullptr);
  EXPECT_CALL(
      *mock_cookie_manager,
      SetCanonicalCookie(CookieMatcher("__Secure-1PSIDTS",
                                       "secure-1p-sidts-value", ".google.com"),
                         _, _, _))
      .WillOnce(RunSetCookieCallbackWithSuccess);

  ASSERT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(
      multilogin_url(),
      kMultiloginSuccessWithStandardDeviceBoundSessionCredentialsResponse);
  ASSERT_FALSE(url_loader()->IsPending(multilogin_url()));

  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
  histogram_tester.ExpectTotalCount(
      "Signin.DeviceBoundSessions.OAuthMultilogin.CreateSessionsResult",
      /*expected_count=*/0);
}

class OAuthMultiloginHelperStandardBoundSessionsEnabledPrototypeDisabledTest
    : public OAuthMultiloginHelperTest {
 public:
  OAuthMultiloginHelperStandardBoundSessionsEnabledPrototypeDisabledTest()
      : OAuthMultiloginHelperTest(
            /*enabled_features=*/
            {{switches::kEnableOAuthMultiloginStandardCookiesBinding, {}}},
            /*disabled_features=*/{
                switches::kEnableOAuthMultiloginCookiesBinding}) {}
};

TEST_F(OAuthMultiloginHelperStandardBoundSessionsEnabledPrototypeDisabledTest,
       SetCookiesViaDeviceBoundSessionManager) {
  base::HistogramTester histogram_tester;

  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  const std::vector<uint8_t> binding_key = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown, binding_key);
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // No sessions are created via `BoundSessionOAuthMultiLoginDelegate` if the
  // standard flow is enabled.
  MockBoundSessionOAuthMultiLoginDelegate* mock_bound_session_delegate =
      bound_session_delegate();
  ASSERT_NE(mock_bound_session_delegate, nullptr);
  EXPECT_CALL(*mock_bound_session_delegate, BeforeSetCookies).Times(0);
  EXPECT_CALL(*mock_bound_session_delegate, OnCookiesSet).Times(0);

  // No cookies are set via `CookieManager` if standard DBSC is enabled.
  MockCookieManager* mock_cookie_manager = cookie_manager();
  ASSERT_NE(mock_cookie_manager, nullptr);
  EXPECT_CALL(*mock_cookie_manager, SetCanonicalCookie).Times(0);

  EXPECT_CALL(mock_device_bound_session_manager(),
              CreateBoundSessions(SizeIs(1), binding_key, SizeIs(1), _, _))
      .WillOnce(base::test::RunOnceCallback<4>(
          std::vector<net::device_bound_sessions::SessionError::ErrorType>{
              net::device_bound_sessions::SessionError::ErrorType::kSuccess},
          std::vector<net::CookieInclusionStatus>()));

  const std::string url = multilogin_url() + "&cookie_binding=2";
  ASSERT_TRUE(url_loader()->IsPending(url));
  url_loader()->AddResponse(
      url, kMultiloginSuccessWithStandardDeviceBoundSessionCredentialsResponse);
  ASSERT_FALSE(url_loader()->IsPending(url));

  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
  histogram_tester.ExpectUniqueSample(
      "Signin.DeviceBoundSessions.OAuthMultilogin.CreateSessionsResult",
      OAuthMultiloginHelper::DeviceBoundSessionCreateSessionsResult::kSuccess,
      /*expected_bucket_count=*/1);
}

TEST_F(
    OAuthMultiloginHelperStandardBoundSessionsEnabledPrototypeDisabledTest,
    FallbackToLegacySetCookiesButNotPrototypeIfDeviceBoundSessionManagerMissing) {
  SetShouldReturnDeviceBoundSessionManager(false);

  base::HistogramTester histogram_tester;

  ReplaceTokenService(/*use_refresh_tokens_for_multilogin=*/true);
  const std::vector<uint8_t> binding_key = {1, 2, 3};
  token_service()->UpdateCredentials(
      kAccountId, "refresh_token",
      signin_metrics::SourceForRefreshTokenOperation::kUnknown, binding_key);
  CreateHelper(/*accounts=*/{{kAccountId, kGaiaId}});

  // No sessions are created and no cookies are set via
  // `DeviceBoundSessionManager` if the device bound session manager is missing
  // (e.g. might be disabled for a given partition).
  EXPECT_CALL(mock_device_bound_session_manager(), CreateBoundSessions)
      .Times(0);

  // No sessions are created via `BoundSessionOAuthMultiLoginDelegate` if the
  // prototype flow is disabled even if falling back to the legacy flow.
  MockBoundSessionOAuthMultiLoginDelegate* mock_bound_session_delegate =
      bound_session_delegate();
  ASSERT_NE(mock_bound_session_delegate, nullptr);
  EXPECT_CALL(*mock_bound_session_delegate, BeforeSetCookies).Times(0);
  EXPECT_CALL(*mock_bound_session_delegate, OnCookiesSet).Times(0);

  // Falling back to legacy set cookies flow.
  MockCookieManager* mock_cookie_manager = cookie_manager();
  ASSERT_NE(mock_cookie_manager, nullptr);
  EXPECT_CALL(*mock_cookie_manager, SetCanonicalCookie)
      .WillOnce(RunSetCookieCallbackWithSuccess);

  ASSERT_TRUE(url_loader()->IsPending(multilogin_url()));
  url_loader()->AddResponse(
      multilogin_url(),
      kMultiloginSuccessWithStandardDeviceBoundSessionCredentialsResponse);
  ASSERT_FALSE(url_loader()->IsPending(multilogin_url()));

  EXPECT_EQ(SetAccountsInCookieResult::kSuccess, result_);
  histogram_tester.ExpectTotalCount(
      "Signin.DeviceBoundSessions.OAuthMultilogin.CreateSessionsResult",
      /*expected_count=*/0);
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}  // namespace signin

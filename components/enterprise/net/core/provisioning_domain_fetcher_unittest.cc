// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/provisioning_domain_fetcher.h"

#include <optional>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

namespace {

constexpr char kTestUserEmail[] = "user@managed.com";
constexpr char kTestDomain[] = "pvd.example.com";
constexpr char kTestUrl[] = "https://pvd.example.com/.well-known/pvd";
constexpr char kTestOAuthToken[] = "bearer_token_abc";

constexpr char kTestCustomHeaderKey[] = "x-profile-id";
constexpr char kTestCustomHeaderValue[] = "user-profile-42";

constexpr char kValidPvdJson[] = R"({
  "identifier": "pvd.example.com",
  "proxies": [
    {
      "protocol": "https-connect",
      "identity": "proxy1",
      "proxy": "https://proxy1.example.com:443"
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy1"],
      "domains": ["*.example.com"]
    }
  ]
})";

constexpr char kInvalidJson[] = R"({ invalid_json: true })";

class ProvisioningDomainFetcherTest : public testing::Test {
 public:
  ProvisioningDomainFetcherTest() {
    pref_service_.registry()->RegisterStringPref("intl.accept_languages",
                                                 "en-US,en;q=0.9");
  }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  void SetUpManagedPrimaryAccount(const std::string& email = kTestUserEmail) {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        "managed.com", "Full Name", "Given Name", "en-US", "picture_url");
  }

  // Synchronously executes fetch when no HTTP network call is expected (e.g.
  // invalid URL or immediate failure).
  ProvisioningDomainFetchResult FetchSync(
      EnterpriseNetworkAuthService* auth_service,
      const ProvisioningDomainConfig& policy) {
    ProvisioningDomainFetcher fetcher(policy, auth_service,
                                      GetURLLoaderFactory());
    ProvisioningDomainFetchResult result =
        base::unexpected(ProvisioningDomainFetchError(
            ProvisioningDomainFetchResultStatus::kHttpError));
    fetcher.Start(base::BindOnce(
        [](ProvisioningDomainFetchResult* out,
           ProvisioningDomainFetchResult res) { *out = std::move(res); },
        base::Unretained(&result)));
    return result;
  }

  // Initiates fetch without OAuth token requirement and simulates HTTP loader
  // response.
  ProvisioningDomainFetchResult FetchWithoutOAuthWithSimulatedResponse(
      EnterpriseNetworkAuthService* auth_service,
      const ProvisioningDomainConfig& policy,
      const std::string& response_body,
      net::HttpStatusCode status_code = net::HTTP_OK) {
    ProvisioningDomainFetcher fetcher(policy, auth_service,
                                      GetURLLoaderFactory());
    base::RunLoop run_loop;
    ProvisioningDomainFetchResult result =
        base::unexpected(ProvisioningDomainFetchError(
            ProvisioningDomainFetchResultStatus::kHttpError));

    fetcher.Start(base::BindOnce(
        [](base::RunLoop* loop, ProvisioningDomainFetchResult* out,
           ProvisioningDomainFetchResult res) {
          *out = std::move(res);
          loop->Quit();
        },
        base::Unretained(&run_loop), base::Unretained(&result)));

    if (test_url_loader_factory_.NumPending() > 0) {
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          kTestUrl, response_body, status_code);
      run_loop.Run();
    }

    return result;
  }

  // Initiates an async OAuth token fetch and mocks a GAIA error response.
  ProvisioningDomainFetchResult FetchWithOAuthAsyncError(
      EnterpriseNetworkAuthService& auth_service,
      const ProvisioningDomainConfig& policy,
      const GoogleServiceAuthError& gaia_error) {
    ProvisioningDomainFetcher fetcher(policy, &auth_service,
                                      GetURLLoaderFactory());
    base::RunLoop run_loop;
    ProvisioningDomainFetchResult result =
        base::unexpected(ProvisioningDomainFetchError(
            ProvisioningDomainFetchResultStatus::kHttpError));

    fetcher.Start(base::BindOnce(
        [](base::RunLoop* loop, ProvisioningDomainFetchResult* out,
           ProvisioningDomainFetchResult res) {
          *out = std::move(res);
          loop->Quit();
        },
        base::Unretained(&run_loop), base::Unretained(&result)));

    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        gaia_error);

    return result;
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
  enterprise::ProfileIdService profile_id_service_{"test_profile_id"};
};

TEST_F(ProvisioningDomainFetcherTest, FetchWithoutOAuth_Success) {
  base::HistogramTester histogram_tester;
  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  ProvisioningDomainConfig policy;
  policy.pvd_id = kTestDomain;
  policy.extra_headers = {
      ProxyExtraHeader(kTestCustomHeaderKey, kTestCustomHeaderValue)};

  ProvisioningDomainFetcher fetcher(policy, &auth_service,
                                    GetURLLoaderFactory());

  base::RunLoop run_loop;
  ProvisioningDomainFetchResult result =
      base::unexpected(ProvisioningDomainFetchError(
          ProvisioningDomainFetchResultStatus::kHttpError));

  fetcher.Start(base::BindOnce(
      [](base::RunLoop* loop, ProvisioningDomainFetchResult* out,
         ProvisioningDomainFetchResult res) {
        *out = std::move(res);
        loop->Quit();
      },
      base::Unretained(&run_loop), base::Unretained(&result)));

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const network::ResourceRequest& req =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(kTestUrl, req.url.spec());
  EXPECT_FALSE(req.headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
  EXPECT_EQ(kTestCustomHeaderValue,
            req.headers.GetHeader(kTestCustomHeaderKey).value_or(""));

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestUrl,
                                                             kValidPvdJson);
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kTestDomain, result->pvd_id);
  EXPECT_EQ(1u, result->proxy_endpoints.size());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ProvisioningDomain.FetchResult",
      ProvisioningDomainFetchResultStatus::kSuccess, 1);
}

TEST_F(ProvisioningDomainFetcherTest, FetchWithoutOAuth_Failures) {
  base::HistogramTester histogram_tester;
  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);
  ProvisioningDomainConfig policy;
  policy.pvd_id = kTestDomain;

  // 1. Invalid URL failure
  ProvisioningDomainConfig invalid_policy;
  invalid_policy.pvd_id = "invalid:host:name";
  ProvisioningDomainFetchResult res1 = FetchSync(&auth_service, invalid_policy);
  ASSERT_FALSE(res1.has_value());
  EXPECT_EQ(ProvisioningDomainFetchResultStatus::kInvalidUrl,
            res1.error().status);
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());

  // 2. HTTP response error (404)
  ProvisioningDomainFetchResult res2 = FetchWithoutOAuthWithSimulatedResponse(
      &auth_service, policy, "", net::HTTP_NOT_FOUND);
  ASSERT_FALSE(res2.has_value());
  EXPECT_EQ(ProvisioningDomainFetchResultStatus::kHttpError,
            res2.error().status);
  EXPECT_EQ(net::HTTP_NOT_FOUND, res2.error().response_code.value_or(-1));

  // 3. JSON parsing error
  ProvisioningDomainFetchResult res3 = FetchWithoutOAuthWithSimulatedResponse(
      &auth_service, policy, kInvalidJson);
  ASSERT_FALSE(res3.has_value());
  EXPECT_EQ(ProvisioningDomainFetchResultStatus::kParseError,
            res3.error().status);

  histogram_tester.ExpectBucketCount(
      "Enterprise.ProvisioningDomain.FetchResult",
      ProvisioningDomainFetchResultStatus::kInvalidUrl, 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.ProvisioningDomain.FetchResult",
      ProvisioningDomainFetchResultStatus::kHttpError, 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.ProvisioningDomain.FetchResult",
      ProvisioningDomainFetchResultStatus::kParseError, 1);
}

TEST_F(ProvisioningDomainFetcherTest, FetchWithOAuth_Success) {
  SetUpManagedPrimaryAccount();
  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  ProvisioningDomainConfig policy;
  policy.pvd_id = kTestDomain;
  policy.auth_config = ProxyAuthConfig{AuthType::kProfileBearerToken,
                                       AuthScope::kCloudSecureGateway};
  policy.extra_headers = {
      ProxyExtraHeader(kTestCustomHeaderKey, kTestCustomHeaderValue)};

  ProvisioningDomainFetcher fetcher(policy, &auth_service,
                                    GetURLLoaderFactory());

  base::RunLoop run_loop;
  ProvisioningDomainFetchResult result =
      base::unexpected(ProvisioningDomainFetchError(
          ProvisioningDomainFetchResultStatus::kHttpError));

  fetcher.Start(base::BindOnce(
      [](base::RunLoop* loop, ProvisioningDomainFetchResult* out,
         ProvisioningDomainFetchResult res) {
        *out = std::move(res);
        loop->Quit();
      },
      base::Unretained(&run_loop), base::Unretained(&result)));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestOAuthToken, base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  const network::ResourceRequest& req =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ("Bearer bearer_token_abc",
            req.headers.GetHeader(net::HttpRequestHeaders::kAuthorization)
                .value_or(""));
  EXPECT_EQ(kTestCustomHeaderValue,
            req.headers.GetHeader(kTestCustomHeaderKey).value_or(""));

  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestUrl,
                                                             kValidPvdJson);
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kTestDomain, result->pvd_id);
}

TEST_F(ProvisioningDomainFetcherTest, MapsOAuthFetchErrors) {
  ProvisioningDomainConfig policy;
  policy.pvd_id = kTestDomain;
  policy.auth_config = ProxyAuthConfig{AuthType::kProfileBearerToken,
                                       AuthScope::kCloudSecureGateway};

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);
  SetUpManagedPrimaryAccount();

  // GAIA error state mappings
  const struct {
    GoogleServiceAuthError auth_error;
    TokenFetchError expected_token_error;
  } kGaiaErrorCases[] = {
      {GoogleServiceAuthError::CreateAccountNotFound(),
       TokenFetchError::kNoPrimaryAccount},
      {GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
           GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN),
       TokenFetchError::kInvalidCredentials},
      {GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED),
       TokenFetchError::kTransientError},
  };

  for (const auto& test_case : kGaiaErrorCases) {
    ProvisioningDomainFetchResult result =
        FetchWithOAuthAsyncError(auth_service, policy, test_case.auth_error);
    EXPECT_EQ(0, test_url_loader_factory_.NumPending());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(ProvisioningDomainFetchResultStatus::kTokenFetchError,
              result.error().status);
    EXPECT_EQ(test_case.expected_token_error, result.error().token_fetch_error);
  }

  // Shutdown -> TokenFetchError::kCanceled
  {
    EnterpriseNetworkAuthService auth_service_for_cancel(
        identity_test_env_.identity_manager(), &pref_service_,
        &profile_id_service_);
    ProvisioningDomainFetcher fetcher_for_cancel(
        policy, &auth_service_for_cancel, GetURLLoaderFactory());

    ProvisioningDomainFetchResult result =
        base::unexpected(ProvisioningDomainFetchError(
            ProvisioningDomainFetchResultStatus::kHttpError));
    fetcher_for_cancel.Start(base::BindOnce(
        [](ProvisioningDomainFetchResult* out,
           ProvisioningDomainFetchResult res) { *out = std::move(res); },
        base::Unretained(&result)));

    auth_service_for_cancel.Shutdown();

    EXPECT_EQ(0, test_url_loader_factory_.NumPending());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(ProvisioningDomainFetchResultStatus::kTokenFetchError,
              result.error().status);
    EXPECT_EQ(TokenFetchError::kCanceled, result.error().token_fetch_error);
  }
}

TEST_F(ProvisioningDomainFetcherTest, ConcurrentFetchesQueueCallbacks) {
  SetUpManagedPrimaryAccount();
  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  ProvisioningDomainConfig policy;
  policy.pvd_id = kTestDomain;
  policy.auth_config = ProxyAuthConfig{AuthType::kProfileBearerToken,
                                       AuthScope::kCloudSecureGateway};

  ProvisioningDomainFetcher fetcher(policy, &auth_service,
                                    GetURLLoaderFactory());

  ProvisioningDomainFetchResult result1;
  ProvisioningDomainFetchResult result2;

  fetcher.Start(base::BindOnce(
      [](ProvisioningDomainFetchResult* out,
         ProvisioningDomainFetchResult res) { *out = std::move(res); },
      base::Unretained(&result1)));

  // Starting a second fetch while the first is in progress queues callback.
  fetcher.Start(base::BindOnce(
      [](ProvisioningDomainFetchResult* out,
         ProvisioningDomainFetchResult res) { *out = std::move(res); },
      base::Unretained(&result2)));

  // Respond to the OAuth request.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestOAuthToken, base::Time::Max());

  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.SimulateResponseForPendingRequest(kTestUrl,
                                                             kValidPvdJson);

  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(kTestDomain, result1->pvd_id);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(kTestDomain, result2->pvd_id);
}

TEST_F(ProvisioningDomainFetcherTest,
       DelegateGetExtraHeadersResolvesConfigHeaders) {
  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  ProvisioningDomainConfig policy;
  policy.pvd_id = kTestDomain;
  policy.extra_headers = {
      ProxyExtraHeader("X-Constant-Header", "constant_value",
                       ProxyExtraHeader::HeaderType::kConstant),
      ProxyExtraHeader("X-Variable-Profile-Id", "pid-${profile_id}",
                       ProxyExtraHeader::HeaderType::kVariable),
      ProxyExtraHeader("X-Variable-Lang", "${accept_language}",
                       ProxyExtraHeader::HeaderType::kVariable),
  };

  ProvisioningDomainFetcher fetcher(policy, &auth_service,
                                    GetURLLoaderFactory());

  net::HttpRequestHeaders headers = fetcher.GetExtraHeaders();

  EXPECT_EQ("constant_value",
            headers.GetHeader("X-Constant-Header").value_or(""));
  EXPECT_EQ("pid-test_profile_id",
            headers.GetHeader("X-Variable-Profile-Id").value_or(""));
  EXPECT_EQ("en-US,en;q=0.9",
            headers.GetHeader("X-Variable-Lang").value_or(""));
}

}  // namespace
}  // namespace enterprise_net

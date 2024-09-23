// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/user_cloud_signin_restriction_policy_fetcher.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kSecureConnectApiGetSecondaryGoogleAccountUsageUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction?policy_name="
    "SecondaryGoogleAccountUsage";
const char kSecureConnectApiGetSecondaryAccountAllowedInArcPolicyUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction?policy_name="
    "SecondaryAccountAllowedInArcPolicy";
const char kFakeAccessToken[] = "fake-access-token";
const char kFakeRefreshToken[] = "fake-refresh-token";
const char kFakeEnterpriseAccount[] = "alice@acme.com";
const char kFakeEnterpriseDomain[] = "acme.com";
const char kFakeGmailAccount[] = "example@gmail.com";
const char kFakeNonEnterpriseAccount[] = "alice@consumeremail.com";
const char kBadResponseBody[] = "bad-response-body";
static const char kUserInfoResponse[] =
    "{"
    "  \"hd\": \"acme.com\""
    "}";

// A mock access token fetcher. Calls the appropriate error or success callback,
// depending on the `error` provided in the constructor.
class MockAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  MockAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                         const GoogleServiceAuthError& error)
      : OAuth2AccessTokenFetcher(consumer), error_(error) {}

  MockAccessTokenFetcher(const MockAccessTokenFetcher&) = delete;
  MockAccessTokenFetcher& operator=(const MockAccessTokenFetcher&) = delete;

  ~MockAccessTokenFetcher() override = default;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override {
    if (error_ != GoogleServiceAuthError::AuthErrorNone()) {
      FireOnGetTokenFailure(error_);
      return;
    }

    // Send a success response.
    OAuth2AccessTokenConsumer::TokenResponse::Builder builder;
    builder.WithAccessToken(kFakeAccessToken);
    builder.WithRefreshToken(kFakeRefreshToken);
    builder.WithExpirationTime(base::Time::Now() + base::Hours(1));
    builder.WithIdToken("id_token");
    FireOnGetTokenSuccess(builder.build());
  }

  void CancelRequest() override {}

 private:
  const GoogleServiceAuthError error_;
};

}  // namespace

class UserCloudSigninRestrictionPolicyFetcherTest : public ::testing::Test {
 public:
  UserCloudSigninRestrictionPolicyFetcherTest() = default;

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &url_loader_factory_);
  }

 protected:
  // Get policy value for SecondaryGoogleAccountUsage.
  void GetSecondaryGoogleAccountUsageBlocking(
      UserCloudSigninRestrictionPolicyFetcher* restriction_fetcher,
      std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher) {
    base::test::TestFuture<UserCloudSigninRestrictionPolicyFetcher::Status,
                           std::optional<std::string>, const std::string&>
        future;
    restriction_fetcher->GetSecondaryGoogleAccountUsage(
        std::move(access_token_fetcher), future.GetCallback());
    this->status_ = future.Get<0>();
    this->policy_result_ = future.Get<1>();
    this->hosted_domain_ = future.Get<2>();
  }

  // Get policy value for SecondaryAccountAllowedInArcPolicy.
  void GetSecondaryAccountAllowedInArcPolicyBlocking(
      UserCloudSigninRestrictionPolicyFetcher* restriction_fetcher,
      std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher) {
    base::test::TestFuture<UserCloudSigninRestrictionPolicyFetcher::Status,
                           std::optional<bool>>
        future;
    restriction_fetcher->GetSecondaryAccountAllowedInArcPolicy(
        std::move(access_token_fetcher), future.GetCallback());
    this->status_ = future.Get<0>();
    this->policy_value_ = future.Get<1>();
  }

  const std::string& oauth_user_info_url() const {
    return GaiaUrls::GetInstance()->oauth_user_info_url().spec();
  }

  // Check base/test/task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  UserCloudSigninRestrictionPolicyFetcher::Status status_ =
      UserCloudSigninRestrictionPolicyFetcher::Status::kUnknownError;
  std::optional<std::string> policy_result_;
  std::optional<bool> policy_value_;
  std::string hosted_domain_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingPolicyValueSucceedsForSecondaryGoogleAccountUsage) {
  // Set API response.
  base::Value::Dict expected_response;
  expected_response.Set("policyValue", "primary_account_signin");
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, std::move(response));
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);
  base::HistogramTester tester;

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_TRUE(policy_result_);
  EXPECT_EQ(policy_result_.value(), "primary_account_signin");
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcher::Status::kSuccess);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
  tester.ExpectTotalCount(
      "Enterprise.SecondaryGoogleAccountUsage.PolicyFetch.ResponseLatency", 1);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingPolicyValueSucceedsForSecondaryAccountAllowedInArcPolicy) {
  // Set API response.
  base::Value::Dict expected_response;
  expected_response.Set("policyValue", "true");
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryAccountAllowedInArcPolicyUrl,
      std::move(response));
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);
  base::HistogramTester tester;

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  GetSecondaryAccountAllowedInArcPolicyBlocking(
      &restriction_fetcher, std::move(access_token_fetcher));

  EXPECT_TRUE(policy_value_);
  EXPECT_TRUE(policy_value_.value());
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcher::Status::kSuccess);
  tester.ExpectTotalCount("Arc.Policy.SecondaryAccountAllowedInArc.TimeDelta",
                          1);
  tester.ExpectUniqueSample(
      "Arc.Policy.SecondaryAccountAllowedInArc.Status",
      UserCloudSigninRestrictionPolicyFetcher::Status::kSuccess, 1);
  tester.ExpectBucketCount("Arc.Policy.SecondaryAccountAllowedInArc.Value", 1,
                           1);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingArcPolicyValueFailsForNetworkErrors) {
  // Fake network error.
  url_loader_factory_.AddResponse(
      GURL(kSecureConnectApiGetSecondaryAccountAllowedInArcPolicyUrl),
      /*head=*/network::mojom::URLResponseHead::New(),
      /*content=*/std::string(),
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);
  base::HistogramTester tester;

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryAccountAllowedInArcPolicyBlocking(
      &restriction_fetcher, std::move(access_token_fetcher));

  EXPECT_FALSE(policy_value_);
  EXPECT_EQ(status_,
            UserCloudSigninRestrictionPolicyFetcher::Status::kNetworkError);
  tester.ExpectUniqueSample(
      "Arc.Policy.SecondaryAccountAllowedInArc.Status",
      UserCloudSigninRestrictionPolicyFetcher::Status::kNetworkError, 1);
  tester.ExpectBucketCount("Arc.Policy.SecondaryAccountAllowedInArc.Value", 0,
                           0);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingUserInfoFailsForNetworkConnectionErrors) {
  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Fake a failed UserInfo fetch.
  url_loader_factory_.AddResponse(oauth_user_info_url(), std::string(),
                                  net::HTTP_INTERNAL_SERVER_ERROR);

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_,
            UserCloudSigninRestrictionPolicyFetcher::Status::kGetUserInfoError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingAccessTokenFailsForNetworkConnectionErrors) {
  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create an access token fetcher that simulates a network connection error.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
          &restriction_fetcher,
          GoogleServiceAuthError::FromConnectionError(/*error=*/0));

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_,
            UserCloudSigninRestrictionPolicyFetcher::Status::kGetTokenError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingPolicyValueFailsForNetworkErrors) {
  // Fake network error.
  url_loader_factory_.AddResponse(
      GURL(kSecureConnectApiGetSecondaryGoogleAccountUsageUrl),
      /*head=*/network::mojom::URLResponseHead::New(),
      /*content=*/std::string(),
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_,
            UserCloudSigninRestrictionPolicyFetcher::Status::kNetworkError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingPolicyValueFailsForHTTPErrors) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, std::string(),
      net::HTTP_BAD_GATEWAY);
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_,
            UserCloudSigninRestrictionPolicyFetcher::Status::kHttpError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingPolicyReturnsEmptyPolicyForResponsesNotParsable) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, kBadResponseBody);
  url_loader_factory_.AddResponse(oauth_user_info_url(), kUserInfoResponse);

  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(
      status_,
      UserCloudSigninRestrictionPolicyFetcher::Status::kParsingResponseError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingPolicyReturnsEmptyPolicyForConsumerGmailAccounts) {
  // Create policy fetcher.
  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeGmailAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcher::Status::
                         kUnsupportedAccountTypeError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       FetchingPolicyReturnsEmptyPolicyForNonEnterpriseAccounts) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, kBadResponseBody);
  // Simulate an empty response body for non enterprise accounts.
  url_loader_factory_.AddResponse(oauth_user_info_url(), /*content=*/"{}");

  UserCloudSigninRestrictionPolicyFetcher restriction_fetcher(
      kFakeNonEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      std::make_unique<MockAccessTokenFetcher>(
          /*consumer=*/&restriction_fetcher,
          /*error=*/GoogleServiceAuthError::AuthErrorNone());

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_);
  EXPECT_EQ(status_, UserCloudSigninRestrictionPolicyFetcher::Status::
                         kUnsupportedAccountTypeError);
  EXPECT_EQ(hosted_domain_, std::string());
}

}  // namespace ash

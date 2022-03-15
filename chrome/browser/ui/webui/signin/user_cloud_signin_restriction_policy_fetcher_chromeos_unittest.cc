// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/user_cloud_signin_restriction_policy_fetcher_chromeos.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
const char kSecureConnectApiGetSecondaryGoogleAccountUsageUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction";
const char kFakeAccessToken[] = "fake-access-token";
const char kFakeRefreshToken[] = "fake-refresh-token";
const char kFakeEnterpriseAccount[] = "alice@acme.com";
const char kFakeEnterpriseDomain[] = "acme.com";
const char kFakeGmailAccount[] = "example@gmail.com";
const char kFakeNonEnterpriseAccount[] = "alice@nonenterprise.com";
const char KBadResponseBody[] = "bad-response-body";
}  // namespace

class MockUserCloudSigninRestrictionPolicyFetcherChromeOS
    : public UserCloudSigninRestrictionPolicyFetcherChromeOS {
 public:
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS(
      const std::string& email,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : UserCloudSigninRestrictionPolicyFetcherChromeOS(email,
                                                        url_loader_factory) {
    ON_CALL(*this, FetchAccessToken).WillByDefault([this]() {
      return this->OnGetTokenSuccess(
          OAuth2AccessTokenConsumer::TokenResponse::Builder()
              .WithAccessToken(kFakeAccessToken)
              .build());
    });
    ON_CALL(*this, FetchUserInfo).WillByDefault([this]() {
      return this->OnGetUserInfoSuccess(
          std::move(user_info_response_dictionary_.get()));
    });
  }

  MockUserCloudSigninRestrictionPolicyFetcherChromeOS(
      const MockUserCloudSigninRestrictionPolicyFetcherChromeOS&) = delete;
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS& operator=(
      const MockUserCloudSigninRestrictionPolicyFetcherChromeOS&) = delete;

  // TODO(b/224747082): Remove overrides.
  void OnGetTokenFailure(const GoogleServiceAuthError& error);
  void OnGetUserInfoFailure(const GoogleServiceAuthError& error);

  MOCK_METHOD(void, FetchAccessToken, ());
  MOCK_METHOD(void, FetchUserInfo, ());

  std::unique_ptr<base::DictionaryValue> user_info_response_dictionary_ =
      std::make_unique<base::DictionaryValue>();
};

void MockUserCloudSigninRestrictionPolicyFetcherChromeOS::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  UserCloudSigninRestrictionPolicyFetcherChromeOS::OnGetTokenFailure(error);
}

void MockUserCloudSigninRestrictionPolicyFetcherChromeOS::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  UserCloudSigninRestrictionPolicyFetcherChromeOS::OnGetUserInfoFailure(error);
}

class UserCloudSigninRestrictionPolicyFetcherChromeOSTest
    : public ::testing::Test {
 public:
  UserCloudSigninRestrictionPolicyFetcherChromeOSTest() = default;

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &url_loader_factory_);
  }

  // Set dictionary values that will be used by `OnGetUserInfoResponse` to fake
  // API server response.
  void SetHostedDomain(
      MockUserCloudSigninRestrictionPolicyFetcherChromeOS& restriction_fetcher,
      const std::string& hosted_domain) {
    restriction_fetcher.user_info_response_dictionary_->DictClear();
    restriction_fetcher.user_info_response_dictionary_->SetKey(
        "hd", base::Value(hosted_domain));
  }

 protected:
  // Get policy value for SecondaryGoogleAccountUsage.
  void GetSecondaryGoogleAccountUsageBlocking(
      MockUserCloudSigninRestrictionPolicyFetcherChromeOS* const
          restriction_fetcher,
      std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher) {
    base::RunLoop run_loop;
    restriction_fetcher->GetSecondaryGoogleAccountUsage(
        std::move(access_token_fetcher),
        base::BindLambdaForTesting(
            [this, &run_loop](
                MockUserCloudSigninRestrictionPolicyFetcherChromeOS::Status st,
                absl::optional<std::string> res, const std::string& hd) {
              this->policy_result_ = res;
              this->status_ = st;
              this->hosted_domain_ = hd;
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      MockUserCloudSigninRestrictionPolicyFetcherChromeOS*
          restriction_fetcher) {
    return GaiaAccessTokenFetcher::
        CreateExchangeRefreshTokenForAccessTokenInstance(
            restriction_fetcher, GetSharedURLLoaderFactory(),
            kFakeRefreshToken);
  }

  // Check base/test/task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  MockUserCloudSigninRestrictionPolicyFetcherChromeOS::Status status_ =
      MockUserCloudSigninRestrictionPolicyFetcherChromeOS::Status::
          kUnknownError;
  absl::optional<std::string> policy_result_;
  std::string hosted_domain_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyValueSucceeds) {
  // Set API response.
  base::Value expected_response(base::Value::Type::DICTIONARY);
  expected_response.SetStringKey("policyValue", "primary_account_signin");
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, std::move(response));

  // Create policy fetcher.
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());
  SetHostedDomain(restriction_fetcher, kFakeEnterpriseDomain);

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      CreateAccessTokenFetcher(&restriction_fetcher);

  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_TRUE(policy_result_.has_value());
  EXPECT_EQ(policy_result_.value(), "primary_account_signin");
  EXPECT_EQ(
      status_,
      MockUserCloudSigninRestrictionPolicyFetcherChromeOS::Status::kSuccess);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingUserInfoFailsForNetworkConnectionErrors) {
  // Create policy fetcher.
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      CreateAccessTokenFetcher(&restriction_fetcher);

  // TODO(b/224754860): Use mocked dependency injection and remove this.
  // Fake a failed UserInfo fetch.
  EXPECT_CALL(restriction_fetcher, FetchUserInfo())
      .WillOnce([&restriction_fetcher]() {
        return restriction_fetcher.OnGetUserInfoFailure(
            GoogleServiceAuthError::FromConnectionError(0));
      });

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_.has_value());
  EXPECT_EQ(status_, MockUserCloudSigninRestrictionPolicyFetcherChromeOS::
                         Status::kGetUserInfoError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingAccessTokenFailsForNetworkConnectionErrors) {
  // Create policy fetcher.
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      CreateAccessTokenFetcher(&restriction_fetcher);

  // TODO(b/224754860): Use mocked dependency injection and remove this.
  // Fake a failed AccessToken fetch.
  EXPECT_CALL(restriction_fetcher, FetchAccessToken())
      .WillOnce([&restriction_fetcher]() {
        return restriction_fetcher.OnGetTokenFailure(
            GoogleServiceAuthError::FromConnectionError(0));
      });

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_.has_value());
  EXPECT_EQ(status_, MockUserCloudSigninRestrictionPolicyFetcherChromeOS::
                         Status::kGetTokenError);
  EXPECT_EQ(hosted_domain_, std::string());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
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

  // Create policy fetcher.
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());
  SetHostedDomain(restriction_fetcher, kFakeEnterpriseDomain);

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      CreateAccessTokenFetcher(&restriction_fetcher);

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_.has_value());
  EXPECT_EQ(status_, MockUserCloudSigninRestrictionPolicyFetcherChromeOS::
                         Status::kNetworkError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyValueFailsForHTTPErrors) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, std::string(),
      net::HTTP_BAD_GATEWAY);

  // Create policy fetcher.
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());
  SetHostedDomain(restriction_fetcher, kFakeEnterpriseDomain);

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      CreateAccessTokenFetcher(&restriction_fetcher);

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_.has_value());
  EXPECT_EQ(
      status_,
      MockUserCloudSigninRestrictionPolicyFetcherChromeOS::Status::kHttpError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyReturnsEmptyPolicyForResponsesNotParsable) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, KBadResponseBody);

  // Create policy fetcher.
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeEnterpriseAccount, GetSharedURLLoaderFactory());
  SetHostedDomain(restriction_fetcher, kFakeEnterpriseDomain);

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      CreateAccessTokenFetcher(&restriction_fetcher);

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_.has_value());
  EXPECT_EQ(status_, MockUserCloudSigninRestrictionPolicyFetcherChromeOS::
                         Status::kParsingResponseError);
  EXPECT_EQ(hosted_domain_, kFakeEnterpriseDomain);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherChromeOSTest,
       FetchingPolicyReturnsEmptyPolicyForNonEnterpriseAccounts) {
  url_loader_factory_.AddResponse(
      kSecureConnectApiGetSecondaryGoogleAccountUsageUrl, KBadResponseBody);

  // Create policy fetcher.
  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcher(
      kFakeGmailAccount, GetSharedURLLoaderFactory());

  // Create access token fetcher.
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher =
      CreateAccessTokenFetcher(&restriction_fetcher);

  EXPECT_CALL(restriction_fetcher, FetchUserInfo()).Times(0);

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcher,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_.has_value());
  EXPECT_EQ(status_, MockUserCloudSigninRestrictionPolicyFetcherChromeOS::
                         Status::kUnsupportedAccountTypeError);
  EXPECT_EQ(hosted_domain_, std::string());

  MockUserCloudSigninRestrictionPolicyFetcherChromeOS restriction_fetcherother(
      kFakeNonEnterpriseAccount, GetSharedURLLoaderFactory());
  EXPECT_CALL(restriction_fetcherother, FetchUserInfo()).Times(1);

  // Recreate access token fetcher.
  access_token_fetcher =
      GaiaAccessTokenFetcher::CreateExchangeRefreshTokenForAccessTokenInstance(
          &restriction_fetcherother, GetSharedURLLoaderFactory(),
          kFakeRefreshToken);

  // Try to fetch policy value.
  GetSecondaryGoogleAccountUsageBlocking(&restriction_fetcherother,
                                         std::move(access_token_fetcher));

  EXPECT_FALSE(policy_result_.has_value());
  EXPECT_EQ(status_, MockUserCloudSigninRestrictionPolicyFetcherChromeOS::
                         Status::kUnsupportedAccountTypeError);
  EXPECT_EQ(hosted_domain_, std::string());
}

}  // namespace ash

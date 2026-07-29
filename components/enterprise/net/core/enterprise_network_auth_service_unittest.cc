// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_network_auth_service.h"

#include <optional>
#include <string>

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

namespace {

constexpr char kTestOAuthToken[] = "test_access_token_123";
constexpr char kTestHistogramName[] = "Enterprise.NetworkAuth.TokenFetchError";

class EnterpriseNetworkAuthServiceTest : public testing::Test {
 public:
  EnterpriseNetworkAuthServiceTest() {
    pref_service_.registry()->RegisterStringPref("intl.accept_languages",
                                                 "en-US,en;q=0.9");
  }

 protected:
  void SetUpManagedPrimaryAccount(
      const std::string& email = "user@managed.com") {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
        account_info.account_id, account_info.email, account_info.gaia,
        "managed.com", "Full Name", "Given Name", "en-US", "picture_url");
  }

  // Synchronously fetches an access token for immediate precondition checks
  // (e.g. unsupported scope) where no async network fetch is started.
  AccessTokenResult FetchAccessTokenSyncFailure(
      EnterpriseNetworkAuthService& service,
      AuthScope scope) {
    base::test::TestFuture<AccessTokenResult> future;
    service.FetchAccessToken(scope, future.GetCallback());
    return future.Take();
  }

  // Initiates an async access token request and mocks a successful token
  // response.
  AccessTokenResult FetchAccessTokenAsyncSuccess(
      EnterpriseNetworkAuthService& service,
      AuthScope scope,
      const std::string& token) {
    base::test::TestFuture<AccessTokenResult> future;
    service.FetchAccessToken(scope, future.GetCallback());
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        token, base::Time::Max());
    return future.Take();
  }

  // Initiates an async access token request and mocks a GAIA error response.
  AccessTokenResult FetchAccessTokenAsyncFailure(
      EnterpriseNetworkAuthService& service,
      AuthScope scope,
      const GoogleServiceAuthError& error) {
    base::test::TestFuture<AccessTokenResult> future;
    service.FetchAccessToken(scope, future.GetCallback());
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        error);
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingPrefServiceSimple pref_service_;
  enterprise::ProfileIdService profile_id_service_{"test_profile_id"};
};

TEST_F(EnterpriseNetworkAuthServiceTest, SuccessfulAccessTokenFetch) {
  base::HistogramTester histogram_tester;
  SetUpManagedPrimaryAccount();

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  AccessTokenResult fetched_result = FetchAccessTokenAsyncSuccess(
      auth_service, AuthScope::kCloudSecureGateway, kTestOAuthToken);

  ASSERT_TRUE(fetched_result.has_value());
  EXPECT_EQ(kTestOAuthToken, *fetched_result);
  histogram_tester.ExpectBucketCount(kTestHistogramName, kNoErrorForMetrics, 1);
}

TEST_F(EnterpriseNetworkAuthServiceTest, ConcurrentAccessTokenFetches) {
  SetUpManagedPrimaryAccount();

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  base::MockCallback<EnterpriseNetworkAuthService::AccessTokenCallback>
      callback1;
  base::MockCallback<EnterpriseNetworkAuthService::AccessTokenCallback>
      callback2;

  EXPECT_CALL(callback1, Run(AccessTokenResult(kTestOAuthToken)));
  EXPECT_CALL(callback2, Run(AccessTokenResult(kTestOAuthToken)));

  auth_service.FetchAccessToken(AuthScope::kCloudSecureGateway,
                                callback1.Get());
  auth_service.FetchAccessToken(AuthScope::kCloudSecureGateway,
                                callback2.Get());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestOAuthToken, base::Time::Max());
}

TEST_F(EnterpriseNetworkAuthServiceTest,
       MultipleRequestsWithMixedSuccessAndFailure) {
  SetUpManagedPrimaryAccount();

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  EXPECT_EQ(0u, auth_service.GetPendingTokenFetchCountForTesting());

  // 1st request: Success
  AccessTokenResult result1 = FetchAccessTokenAsyncSuccess(
      auth_service, AuthScope::kCloudSecureGateway, "token_1");
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ("token_1", *result1);
  EXPECT_EQ(0u, auth_service.GetPendingTokenFetchCountForTesting());

  // 2nd request: Failure (Transient network error)
  AccessTokenResult result2 = FetchAccessTokenAsyncFailure(
      auth_service, AuthScope::kCloudSecureGateway,
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(TokenFetchError::kTransientError, result2.error());
  EXPECT_EQ(0u, auth_service.GetPendingTokenFetchCountForTesting());

  // 3rd request: Success
  AccessTokenResult result3 = FetchAccessTokenAsyncSuccess(
      auth_service, AuthScope::kCloudSecureGateway, "token_3");
  ASSERT_TRUE(result3.has_value());
  EXPECT_EQ("token_3", *result3);
  EXPECT_EQ(0u, auth_service.GetPendingTokenFetchCountForTesting());

  // In the end the service should be clean.
  EXPECT_EQ(0u, auth_service.GetPendingTokenFetchCountForTesting());
}

TEST_F(EnterpriseNetworkAuthServiceTest, CancelPendingFetchesOnShutdown) {
  base::HistogramTester histogram_tester;
  SetUpManagedPrimaryAccount();

  auto auth_service = std::make_unique<EnterpriseNetworkAuthService>(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  base::MockCallback<EnterpriseNetworkAuthService::AccessTokenCallback>
      mock_callback;
  EXPECT_CALL(
      mock_callback,
      Run(AccessTokenResult(base::unexpected(TokenFetchError::kCanceled))));

  auth_service->FetchAccessToken(AuthScope::kCloudSecureGateway,
                                 mock_callback.Get());

  auth_service->Shutdown();

  histogram_tester.ExpectBucketCount(kTestHistogramName,
                                     TokenFetchError::kCanceled, 1);
}

TEST_F(EnterpriseNetworkAuthServiceTest, MapsGoogleServiceAuthErrors) {
  SetUpManagedPrimaryAccount();

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  const struct {
    GoogleServiceAuthError auth_error;
    TokenFetchError expected_error;
  } kTestCases[] = {
      {GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED),
       TokenFetchError::kTransientError},
      {GoogleServiceAuthError::FromServiceUnavailable(""),
       TokenFetchError::kTransientError},
      {GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
           GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN),
       TokenFetchError::kInvalidCredentials},
      {GoogleServiceAuthError::CreateAccountNotFound(),
       TokenFetchError::kNoPrimaryAccount},
  };

  for (const auto& test_case : kTestCases) {
    base::HistogramTester histogram_tester;
    AccessTokenResult result = FetchAccessTokenAsyncFailure(
        auth_service, AuthScope::kCloudSecureGateway, test_case.auth_error);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(test_case.expected_error, result.error());
    histogram_tester.ExpectBucketCount(kTestHistogramName,
                                       test_case.expected_error, 1);
  }
}

TEST_F(EnterpriseNetworkAuthServiceTest,
       UnsupportedScopeReturnsUnsupportedScopeError) {
  base::HistogramTester histogram_tester;
  SetUpManagedPrimaryAccount();

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  AccessTokenResult result =
      FetchAccessTokenSyncFailure(auth_service, AuthScope::kNone);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(TokenFetchError::kUnsupportedScope, result.error());
  histogram_tester.ExpectBucketCount(kTestHistogramName,
                                     TokenFetchError::kUnsupportedScope, 1);
}

TEST_F(EnterpriseNetworkAuthServiceTest,
       NoPrimaryAccountReturnsNoPrimaryAccountError) {
  base::HistogramTester histogram_tester;
  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  AccessTokenResult result =
      FetchAccessTokenSyncFailure(auth_service, AuthScope::kCloudSecureGateway);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(TokenFetchError::kNoPrimaryAccount, result.error());
  histogram_tester.ExpectBucketCount(kTestHistogramName,
                                     TokenFetchError::kNoPrimaryAccount, 1);
}

TEST_F(EnterpriseNetworkAuthServiceTest,
       UnmanagedUserReturnsUnmanagedUserError) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  AccessTokenResult result =
      FetchAccessTokenSyncFailure(auth_service, AuthScope::kCloudSecureGateway);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(TokenFetchError::kUnmanagedUser, result.error());
  histogram_tester.ExpectBucketCount(kTestHistogramName,
                                     TokenFetchError::kUnmanagedUser, 1);
}

TEST_F(EnterpriseNetworkAuthServiceTest,
       PersistentErrorStateReturnsInvalidCredentials) {
  base::HistogramTester histogram_tester;
  SetUpManagedPrimaryAccount();
  CoreAccountInfo primary_account =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  AccessTokenResult result =
      FetchAccessTokenSyncFailure(auth_service, AuthScope::kCloudSecureGateway);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(TokenFetchError::kInvalidCredentials, result.error());
  histogram_tester.ExpectBucketCount(kTestHistogramName,
                                     TokenFetchError::kInvalidCredentials, 1);
}

TEST_F(EnterpriseNetworkAuthServiceTest,
       PendingAccountManagedStatusFinderResolvesAsync) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@custom-domain.com", signin::ConsentLevel::kSignin);

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service_,
      &profile_id_service_);

  base::MockCallback<EnterpriseNetworkAuthService::AccessTokenCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(AccessTokenResult(kTestOAuthToken)));

  auth_service.FetchAccessToken(AuthScope::kCloudSecureGateway,
                                mock_callback.Get());

  EXPECT_EQ(1u, auth_service.GetPendingTokenFetchCountForTesting());

  // Simulate extended account info update confirming managed status.
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account_info.account_id, account_info.email, account_info.gaia,
      "custom-domain.com", "Full Name", "Given Name", "en-US", "picture_url");

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestOAuthToken, base::Time::Max());

  histogram_tester.ExpectBucketCount(kTestHistogramName, kNoErrorForMetrics, 1);
}

TEST_F(EnterpriseNetworkAuthServiceTest,
       GetExtraHeadersExpandsProfileIdAndPrefs) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterStringPref("intl.accept_languages",
                                              "en-US,ja");

  enterprise::ProfileIdService profile_id_service("test-profile-guid-999");

  EnterpriseNetworkAuthService auth_service(
      identity_test_env_.identity_manager(), &pref_service,
      &profile_id_service);

  std::vector<ProxyExtraHeader> extra_headers = {
      ProxyExtraHeader("X-Constant-Header", "constant_value",
                       ProxyExtraHeader::HeaderType::kConstant),
      ProxyExtraHeader("X-Constant-Literal-Placeholder", "${profile_id}",
                       ProxyExtraHeader::HeaderType::kConstant),
      ProxyExtraHeader("X-Variable-Profile-Id", "pid-${profile_id}",
                       ProxyExtraHeader::HeaderType::kVariable),
      ProxyExtraHeader("X-Variable-Lang", "${accept_language}",
                       ProxyExtraHeader::HeaderType::kVariable),
      ProxyExtraHeader("X-Variable-Unsupported", "${unsupported_var}",
                       ProxyExtraHeader::HeaderType::kVariable),
  };

  net::HttpRequestHeaders resolved =
      auth_service.ResolveExtraHeaders(extra_headers);

  std::optional<std::string> val1 = resolved.GetHeader("X-Constant-Header");
  ASSERT_TRUE(val1.has_value());
  EXPECT_EQ("constant_value", *val1);

  std::optional<std::string> val2 =
      resolved.GetHeader("X-Constant-Literal-Placeholder");
  ASSERT_TRUE(val2.has_value());
  EXPECT_EQ("${profile_id}", *val2);

  std::optional<std::string> val3 = resolved.GetHeader("X-Variable-Profile-Id");
  ASSERT_TRUE(val3.has_value());
  EXPECT_EQ("pid-test-profile-guid-999", *val3);

  std::optional<std::string> val4 = resolved.GetHeader("X-Variable-Lang");
  ASSERT_TRUE(val4.has_value());
  EXPECT_EQ("en-US,ja", *val4);

  EXPECT_FALSE(resolved.HasHeader("X-Variable-Unsupported"));
}

}  // namespace
}  // namespace enterprise_net

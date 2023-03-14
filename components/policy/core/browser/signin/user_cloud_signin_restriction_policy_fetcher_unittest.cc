// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl[] =
    "https://secureconnect-pa.clients6.google.com/"
    "v1:getManagedAccountsSigninRestriction";
}

namespace policy {

class UserCloudSigninRestrictionPolicyFetcherTest : public ::testing::Test {
 public:
  UserCloudSigninRestrictionPolicyFetcherTest()
      : feature_list_(std::make_unique<base::test::ScopedFeatureList>(
            policy::features::kEnableUserCloudSigninRestrictionPolicyFetcher)),
        policy_fetcher_(
            std::make_unique<UserCloudSigninRestrictionPolicyFetcher>(
                nullptr,
                nullptr)) {}

  UserCloudSigninRestrictionPolicyFetcher* policy_fetcher() {
    return policy_fetcher_.get();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  void ResetPolicyFetcher() { policy_fetcher_.reset(); }

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<UserCloudSigninRestrictionPolicyFetcher> policy_fetcher_;
  signin::IdentityTestEnvironment identity_test_env_;
};

// Regression test for https://crbug.com/1337676
TEST_F(UserCloudSigninRestrictionPolicyFetcherTest, NoUseAfterFreeCrash) {
  feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  feature_list_->InitAndDisableFeature(
      policy::features::kEnableUserCloudSigninRestrictionPolicyFetcher);
  network::TestURLLoaderFactory url_loader_factory;
  base::Value::Dict expected_response;
  expected_response.Set("policyValue", "primary_account");
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting([](const std::string&) { NOTREACHED(); }));
  ResetPolicyFetcher();
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest, ReturnsValueFromBody) {
  network::TestURLLoaderFactory url_loader_factory;
  base::Value::Dict expected_response;
  expected_response.Set("policyValue", "primary_account");
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("primary_account", result);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueIfNetworkError) {
  network::TestURLLoaderFactory url_loader_factory;
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  url_loader_factory.AddResponse(
      GURL(kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl),
      /*head=*/std::move(head), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::string(), result);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueIfHTTPError) {
  network::TestURLLoaderFactory url_loader_factory;
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl, std::string(),
      net::HTTP_BAD_GATEWAY);

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::string(), result);
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueInResponseNotParsable) {
  network::TestURLLoaderFactory url_loader_factory;
  url_loader_factory.AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl, "bad");

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  std::string result;
  policy_fetcher()->SetURLLoaderFactoryForTesting(&url_loader_factory);
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&result](const std::string& res) { result = res; }));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::string(), result);
}

}  // namespace policy

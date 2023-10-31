// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
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
      : policy_fetcher_(
            std::make_unique<UserCloudSigninRestrictionPolicyFetcher>(
                nullptr,
                nullptr)) {}

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  UserCloudSigninRestrictionPolicyFetcher* policy_fetcher() {
    return policy_fetcher_.get();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  void ResetPolicyFetcher() { policy_fetcher_.reset(); }

 private:
  base::test::TaskEnvironment task_env_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<UserCloudSigninRestrictionPolicyFetcher> policy_fetcher_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsLegacyValueFromBody) {
  base::Value::Dict expected_response;
  expected_response.Set("policyValue", "primary_account");
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory()->AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Valid());
  EXPECT_FALSE(policies.Empty());
  EXPECT_FALSE(policies.profile_separation_settings());
  EXPECT_FALSE(policies.profile_separation_data_migration_settings());
  EXPECT_EQ("primary_account", policies.managed_accounts_signin_restrictions());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest, ReturnsNewValueFromBody) {
  base::Value::Dict expected_response;
  expected_response.Set("profileSeparationSettings", 1);
  expected_response.Set("profileSeparationDataMigrationSettings", 2);
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory()->AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Valid());
  EXPECT_FALSE(policies.Empty());
  EXPECT_EQ(1, policies.profile_separation_settings());
  EXPECT_EQ(2, policies.profile_separation_data_migration_settings());
  EXPECT_FALSE(policies.managed_accounts_signin_restrictions());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsNewValueWhenLegacyAvailableFromBody) {
  base::Value::Dict expected_response;
  expected_response.Set("policyValue", "primary_account");
  expected_response.Set("profileSeparationSettings", 1);
  expected_response.Set("profileSeparationDataMigrationSettings", 2);
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory()->AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Valid());
  EXPECT_FALSE(policies.Empty());
  EXPECT_EQ(1, policies.profile_separation_settings());
  EXPECT_EQ(2, policies.profile_separation_data_migration_settings());
  EXPECT_FALSE(policies.managed_accounts_signin_restrictions());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueIfNetworkError) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  url_loader_factory()->AddResponse(
      GURL(kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl),
      /*head=*/std::move(head), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Empty());
  EXPECT_TRUE(policies.Valid());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueIfHTTPError) {
  url_loader_factory()->AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl, std::string(),
      net::HTTP_BAD_GATEWAY);

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Empty());
  EXPECT_TRUE(policies.Valid());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsEmptyValueInResponseNotParsable) {
  url_loader_factory()->AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl, "bad");

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Empty());
  EXPECT_TRUE(policies.Valid());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest, ReturnsValueForTesting) {
  url_loader_factory()->AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl, "bad");

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }),
      R"(
        {"policyValue": "none",
         "profileSeparationSettings": 2,
         "profileSeparationDataMigrationSettings": 3})");

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Valid());
  EXPECT_FALSE(policies.Empty());
  EXPECT_EQ(2, policies.profile_separation_settings());
  EXPECT_EQ(3, policies.profile_separation_data_migration_settings());
  EXPECT_FALSE(policies.managed_accounts_signin_restrictions());
}

TEST_F(UserCloudSigninRestrictionPolicyFetcherTest,
       ReturnsNewValueWhenLegacyAvailableFromBodyWithEmptyValueForTesting) {
  base::Value::Dict expected_response;
  expected_response.Set("policyValue", "primary_account");
  expected_response.Set("profileSeparationSettings", 1);
  expected_response.Set("profileSeparationDataMigrationSettings", 2);
  std::string response;
  JSONStringValueSerializer serializer(&response);
  ASSERT_TRUE(serializer.Serialize(expected_response));
  url_loader_factory()->AddResponse(
      kSecureConnectApiGetManagedAccountsSigninRestrictionsUrl,
      std::move(response));

  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  policy::ProfileSeparationPolicies policies;
  policy_fetcher()->SetURLLoaderFactoryForTesting(url_loader_factory());
  policy_fetcher()->GetManagedAccountsSigninRestriction(
      identity_test_env()->identity_manager(), account_info.account_id,
      base::BindLambdaForTesting(
          [&policies](const policy::ProfileSeparationPolicies& res) {
            policies = res;
          }),
      std::string());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(policies.Valid());
  EXPECT_FALSE(policies.Empty());
  EXPECT_EQ(1, policies.profile_separation_settings());
  EXPECT_EQ(2, policies.profile_separation_data_migration_settings());
  EXPECT_FALSE(policies.managed_accounts_signin_restrictions());
}

}  // namespace policy

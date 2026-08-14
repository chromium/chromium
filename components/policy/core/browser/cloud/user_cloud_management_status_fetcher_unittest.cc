// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_cloud_management_status_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/features.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kTestEmail[] = "test@example.com";
const char kTestAccessToken[] = "test_access_token";

struct FetchTestResult {
  std::optional<UserManagementStatus> status;
  std::optional<UserInterceptionPolicies> policies;
};

class UserCloudManagementStatusFetcherTestBase : public testing::Test {
 public:
  UserCloudManagementStatusFetcherTestBase()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        account_id_(identity_test_env_
                        .MakePrimaryAccountAvailable(
                            kTestEmail, signin::ConsentLevel::kSignin)
                        .account_id) {
    scoped_feature_list_.InitAndEnableFeature(
        policy::features::kMigrateSecureConnectApiToDmServer);
    service_.ScheduleInitialization(0);
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  FetchTestResult FetchSync(bool should_fetch_policies) {
    FetchTestResult result;
    UserCloudManagementStatusFetcher::FetchStatusAndPolicies(
        &service_, test_url_loader_factory_.GetSafeWeakWrapper(),
        identity_test_env_.identity_manager(), account_id_,
        should_fetch_policies,
        base::BindLambdaForTesting(
            [&](std::optional<UserManagementStatus> status,
                std::optional<UserInterceptionPolicies> policies) {
              result.status = std::move(status);
              result.policies = std::move(policies);
            }));
    RespondWithAccessToken();
    return result;
  }

  void RespondWithAccessToken() {
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kTestAccessToken, base::Time::Now() + base::Hours(1));
    task_environment_.RunUntilIdle();
  }

  void RespondWithAuthError() {
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError::FromServiceError("auth error"));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService service_{&job_creation_handler_};
  CoreAccountId account_id_;
};

// Parameterized by whether to fetch policies.
class UserCloudManagementStatusFetcherLifecycleTest
    : public UserCloudManagementStatusFetcherTestBase,
      public testing::WithParamInterface<bool> {
 public:
  bool IncludePolicies() const { return GetParam(); }

  void Fetch(base::OnceCallback<void(FetchTestResult)> callback) {
    UserCloudManagementStatusFetcher::FetchStatusAndPolicies(
        &service_, test_url_loader_factory_.GetSafeWeakWrapper(),
        identity_test_env_.identity_manager(), account_id_, IncludePolicies(),
        base::BindOnce(
            [](base::OnceCallback<void(FetchTestResult)> callback,
               std::optional<UserManagementStatus> status,
               std::optional<UserInterceptionPolicies> policies) {
              std::move(callback).Run(
                  FetchTestResult{std::move(status), std::move(policies)});
            },
            std::move(callback)));
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         UserCloudManagementStatusFetcherLifecycleTest,
                         testing::Bool());

TEST_P(UserCloudManagementStatusFetcherLifecycleTest, Failure_Timeout) {
  bool callback_called = false;
  std::optional<UserManagementStatus> result_status;
  Fetch(base::BindLambdaForTesting([&](FetchTestResult result) {
    callback_called = true;
    result_status = std::move(result.status);
  }));

  // Fast forward past the 10.0s timeout.
  task_environment_.FastForwardBy(base::Seconds(11));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(result_status.has_value());
}

TEST_P(UserCloudManagementStatusFetcherLifecycleTest, Failure_ConfigurableTimeout) {
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      features::kMigrateSecureConnectApiToDmServer,
      {{"fetch_timeout", "5s"}});

  bool callback_called = false;
  std::optional<UserManagementStatus> result_status;
  Fetch(base::BindLambdaForTesting([&](FetchTestResult result) {
    callback_called = true;
    result_status = std::move(result.status);
  }));

  // Fast forward past the overridden 5.0s timeout.
  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(result_status.has_value());
}

TEST_P(UserCloudManagementStatusFetcherLifecycleTest, Failure_AuthError) {
  bool callback_called = false;
  std::optional<UserManagementStatus> result_status;
  Fetch(base::BindLambdaForTesting([&](FetchTestResult result) {
    callback_called = true;
    result_status = std::move(result.status);
  }));

  RespondWithAuthError();

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(result_status.has_value());
}

TEST_P(UserCloudManagementStatusFetcherLifecycleTest, Failure_DMServerError) {
  EXPECT_CALL(job_creation_handler_, OnJobCreation(testing::_))
      .WillOnce(service_.SendJobResponseAsync(net::ERR_FAILED, 500,
                                              std::string()));

  bool callback_called = false;
  std::optional<UserManagementStatus> result_status;
  Fetch(base::BindLambdaForTesting([&](FetchTestResult result) {
    callback_called = true;
    result_status = std::move(result.status);
  }));

  RespondWithAccessToken();

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(result_status.has_value());
}

using UserCloudManagementStatusFetcherParsingTest =
    UserCloudManagementStatusFetcherTestBase;

TEST_F(UserCloudManagementStatusFetcherParsingTest,
       StatusFetcher_SuccessfulResponse_Managed) {
  enterprise_management::DeviceManagementResponse response;
  auto* status_response =
      response.mutable_user_management_status_and_policies_response();
  status_response->set_is_account_managed(true);
  status_response->set_is_chrome_profile_management_enabled(true);

  EXPECT_CALL(job_creation_handler_, OnJobCreation(testing::_))
      .WillOnce(service_.SendJobOKAsync(response));

  FetchTestResult result = FetchSync(/*should_fetch_policies=*/false);

  ASSERT_TRUE(result.status.has_value());
  EXPECT_TRUE(result.status->is_account_managed);
  EXPECT_TRUE(result.status->is_chrome_profile_management_enabled);
  EXPECT_TRUE(result.status->CanBeSubjectedToEnterprisePolicies());
  EXPECT_FALSE(result.policies.has_value());
}

TEST_F(UserCloudManagementStatusFetcherParsingTest,
       StatusFetcher_SuccessfulResponse_Unmanaged) {
  enterprise_management::DeviceManagementResponse response;
  auto* status_response =
      response.mutable_user_management_status_and_policies_response();
  status_response->set_is_account_managed(false);
  status_response->set_is_chrome_profile_management_enabled(false);

  EXPECT_CALL(job_creation_handler_, OnJobCreation(testing::_))
      .WillOnce(service_.SendJobOKAsync(response));

  FetchTestResult result = FetchSync(/*should_fetch_policies=*/false);

  ASSERT_TRUE(result.status.has_value());
  EXPECT_FALSE(result.status->is_account_managed);
  EXPECT_FALSE(result.status->is_chrome_profile_management_enabled);
  EXPECT_FALSE(result.status->CanBeSubjectedToEnterprisePolicies());
  EXPECT_FALSE(result.policies.has_value());
}

TEST_F(UserCloudManagementStatusFetcherParsingTest,
       PoliciesFetcher_SuccessfulResponse_WithPolicies) {
  enterprise_management::DeviceManagementResponse response;
  auto* status_response =
      response.mutable_user_management_status_and_policies_response();
  status_response->set_is_account_managed(true);
  status_response->set_is_chrome_profile_management_enabled(true);
  auto* policies = status_response->mutable_signin_experience_policies();
  policies->set_sync_disabled(true);
  policies->set_profile_separation_settings(
      enterprise_management::SigninExperiencePolicies::ENFORCED);
  policies->set_profile_separation_data_migration_settings(
      enterprise_management::SigninExperiencePolicies::USER_OPT_OUT);
  policies->set_browser_theme_color("#ff0000");
  policies->set_enterprise_logo_url("https://example.com/logo.png");
  policies->set_enterprise_custom_label("Corp Inc");
  policies->set_managed_accounts_signin_restrictions("strict");

  EXPECT_CALL(job_creation_handler_, OnJobCreation(testing::_))
      .WillOnce(service_.SendJobOKAsync(response));

  FetchTestResult result = FetchSync(/*should_fetch_policies=*/true);

  ASSERT_TRUE(result.status.has_value());
  EXPECT_TRUE(result.status->is_account_managed);
  EXPECT_TRUE(result.status->is_chrome_profile_management_enabled);

  ASSERT_TRUE(result.policies.has_value());
  EXPECT_EQ(result.policies->sync_disabled, std::optional<bool>(true));
  EXPECT_EQ(result.policies->profile_separation_policies.profile_separation_settings(),
            std::optional<int>(ProfileSeparationSettings::ENFORCED));
  EXPECT_EQ(result.policies->profile_separation_policies.profile_separation_data_migration_settings(),
            std::optional<int>(ProfileSeparationDataMigrationSettings::USER_OPT_OUT));
  EXPECT_EQ(result.policies->browser_theme_color,
            std::optional<std::string>("#ff0000"));
  EXPECT_EQ(result.policies->enterprise_logo_url,
            std::optional<std::string>("https://example.com/logo.png"));
  EXPECT_EQ(result.policies->enterprise_custom_label,
            std::optional<std::string>("Corp Inc"));
  EXPECT_EQ(result.policies->profile_separation_policies.managed_accounts_signin_restrictions(),
            std::optional<std::string>("strict"));
}

TEST_F(UserCloudManagementStatusFetcherParsingTest,
       PoliciesFetcher_SuccessfulResponse_WithoutPolicies) {
  enterprise_management::DeviceManagementResponse response;
  auto* status_response =
      response.mutable_user_management_status_and_policies_response();
  status_response->set_is_account_managed(true);
  status_response->set_is_chrome_profile_management_enabled(true);

  EXPECT_CALL(job_creation_handler_, OnJobCreation(testing::_))
      .WillOnce(service_.SendJobOKAsync(response));

  FetchTestResult result = FetchSync(/*should_fetch_policies=*/true);

  ASSERT_TRUE(result.status.has_value());
  EXPECT_TRUE(result.status->is_account_managed);
  EXPECT_FALSE(result.policies.has_value());
}



TEST_F(UserCloudManagementStatusFetcherParsingTest,
       DisabledFeatureFlagCrashes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      policy::features::kMigrateSecureConnectApiToDmServer);

  EXPECT_CHECK_DEATH(UserCloudManagementStatusFetcher::FetchStatusAndPolicies(
      &service_, test_url_loader_factory_.GetSafeWeakWrapper(),
      identity_test_env_.identity_manager(), account_id_,
      /*should_fetch_policies=*/false, base::DoNothing()));
}

}  // namespace

}  // namespace policy


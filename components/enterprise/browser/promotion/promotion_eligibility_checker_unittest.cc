// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "device_management_backend.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;

namespace {

constexpr char kExpectedAccessToken[] = "access_token";
constexpr char kNonEmptyDMToken[] = "dm_token";
constexpr char kProfileId[] = "profile_id";
constexpr char kClientId[] = "client_id";
constexpr char kValidLocale[] = "en-US";
constexpr char kInvalidLocale[] = "en-GB";
constexpr bool kDismissedBannerPref = true;
}  // namespace

namespace enterprise_promotion {

class PromotionEligibilityCheckerTest : public testing::Test {
 public:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  void SetUp() override {
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetURLLoaderFactoryForTesting(
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
    client_->SetDMToken(kNonEmptyDMToken);
    client_->SetClientId(kClientId);
    account_id_ = identity_test_env()
                      ->MakePrimaryAccountAvailable(
                          "test@example.com", signin::ConsentLevel::kSignin)
                      .account_id;

    checker_ = std::make_unique<PromotionEligibilityChecker>(
        kProfileId, client_.get(), identity_test_env()->identity_manager(),
        kValidLocale, !kDismissedBannerPref);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  CoreAccountId account_id_;
  std::unique_ptr<PromotionEligibilityChecker> checker_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

TEST_F(PromotionEligibilityCheckerTest, FetchAccessTokenSuccess) {
  auto client = std::make_unique<policy::MockCloudPolicyClient>();

  auto* client_ptr = client.get();

  checker_->SetCloudPolicyClientForTesting(std::move(client));

  client_ptr->SetDMToken(kNonEmptyDMToken);

  checker_->MaybeCheckPromotionEligibility(account_id_, base::DoNothing());

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kExpectedAccessToken, base::Time::Max());

  EXPECT_EQ(client_ptr->GetOAuthToken(), kExpectedAccessToken);
}

TEST_F(PromotionEligibilityCheckerTest, DeterminePromotionEligibilitySuccess) {
  base::MockCallback<base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>>
      callback;

  enterprise_management::GetUserEligiblePromotionsResponse response;
  response.mutable_promotions()->set_policy_page_promotion(
      enterprise_management::CHROME_ENTERPRISE_CORE);

  std::string actual_oauth_token;

  auto client = std::make_unique<policy::MockCloudPolicyClient>();

  auto* client_ptr = client.get();

  EXPECT_CALL(*client, DeterminePromotionEligibility(testing::_))
      .WillOnce(testing::Invoke(
          [response](
              policy::CloudPolicyClient::PromotionEligibilityCallback cb) {
            std::move(cb).Run(response);
          }));
  EXPECT_CALL(callback, Run(EqualsProto(response)));

  checker_->SetCloudPolicyClientForTesting(std::move(client));

  client_ptr->SetDMToken(kNonEmptyDMToken);

  checker_->MaybeCheckPromotionEligibility(account_id_, callback.Get());

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kExpectedAccessToken, base::Time::Max());
}

TEST_F(PromotionEligibilityCheckerTest,
       DeterminePromotionEligibilityEmptyAccessToken) {
  base::MockCallback<base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>>
      callback;
  auto client = std::make_unique<policy::MockCloudPolicyClient>();

  auto* client_ptr = client.get();

  checker_->SetCloudPolicyClientForTesting(std::move(client));

  client_ptr->SetDMToken(kNonEmptyDMToken);

  EXPECT_CALL(callback,
              Run(EqualsProto(
                  enterprise_management::GetUserEligiblePromotionsResponse())));

  checker_->MaybeCheckPromotionEligibility(account_id_, callback.Get());

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "", base::Time::Max());
}

TEST_F(PromotionEligibilityCheckerTest,
       DeterminePromotionEligibilityInvalidAccessToken) {
  base::MockCallback<base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>>
      callback;
  auto client = std::make_unique<policy::MockCloudPolicyClient>();

  auto* client_ptr = client.get();

  checker_->SetCloudPolicyClientForTesting(std::move(client));

  client_ptr->SetDMToken(kNonEmptyDMToken);

  EXPECT_CALL(callback,
              Run(EqualsProto(
                  enterprise_management::GetUserEligiblePromotionsResponse())));
  checker_->MaybeCheckPromotionEligibility(account_id_, callback.Get());

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

TEST_F(PromotionEligibilityCheckerTest,
       DeterminePromotionEligibilityNoDMToken) {
  base::MockCallback<base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>>
      callback;
  std::unique_ptr<policy::MockCloudPolicyClient> client_no_dm_token =
      std::make_unique<policy::MockCloudPolicyClient>();

  client_no_dm_token->SetDMToken("");
  client_no_dm_token->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_CALL(callback,
              Run(EqualsProto(
                  enterprise_management::GetUserEligiblePromotionsResponse())));
  PromotionEligibilityChecker checker_no_dm_token(
      kProfileId, client_no_dm_token.get(),
      identity_test_env()->identity_manager(), kValidLocale,
      !kDismissedBannerPref);

  checker_no_dm_token.MaybeCheckPromotionEligibility(account_id_,
                                                     callback.Get());
}

TEST_F(PromotionEligibilityCheckerTest,
       DeterminePromotionEligibilityDismissedBannerPref) {
  base::MockCallback<base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>>
      callback;
  std::unique_ptr<policy::MockCloudPolicyClient> client =
      std::make_unique<policy::MockCloudPolicyClient>();

  client->SetDMToken(kNonEmptyDMToken);
  client->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_CALL(callback,
              Run(EqualsProto(
                  enterprise_management::GetUserEligiblePromotionsResponse())));
  PromotionEligibilityChecker checker(kProfileId, client.get(),
                                      identity_test_env()->identity_manager(),
                                      kValidLocale, kDismissedBannerPref);

  checker.MaybeCheckPromotionEligibility(account_id_, callback.Get());
}

TEST_F(PromotionEligibilityCheckerTest,
       DeterminePromotionEligibilityInvalidLocale) {
  base::MockCallback<base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>>
      callback;
  std::unique_ptr<policy::MockCloudPolicyClient> client_invalid_locale =
      std::make_unique<policy::MockCloudPolicyClient>();

  client_invalid_locale->SetDMToken(kNonEmptyDMToken);
  client_invalid_locale->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_CALL(callback,
              Run(EqualsProto(
                  enterprise_management::GetUserEligiblePromotionsResponse())));
  PromotionEligibilityChecker checker_invalid_locale(
      kProfileId, client_invalid_locale.get(),
      identity_test_env()->identity_manager(), kInvalidLocale,
      !kDismissedBannerPref);

  checker_invalid_locale.MaybeCheckPromotionEligibility(account_id_,
                                                        callback.Get());
}

TEST_F(PromotionEligibilityCheckerTest,
       DeterminePromotionEligibilityNoAccount) {
  base::MockCallback<base::OnceCallback<void(
      enterprise_management::GetUserEligiblePromotionsResponse)>>
      callback;
  std::unique_ptr<policy::MockCloudPolicyClient> client_no_account =
      std::make_unique<policy::MockCloudPolicyClient>();

  client_no_account->SetClientId(kClientId);
  client_no_account->SetDMToken(kNonEmptyDMToken);
  client_no_account->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  EXPECT_CALL(callback,
              Run(EqualsProto(
                  enterprise_management::GetUserEligiblePromotionsResponse())));
  PromotionEligibilityChecker checker_no_account(
      kProfileId, client_no_account.get(),
      identity_test_env()->identity_manager(), kValidLocale,
      !kDismissedBannerPref);

  checker_no_account.MaybeCheckPromotionEligibility(CoreAccountId(),
                                                    callback.Get());
}

}  // namespace enterprise_promotion

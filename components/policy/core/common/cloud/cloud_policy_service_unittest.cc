// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;

namespace policy {

constexpr auto kTestReason = PolicyFetchReason::kTest;

class CloudPolicyServiceTest : public testing::Test {
 public:
  CloudPolicyServiceTest()
      : policy_type_(dm_protocol::kChromeUserPolicyType),
        service_(policy_type_, std::string(), &client_, &store_) {}

  MOCK_METHOD1(OnPolicyRefresh, void(bool));

 protected:
  std::string policy_type_;
  MockCloudPolicyClient client_;
  MockCloudPolicyStore store_;
  CloudPolicyService service_;
};

MATCHER_P(ProtoMatches, proto, std::string()) {
  return arg.SerializePartialAsString() == proto.SerializePartialAsString();
}

TEST_F(CloudPolicyServiceTest, PolicyUpdateSuccess) {
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_type_, std::string(), policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // After |store_| initializes, credentials and other meta data should be
  // transferred to |client_|.
  auto policy_data = std::make_unique<em::PolicyData>();
  policy_data->set_request_token("fake token");
  policy_data->set_device_id("fake client id");
  policy_data->set_timestamp(32);
  policy_data->set_public_key_version(17);
  policy_data->add_user_affiliation_ids("id1");
  store_.set_policy_data_for_testing(std::move(policy_data));
  std::vector<std::string> user_affiliation_ids = {
      store_.policy()->user_affiliation_ids(0)};
  EXPECT_CALL(client_, SetupRegistration(store_.policy()->request_token(),
                                         store_.policy()->device_id(),
                                         user_affiliation_ids))
      .Times(1);
  EXPECT_CALL(client_, UploadPolicyValidationReport(_, _, _, _, _)).Times(0);
  store_.NotifyStoreLoaded();
  EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(32),
            client_.last_policy_timestamp_);
  EXPECT_TRUE(client_.public_key_version_valid_);
  EXPECT_EQ(17, client_.public_key_version_);
}

TEST_F(CloudPolicyServiceTest, PolicyUpdateClientFailure) {
  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  EXPECT_CALL(store_, Store(_)).Times(0);
  client_.NotifyPolicyFetched();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicySuccess) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  service_.RefreshPolicy(
      base::BindOnce(&CloudPolicyServiceTest::OnPolicyRefresh,
                     base::Unretained(this)),
      kTestReason);

  // Client responds, push policy to store.
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_type_, std::string(), policy);
  client_.fetched_invalidation_version_ = 12345;
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  EXPECT_EQ(0, store_.invalidation_version());
  client_.NotifyPolicyFetched();
  EXPECT_EQ(12345, store_.invalidation_version());

  // Store reloads policy, callback gets triggered.
  auto policy_data = std::make_unique<em::PolicyData>();
  policy_data->set_request_token("token");
  policy_data->set_device_id("device-id");
  store_.set_policy_data_for_testing(std::move(policy_data));
  EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(1);
  store_.NotifyStoreLoaded();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyNotRegistered) {
  // Clear the token so the client is not registered.
  client_.SetDMToken(std::string());

  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(0);
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  service_.RefreshPolicy(
      base::BindOnce(&CloudPolicyServiceTest::OnPolicyRefresh,
                     base::Unretained(this)),
      kTestReason);
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyClientError) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  service_.RefreshPolicy(
      base::BindOnce(&CloudPolicyServiceTest::OnPolicyRefresh,
                     base::Unretained(this)),
      kTestReason);

  // Client responds with an error, which should trigger the callback.
  client_.SetStatus(DM_STATUS_REQUEST_FAILED);
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  client_.NotifyClientError();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyStoreError) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  service_.RefreshPolicy(
      base::BindOnce(&CloudPolicyServiceTest::OnPolicyRefresh,
                     base::Unretained(this)),
      kTestReason);

  // Client responds, push policy to store.
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_type_, std::string(), policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // Store fails, which should trigger the callback.
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  store_.NotifyStoreError();
}

TEST_F(CloudPolicyServiceTest, RefreshPolicyConcurrent) {
  testing::InSequence seq;

  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  client_.SetDMToken("fake token");

  // Trigger a fetch on the client.
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  service_.RefreshPolicy(
      base::BindOnce(&CloudPolicyServiceTest::OnPolicyRefresh,
                     base::Unretained(this)),
      kTestReason);

  // Triggering another policy refresh should generate a new fetch request.
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  service_.RefreshPolicy(
      base::BindOnce(&CloudPolicyServiceTest::OnPolicyRefresh,
                     base::Unretained(this)),
      kTestReason);

  // Client responds, push policy to store.
  em::PolicyFetchResponse policy;
  policy.set_policy_data("fake policy");
  client_.SetPolicy(policy_type_, std::string(), policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // Trigger another policy fetch.
  EXPECT_CALL(client_, FetchPolicy(kTestReason)).Times(1);
  service_.RefreshPolicy(
      base::BindOnce(&CloudPolicyServiceTest::OnPolicyRefresh,
                     base::Unretained(this)),
      kTestReason);

  // The store finishing the first load should not generate callbacks.
  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  store_.NotifyStoreLoaded();

  // Second policy fetch finishes.
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  client_.NotifyPolicyFetched();

  // Corresponding store operation finishes, all _three_ callbacks fire.
  EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(3);
  store_.NotifyStoreLoaded();
}

TEST_F(CloudPolicyServiceTest, StoreAlreadyInitialized) {
  // Service should start off initialized if the store has already loaded
  // policy.
  store_.NotifyStoreLoaded();
  CloudPolicyService service(policy_type_, std::string(), &client_, &store_);
  EXPECT_TRUE(service.IsInitializationComplete());
}

TEST_F(CloudPolicyServiceTest, StoreLoadAfterCreation) {
  // Service should start off un-initialized if the store has not yet loaded
  // policy.
  EXPECT_FALSE(service_.IsInitializationComplete());
  MockCloudPolicyServiceObserver observer;
  service_.AddObserver(&observer);
  // Service should be marked as initialized and observer should be called back.
  EXPECT_CALL(observer, OnCloudPolicyServiceInitializationCompleted()).Times(1);
  store_.NotifyStoreLoaded();
  EXPECT_TRUE(service_.IsInitializationComplete());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Now, the next time the store is loaded, the observer should not be called
  // again.
  EXPECT_CALL(observer, OnCloudPolicyServiceInitializationCompleted()).Times(0);
  store_.NotifyStoreLoaded();
  service_.RemoveObserver(&observer);
}

TEST_F(CloudPolicyServiceTest, ReportValidationResult) {
  // Sync |policy_data_signature| between store and service by fetching a
  // policy.
  em::PolicyFetchResponse policy;
  policy.set_policy_data_signature("fake-policy-data-signature");
  client_.SetPolicy(policy_type_, std::string(), policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  EXPECT_CALL(client_, UploadPolicyValidationReport(_, _, _, _, _)).Times(0);
  client_.NotifyPolicyFetched();

  // Simulate a value validation error from the store and expect a validation
  // report to be uploaded.
  store_.validation_result_ =
      std::make_unique<CloudPolicyValidatorBase::ValidationResult>();
  store_.validation_result_->status =
      CloudPolicyValidatorBase::VALIDATION_VALUE_ERROR;
  store_.validation_result_->value_validation_issues.push_back(
      {"fake-policy-name", ValueValidationIssue::kError, "message"});
  store_.validation_result_->policy_token = "fake-policy-token";
  store_.validation_result_->policy_data_signature =
      "fake-policy-data-signature";
  EXPECT_CALL(client_,
              UploadPolicyValidationReport(
                  store_.validation_result_->status,
                  store_.validation_result_->value_validation_issues, kLoad,
                  policy_type_, store_.validation_result_->policy_token))
      .Times(1);
  store_.NotifyStoreError();

  // A second validation of the same policy should not trigger another upload.
  EXPECT_CALL(client_, UploadPolicyValidationReport(_, _, _, _, _)).Times(0);
  store_.NotifyStoreError();

  testing::Mock::VerifyAndClearExpectations(&client_);
}

TEST_F(CloudPolicyServiceTest, ReportValidationResultWrongSignature) {
  // Sync |policy_data_signature| between store and service by fetching a
  // policy.
  em::PolicyFetchResponse policy;
  policy.set_policy_data_signature("fake-policy-data-signature-1");
  client_.SetPolicy(policy_type_, std::string(), policy);
  EXPECT_CALL(store_, Store(ProtoMatches(policy))).Times(1);
  EXPECT_CALL(client_, UploadPolicyValidationReport(_, _, _, _, _)).Times(0);
  client_.NotifyPolicyFetched();

  // Simulate a value validation error from the store with a different policy
  // data signature. No Validation report should be uploaded in that case.
  store_.validation_result_ =
      std::make_unique<CloudPolicyValidatorBase::ValidationResult>();
  store_.validation_result_->status =
      CloudPolicyValidatorBase::VALIDATION_VALUE_ERROR;
  store_.validation_result_->value_validation_issues.push_back(
      {"fake-policy-name", ValueValidationIssue::kError, "message"});
  store_.validation_result_->policy_token = "fake-policy-token";
  store_.validation_result_->policy_data_signature =
      "fake-policy-data-signature-2";
  EXPECT_CALL(client_, UploadPolicyValidationReport(_, _, _, _, _)).Times(0);
  store_.NotifyStoreError();

  testing::Mock::VerifyAndClearExpectations(&client_);
}

}  // namespace policy

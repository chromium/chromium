// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_manager.h"

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::_;

namespace em = enterprise_management;

namespace policy {
namespace {

class TestHarness : public PolicyProviderTestHarness {
 public:
  explicit TestHarness(PolicyLevel level);
  ~TestHarness() override;

  void SetUp() override;

  ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

  void InstallEmptyPolicy() override;
  void InstallStringPolicy(const std::string& policy_name,
                           const std::string& policy_value) override;
  void InstallIntegerPolicy(const std::string& policy_name,
                            int policy_value) override;
  void InstallBooleanPolicy(const std::string& policy_name,
                            bool policy_value) override;
  void InstallStringListPolicy(const std::string& policy_name,
                               const base::ListValue* policy_value) override;
  void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) override;

  // Creates harnesses for mandatory and recommended levels, respectively.
  static PolicyProviderTestHarness* CreateMandatory();
  static PolicyProviderTestHarness* CreateRecommended();

 private:
  MockCloudPolicyStore store_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness(PolicyLevel level)
    : PolicyProviderTestHarness(level, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD) {
}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Create and initialize the store.
  store_.NotifyStoreLoaded();
  ConfigurationPolicyProvider* provider = new CloudPolicyManager(
      dm_protocol::kChromeUserPolicyType, std::string(), &store_, task_runner,
      network::TestNetworkConnectionTracker::CreateGetter());
  Mock::VerifyAndClearExpectations(&store_);
  return provider;
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  store_.policy_map_.Set(policy_name, policy_level(), policy_scope(),
                         POLICY_SOURCE_CLOUD,
                         std::make_unique<base::Value>(policy_value), nullptr);
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  store_.policy_map_.Set(policy_name, policy_level(), policy_scope(),
                         POLICY_SOURCE_CLOUD,
                         std::make_unique<base::Value>(policy_value), nullptr);
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  store_.policy_map_.Set(policy_name, policy_level(), policy_scope(),
                         POLICY_SOURCE_CLOUD,
                         std::make_unique<base::Value>(policy_value), nullptr);
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  store_.policy_map_.Set(policy_name, policy_level(), policy_scope(),
                         POLICY_SOURCE_CLOUD, policy_value->CreateDeepCopy(),
                         nullptr);
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  store_.policy_map_.Set(policy_name, policy_level(), policy_scope(),
                         POLICY_SOURCE_CLOUD, policy_value->CreateDeepCopy(),
                         nullptr);
}

// static
PolicyProviderTestHarness* TestHarness::CreateMandatory() {
  return new TestHarness(POLICY_LEVEL_MANDATORY);
}

// static
PolicyProviderTestHarness* TestHarness::CreateRecommended() {
  return new TestHarness(POLICY_LEVEL_RECOMMENDED);
}

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_SUITE_P(UserCloudPolicyManagerProviderTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::CreateMandatory,
                                         TestHarness::CreateRecommended));

class TestCloudPolicyManager : public CloudPolicyManager {
 public:
  TestCloudPolicyManager(
      CloudPolicyStore* store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : CloudPolicyManager(
            dm_protocol::kChromeUserPolicyType,
            std::string(),
            store,
            task_runner,
            network::TestNetworkConnectionTracker::CreateGetter()) {}
  ~TestCloudPolicyManager() override {}

  // Publish the protected members for testing.
  using CloudPolicyManager::client;
  using CloudPolicyManager::store;
  using CloudPolicyManager::service;
  using CloudPolicyManager::CheckAndPublishPolicy;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCloudPolicyManager);
};

MATCHER_P(ProtoMatches, proto, std::string()) {
  return arg.SerializePartialAsString() == proto.SerializePartialAsString();
}

class CloudPolicyManagerTest : public testing::Test {
 protected:
  CloudPolicyManagerTest()
      : policy_type_(dm_protocol::kChromeUserPolicyType) {}

  void SetUp() override {
    // Set up a policy map for testing.
    policy_map_.Set("key", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("value"),
                    nullptr);
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .CopyFrom(policy_map_);

    policy_.payload().mutable_searchsuggestenabled()->set_value(false);
    policy_.Build();

    EXPECT_CALL(store_, Load());
    manager_.reset(new TestCloudPolicyManager(
        &store_, task_environment_.GetMainThreadTaskRunner()));
    manager_->Init(&schema_registry_);
    Mock::VerifyAndClearExpectations(&store_);
    manager_->AddObserver(&observer_);
  }

  void TearDown() override {
    manager_->RemoveObserver(&observer_);
    manager_->Shutdown();
  }

  // Needs to be the first member.
  base::test::TaskEnvironment task_environment_;

  // Testing policy.
  const std::string policy_type_;
  UserPolicyBuilder policy_;
  PolicyMap policy_map_;
  PolicyBundle expected_bundle_;

  // Policy infrastructure.
  SchemaRegistry schema_registry_;
  MockConfigurationPolicyObserver observer_;
  MockCloudPolicyStore store_;
  std::unique_ptr<TestCloudPolicyManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPolicyManagerTest);
};

TEST_F(CloudPolicyManagerTest, InitAndShutdown) {
  PolicyBundle empty_bundle;
  EXPECT_TRUE(empty_bundle.Equals(manager_->policies()));
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  manager_->CheckAndPublishPolicy();
  Mock::VerifyAndClearExpectations(&observer_);

  store_.policy_map_.CopyFrom(policy_map_);
  store_.policy_.reset(new em::PolicyData(policy_.policy_data()));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  MockCloudPolicyClient* client = new MockCloudPolicyClient();
  EXPECT_CALL(*client, SetupRegistration(_, _, _));
  manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));
  Mock::VerifyAndClearExpectations(client);
  EXPECT_TRUE(manager_->client());
  EXPECT_TRUE(manager_->service());

  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  manager_->CheckAndPublishPolicy();
  Mock::VerifyAndClearExpectations(&observer_);

  manager_->core()->Disconnect();
  EXPECT_FALSE(manager_->client());
  EXPECT_FALSE(manager_->service());
}

TEST_F(CloudPolicyManagerTest, RegistrationAndFetch) {
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  MockCloudPolicyClient* client = new MockCloudPolicyClient();
  manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));

  client->SetDMToken(policy_.policy_data().request_token());
  client->NotifyRegistrationStateChanged();

  client->SetPolicy(policy_type_, std::string(), policy_.policy());
  EXPECT_CALL(store_, Store(ProtoMatches(policy_.policy())));
  client->NotifyPolicyFetched();
  Mock::VerifyAndClearExpectations(&store_);

  store_.policy_map_.CopyFrom(policy_map_);
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
}

TEST_F(CloudPolicyManagerTest, Update) {
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  PolicyBundle empty_bundle;
  EXPECT_TRUE(empty_bundle.Equals(manager_->policies()));

  store_.policy_map_.CopyFrom(policy_map_);
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

TEST_F(CloudPolicyManagerTest, RefreshNotRegistered) {
  MockCloudPolicyClient* client = new MockCloudPolicyClient();
  manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));

  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);

  // A refresh on a non-registered store should not block.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  manager_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(CloudPolicyManagerTest, RefreshSuccessful) {
  MockCloudPolicyClient* client = new MockCloudPolicyClient();
  manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));

  // Simulate a store load.
  store_.policy_.reset(new em::PolicyData(policy_.policy_data()));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  EXPECT_CALL(*client, SetupRegistration(_, _, _));
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(client);
  Mock::VerifyAndClearExpectations(&observer_);

  // Acknowledge registration.
  client->SetDMToken(policy_.policy_data().request_token());

  // Start a refresh.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  EXPECT_CALL(*client, FetchPolicy());
  manager_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(client);
  Mock::VerifyAndClearExpectations(&observer_);
  store_.policy_map_.CopyFrom(policy_map_);

  // A stray reload should be suppressed until the refresh completes.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  store_.NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);

  // Respond to the policy fetch, which should trigger a write to |store_|.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  EXPECT_CALL(store_, Store(_));
  client->SetPolicy(policy_type_, std::string(), policy_.policy());
  client->NotifyPolicyFetched();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&store_);

  // The load notification from |store_| should trigger the policy update.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreLoaded();
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(CloudPolicyManagerTest, SignalOnError) {
  // Simulate a failed load and verify that it triggers OnUpdatePolicy().
  store_.policy_.reset(new em::PolicyData(policy_.policy_data()));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_.NotifyStoreError();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

}  // namespace
}  // namespace policy

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_manager.h"

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
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
  TestHarness(const TestHarness&) = delete;
  TestHarness& operator=(const TestHarness&) = delete;
  ~TestHarness() override;

  void SetUp() override;

  void TearDown() override;

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
                               const base::Value::List& policy_value) override;
  void InstallDictionaryPolicy(const std::string& policy_name,
                               const base::Value::Dict& policy_value) override;

  // Creates harnesses for mandatory and recommended levels, respectively.
  static PolicyProviderTestHarness* CreateMandatory();
  static PolicyProviderTestHarness* CreateRecommended();

 private:
  raw_ptr<MockCloudPolicyStore> store_;
};

TestHarness::TestHarness(PolicyLevel level)
    : PolicyProviderTestHarness(level, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD) {
}

TestHarness::~TestHarness() = default;

void TestHarness::SetUp() {}

void TestHarness::TearDown() {
  store_ = nullptr;
}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Create and initialize the store.
  auto store = std::make_unique<MockCloudPolicyStore>();
  store_ = store.get();
  store_->NotifyStoreLoaded();
  ConfigurationPolicyProvider* provider = new CloudPolicyManager(
      dm_protocol::kChromeUserPolicyType, std::string(), std::move(store),
      task_runner, network::TestNetworkConnectionTracker::CreateGetter());
  Mock::VerifyAndClearExpectations(store_.get());
  return provider;
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  store_->policy_map_.Set(policy_name, policy_level(), policy_scope(),
                          POLICY_SOURCE_CLOUD, base::Value(policy_value),
                          nullptr);
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  store_->policy_map_.Set(policy_name, policy_level(), policy_scope(),
                          POLICY_SOURCE_CLOUD, base::Value(policy_value),
                          nullptr);
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  store_->policy_map_.Set(policy_name, policy_level(), policy_scope(),
                          POLICY_SOURCE_CLOUD, base::Value(policy_value),
                          nullptr);
}

void TestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::Value::List& policy_value) {
  store_->policy_map_.Set(policy_name, policy_level(), policy_scope(),
                          POLICY_SOURCE_CLOUD,
                          base::Value(policy_value.Clone()), nullptr);
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::Value::Dict& policy_value) {
  store_->policy_map_.Set(policy_name, policy_level(), policy_scope(),
                          POLICY_SOURCE_CLOUD,
                          base::Value(policy_value.Clone()), nullptr);
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

MATCHER_P(ProtoMatches, proto, std::string()) {
  return arg.SerializePartialAsString() == proto.SerializePartialAsString();
}

class CloudPolicyManagerTest : public testing::Test {
 public:
  CloudPolicyManagerTest(const CloudPolicyManagerTest&) = delete;
  CloudPolicyManagerTest& operator=(const CloudPolicyManagerTest&) = delete;

 protected:
  CloudPolicyManagerTest()
      : policy_type_(dm_protocol::kChromeUserPolicyType) {}

  void SetUp() override {
    // Set up a policy map for testing.
    policy_map_.Set("key", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
        policy_map_.Clone();

    policy_.payload().mutable_searchsuggestenabled()->set_value(false);
    policy_.Build();

    auto store = std::make_unique<MockCloudPolicyStore>();
    store_ = store.get();
    EXPECT_CALL(*store_, Load());
    manager_ = std::make_unique<MockCloudPolicyManager>(
        std::move(store), task_environment_.GetMainThreadTaskRunner());
    manager_->Init(&schema_registry_);
    Mock::VerifyAndClearExpectations(store_.get());
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
  std::unique_ptr<MockCloudPolicyManager> manager_;
  raw_ptr<MockCloudPolicyStore> store_;
};

TEST_F(CloudPolicyManagerTest, InitAndShutdown) {
  PolicyBundle empty_bundle;
  EXPECT_TRUE(empty_bundle.Equals(manager_->policies()));
  EXPECT_FALSE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  manager_->CheckAndPublishPolicy();
  Mock::VerifyAndClearExpectations(&observer_);

  store_->policy_map_ = policy_map_.Clone();
  store_->set_policy_data_for_testing(
      std::make_unique<em::PolicyData>(policy_.policy_data()));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
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
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  MockCloudPolicyClient* client = new MockCloudPolicyClient();
  manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));

  client->SetDMToken(policy_.policy_data().request_token());
  client->NotifyRegistrationStateChanged();

  client->SetPolicy(policy_type_, std::string(), policy_.policy());
  EXPECT_CALL(*store_, Store(ProtoMatches(policy_.policy())));
  client->NotifyPolicyFetched();
  Mock::VerifyAndClearExpectations(store_.get());

  store_->policy_map_ = policy_map_.Clone();
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
}

TEST_F(CloudPolicyManagerTest, Update) {
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  PolicyBundle empty_bundle;
  EXPECT_TRUE(empty_bundle.Equals(manager_->policies()));

  store_->policy_map_ = policy_map_.Clone();
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

TEST_F(CloudPolicyManagerTest, RefreshNotRegistered) {
  MockCloudPolicyClient* client = new MockCloudPolicyClient();
  manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));

  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);

  // A refresh on a non-registered store should not block.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  manager_->RefreshPolicies(PolicyFetchReason::kTest);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(CloudPolicyManagerTest, RefreshSuccessful) {
  MockCloudPolicyClient* client = new MockCloudPolicyClient();
  manager_->core()->Connect(std::unique_ptr<CloudPolicyClient>(client));

  // Simulate a store load.
  store_->set_policy_data_for_testing(
      std::make_unique<em::PolicyData>(policy_.policy_data()));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  EXPECT_CALL(*client, SetupRegistration(_, _, _));
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(client);
  Mock::VerifyAndClearExpectations(&observer_);

  // Acknowledge registration.
  client->SetDMToken(policy_.policy_data().request_token());

  // Start a refresh.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  EXPECT_CALL(*client, FetchPolicy(_));
  manager_->RefreshPolicies(PolicyFetchReason::kTest);
  Mock::VerifyAndClearExpectations(client);
  Mock::VerifyAndClearExpectations(&observer_);
  store_->policy_map_ = policy_map_.Clone();

  // A stray reload should be suppressed until the refresh completes.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  store_->NotifyStoreLoaded();
  Mock::VerifyAndClearExpectations(&observer_);

  // Respond to the policy fetch, which should trigger a write to |store_|.
  EXPECT_CALL(observer_, OnUpdatePolicy(_)).Times(0);
  EXPECT_CALL(*store_, Store(_));
  client->SetPolicy(policy_type_, std::string(), policy_.policy());
  client->NotifyPolicyFetched();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(store_.get());

  // The load notification from |store_| should trigger the policy update.
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreLoaded();
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(CloudPolicyManagerTest, SignalOnError) {
  // Simulate a failed load and verify that it triggers OnUpdatePolicy().
  store_->set_policy_data_for_testing(
      std::make_unique<em::PolicyData>(policy_.policy_data()));
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get()));
  store_->NotifyStoreError();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

}  // namespace
}  // namespace policy

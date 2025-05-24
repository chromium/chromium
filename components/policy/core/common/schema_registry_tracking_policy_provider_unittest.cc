// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/schema_registry_tracking_policy_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;
using testing::_;

namespace policy {

namespace {

constexpr auto test_reason = PolicyFetchReason::kTest;

const char kTestSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"foo\": { \"type\": \"string\" }"
    "  }"
    "}";

}  // namespace

class SchemaRegistryTrackingPolicyProviderTest : public testing::Test {
 protected:
  SchemaRegistryTrackingPolicyProviderTest()
      : schema_registry_tracking_provider_(&mock_provider_) {
    mock_provider_.Init();
    schema_registry_tracking_provider_.Init(&schema_registry_);
    schema_registry_tracking_provider_.AddObserver(&observer_);
  }

  ~SchemaRegistryTrackingPolicyProviderTest() override {
    schema_registry_tracking_provider_.RemoveObserver(&observer_);
    schema_registry_tracking_provider_.Shutdown();
    mock_provider_.Shutdown();
  }

  Schema CreateTestSchema() {
    ASSIGN_OR_RETURN(const auto schema, Schema::Parse(kTestSchema),
                     [](const auto& e) {
                       ADD_FAILURE() << e;
                       return Schema();
                     });
    return schema;
  }

  SchemaRegistry schema_registry_;
  MockConfigurationPolicyObserver observer_;
  MockConfigurationPolicyProvider mock_provider_;
  SchemaRegistryTrackingPolicyProvider schema_registry_tracking_provider_;
};

TEST_F(SchemaRegistryTrackingPolicyProviderTest, Empty) {
  EXPECT_FALSE(schema_registry_.IsReady());
  EXPECT_FALSE(schema_registry_tracking_provider_.IsInitializationComplete(
      POLICY_DOMAIN_EXTENSIONS));

  EXPECT_CALL(mock_provider_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillOnce(Return(false));
  EXPECT_FALSE(schema_registry_tracking_provider_.IsInitializationComplete(
      POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&mock_provider_);

  const PolicyBundle empty_bundle;
  EXPECT_TRUE(
      schema_registry_tracking_provider_.policies().Equals(empty_bundle));
}

TEST_F(SchemaRegistryTrackingPolicyProviderTest, PassOnChromePolicy) {
  PolicyBundle bundle;
  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  bundle.Get(chrome_ns).Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                            POLICY_SOURCE_CLOUD, base::Value("visible"),
                            nullptr);

  EXPECT_CALL(observer_, OnUpdatePolicy(&schema_registry_tracking_provider_));
  PolicyBundle delegate_bundle = bundle.Clone();
  delegate_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"))
      .Set("foo", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           POLICY_SOURCE_CLOUD, base::Value("not visible"), nullptr);
  mock_provider_.UpdatePolicy(std::move(delegate_bundle));
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_FALSE(schema_registry_tracking_provider_.IsInitializationComplete(
      POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(schema_registry_tracking_provider_.policies().Equals(bundle));
}

TEST_F(SchemaRegistryTrackingPolicyProviderTest, RefreshPolicies) {
  EXPECT_CALL(mock_provider_, RefreshPolicies(test_reason));
  schema_registry_tracking_provider_.RefreshPolicies(test_reason);
  Mock::VerifyAndClearExpectations(&mock_provider_);
}

TEST_F(SchemaRegistryTrackingPolicyProviderTest, SchemaReady) {
  EXPECT_CALL(observer_, OnUpdatePolicy(&schema_registry_tracking_provider_));
  schema_registry_.SetAllDomainsReady();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(schema_registry_tracking_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));
}

TEST_F(SchemaRegistryTrackingPolicyProviderTest, SchemaReadyWithComponents) {
  PolicyMap policy_map;
  policy_map.Set("foo", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("omg"), nullptr);
  PolicyBundle bundle;
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, "")) = policy_map.Clone();
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz")) =
      policy_map.Clone();
  EXPECT_CALL(observer_, OnUpdatePolicy(&schema_registry_tracking_provider_));
  mock_provider_.UpdatePolicy(std::move(bundle));
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(mock_provider_,
              RefreshPolicies(PolicyFetchReason::kSchemaUpdated))
      .Times(0);
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"), CreateTestSchema());
  schema_registry_.SetExtensionsDomainsReady();
  Mock::VerifyAndClearExpectations(&mock_provider_);

  EXPECT_CALL(mock_provider_,
              RefreshPolicies(PolicyFetchReason::kSchemaUpdated));
  schema_registry_.SetDomainReady(POLICY_DOMAIN_CHROME);
  Mock::VerifyAndClearExpectations(&mock_provider_);

  EXPECT_FALSE(schema_registry_tracking_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, "")) =
      policy_map.Clone();
  EXPECT_TRUE(
      schema_registry_tracking_provider_.policies().Equals(expected_bundle));

  EXPECT_CALL(observer_, OnUpdatePolicy(&schema_registry_tracking_provider_));
  schema_registry_tracking_provider_.OnUpdatePolicy(&mock_provider_);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(schema_registry_tracking_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz")) =
      policy_map.Clone();
  EXPECT_TRUE(
      schema_registry_tracking_provider_.policies().Equals(expected_bundle));
}

TEST_F(SchemaRegistryTrackingPolicyProviderTest, DelegateUpdates) {
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"), CreateTestSchema());
  EXPECT_FALSE(schema_registry_.IsReady());
  EXPECT_FALSE(schema_registry_tracking_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));

  PolicyMap policy_map;
  policy_map.Set("foo", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("omg"), nullptr);
  // Chrome policy updates are visible even if the components aren't ready.
  EXPECT_CALL(observer_, OnUpdatePolicy(&schema_registry_tracking_provider_));
  mock_provider_.UpdateChromePolicy(policy_map);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(mock_provider_,
              RefreshPolicies(PolicyFetchReason::kSchemaUpdated));
  schema_registry_.SetAllDomainsReady();
  EXPECT_TRUE(schema_registry_.IsReady());
  Mock::VerifyAndClearExpectations(&mock_provider_);
  EXPECT_FALSE(schema_registry_tracking_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));

  // The provider becomes ready after this refresh completes, and policy updates
  // are visible after that.
  EXPECT_CALL(observer_, OnUpdatePolicy(_));
  mock_provider_.UpdateChromePolicy(policy_map);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(schema_registry_tracking_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));

  // Updates continue to be visible.
  EXPECT_CALL(observer_, OnUpdatePolicy(_));
  mock_provider_.UpdateChromePolicy(policy_map);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(SchemaRegistryTrackingPolicyProviderTest, RemoveAndAddComponent) {
  EXPECT_CALL(mock_provider_,
              RefreshPolicies(PolicyFetchReason::kSchemaUpdated));
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, "xyz");
  schema_registry_.RegisterComponent(ns, CreateTestSchema());
  schema_registry_.SetAllDomainsReady();
  Mock::VerifyAndClearExpectations(&mock_provider_);

  // Serve policy for |ns|.
  PolicyBundle platform_policy;
  platform_policy.Get(ns).Set("foo", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_CLOUD, base::Value("omg"), nullptr);
  PolicyBundle copy = platform_policy.Clone();
  EXPECT_CALL(observer_, OnUpdatePolicy(_));
  mock_provider_.UpdatePolicy(std::move(copy));
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(
      schema_registry_tracking_provider_.policies().Equals(platform_policy));

  // Now remove that component.
  EXPECT_CALL(observer_, OnUpdatePolicy(_));
  schema_registry_.UnregisterComponent(ns);
  Mock::VerifyAndClearExpectations(&observer_);
  const PolicyBundle empty;
  EXPECT_TRUE(schema_registry_tracking_provider_.policies().Equals(empty));

  // Adding it back should serve the current policies again, even though they
  // haven't changed on the platform provider.
  EXPECT_CALL(mock_provider_,
              RefreshPolicies(PolicyFetchReason::kSchemaUpdated));
  schema_registry_.RegisterComponent(ns, CreateTestSchema());
  Mock::VerifyAndClearExpectations(&mock_provider_);

  EXPECT_CALL(observer_, OnUpdatePolicy(_));
  copy = platform_policy.Clone();
  mock_provider_.UpdatePolicy(std::move(copy));
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(
      schema_registry_tracking_provider_.policies().Equals(platform_policy));
}

}  // namespace policy

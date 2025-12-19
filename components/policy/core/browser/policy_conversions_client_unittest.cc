// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_conversions_client.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {
constexpr char kPolicyName1[] = "policy_a";
constexpr char kPolicyName2[] = "policy_b";
constexpr char kPolicyName3[] = "policy_c";
}  // namespace

class StubPolicyConversionsClient : public PolicyConversionsClient {
 public:
  StubPolicyConversionsClient() = default;
  StubPolicyConversionsClient(const StubPolicyConversionsClient&) = delete;
  StubPolicyConversionsClient& operator=(const StubPolicyConversionsClient&) =
      delete;
  ~StubPolicyConversionsClient() override = default;

  void SetPolicyService(PolicyService* policy_service) {
    policy_service_ = policy_service;
  }

 private:
  // PolicyConversionsClient.
  bool HasUserPolicies() const override { return false; }
  base::Value::List GetExtensionPolicies(PolicyDomain policy_domain) override {
    return base::Value::List();
  }
#if BUILDFLAG(IS_CHROMEOS)
  base::Value::List GetDeviceLocalAccountPolicies() override {
    return base::Value::List();
  }
  base::Value::Dict GetIdentityFields() override { return base::Value::Dict(); }
#endif
  PolicyService* GetPolicyService() const override { return policy_service_; }
  SchemaRegistry* GetPolicySchemaRegistry() const override { return nullptr; }
  const ConfigurationPolicyHandlerList* GetHandlerList() const override {
    return nullptr;
  }

  raw_ptr<PolicyService> policy_service_ = nullptr;
};

class PolicyConversionsClientTest : public ::testing::Test {
 public:
  PolicyMap::Entry CreateEntry(bool set_is_default) const {
    PolicyMap::Entry entry(policy::POLICY_LEVEL_MANDATORY,
                           policy::POLICY_SCOPE_MACHINE,
                           policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                           base::Value(base::Value::List()), nullptr);
    if (set_is_default)
      entry.SetIsDefaultValue();
    return entry;
  }

  base::Value::Dict GetPolicyValues(
      const PolicyConversionsClient& client,
      const PolicyMap& map,
      const std::optional<PolicyConversions::PolicyToSchemaMap>&
          known_policy_schemas) const {
    return client.GetPolicyValues(map, nullptr, PoliciesSet(), PoliciesSet(),
                                  known_policy_schemas);
  }
};

// Verify dropping default values option is working.
TEST_F(PolicyConversionsClientTest, SetDropDefaultValues) {
  policy::PolicyMap policy_map;
  policy_map.Set(kPolicyName1, CreateEntry(false /* set_is_default */));
  policy_map.Set(kPolicyName2, CreateEntry(true /* set_is_default */));
  policy_map.Set(kPolicyName3, CreateEntry(false /* set_is_default */));

  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      policy::PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName1, policy::Schema()},
           {kPolicyName2, policy::Schema()},
           {kPolicyName3, policy::Schema()}}};

  StubPolicyConversionsClient client;

  // All policies should exist because |drop_default_values_enabled_| is false
  // by default.
  base::Value::Dict policies1 =
      GetPolicyValues(client, policy_map, policy_schemas);
  EXPECT_EQ(3u, policies1.size());
  EXPECT_NE(nullptr, policies1.FindDict(kPolicyName1));
  EXPECT_NE(nullptr, policies1.FindDict(kPolicyName2));
  EXPECT_NE(nullptr, policies1.FindDict(kPolicyName3));

  // Enable dropping default values.
  client.SetDropDefaultValues(true);
  base::Value::Dict policies2 =
      GetPolicyValues(client, policy_map, policy_schemas);

  // A default valued policy should not exist.
  EXPECT_EQ(2u, policies2.size());
  EXPECT_NE(nullptr, policies2.FindDict(kPolicyName1));
  EXPECT_NE(nullptr, policies2.FindDict(kPolicyName3));
}

TEST_F(PolicyConversionsClientTest, HideMachineValues) {
  policy::PolicyMap policy_map;
  base::Value value("value");
  base::Value hidden_value("********");
  policy_map.Set(kPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, value.Clone(), nullptr);
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_CLOUD, value.Clone(), nullptr);
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      policy::PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName1, policy::Schema()}}};
  StubPolicyConversionsClient client;

  base::Value::Dict policies =
      GetPolicyValues(client, policy_map, policy_schemas);
  EXPECT_EQ(value,
            *policies.FindByDottedPath(base::StrCat({kPolicyName1, ".value"})));
  EXPECT_EQ(value,
            *policies.FindByDottedPath(base::StrCat({kPolicyName2, ".value"})));
  EXPECT_TRUE(
      policies.FindByDottedPath(base::StrCat({kPolicyName2, ".error"})));

  client.EnableShowMachineValues(false);
  policies = GetPolicyValues(client, policy_map, policy_schemas);
  EXPECT_EQ(value,
            *policies.FindByDottedPath(base::StrCat({kPolicyName1, ".value"})));
  EXPECT_EQ(hidden_value,
            *policies.FindByDottedPath(base::StrCat({kPolicyName2, ".value"})));
  EXPECT_FALSE(
      policies.FindByDottedPath(base::StrCat({kPolicyName2, ".error"})));
}

// Test restart required status handling.
TEST_F(PolicyConversionsClientTest, RestartRequired) {
  testing::NiceMock<MockPolicyService> policy_service;
  StubPolicyConversionsClient client;
  client.SetPolicyService(&policy_service);

  // Policy that does not support dynamic refresh.
  const char* kPolicyA = policy::key::kComponentUpdatesEnabled;
  // Policy that supports dynamic refresh.
  const char* kPolicyB = policy::key::kAutofillAddressEnabled;

  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      policy::PolicyConversions::PolicyToSchemaMap{
          {{kPolicyA, policy::Schema()}, {kPolicyB, policy::Schema()}}};

  base::Value value1("value1");
  base::Value value2("value2");

  // 1. New policy without dynamic refresh support should require restart.
  EXPECT_CALL(policy_service, GetInitialChromePolicyValueHash(kPolicyA))
      .WillOnce(testing::Return(std::nullopt));

  PolicyMap current_map1;
  current_map1.Set(kPolicyA, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, value1.Clone(), nullptr);

  base::Value::Dict policies1 =
      GetPolicyValues(client, current_map1, policy_schemas);

  const base::Value::Dict* policy_dict1 = policies1.FindDict(kPolicyA);
  ASSERT_NE(nullptr, policy_dict1);
  EXPECT_TRUE(policy_dict1->FindBool("restartRequired").value_or(false));

  // 2. Changed policy without dynamic refresh support should require restart.
  EXPECT_CALL(policy_service, GetInitialChromePolicyValueHash(kPolicyA))
      .WillOnce(testing::Return(
          std::optional<size_t>(policy::PolicyValueHash(value1.Clone()))));

  PolicyMap current_map2;
  current_map2.Set(kPolicyA, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, value2.Clone(), nullptr);

  base::Value::Dict policies2 =
      GetPolicyValues(client, current_map2, policy_schemas);

  const base::Value::Dict* policy_dict2 = policies2.FindDict(kPolicyA);
  ASSERT_NE(nullptr, policy_dict2);
  EXPECT_TRUE(policy_dict2->FindBool("restartRequired").value_or(false));

  // 3. Unchanged policy without dynamic refresh support should not require
  // restart.
  EXPECT_CALL(policy_service, GetInitialChromePolicyValueHash(kPolicyA))
      .WillOnce(testing::Return(
          std::optional<size_t>(policy::PolicyValueHash(value1.Clone()))));

  PolicyMap current_map3;
  current_map3.Set(kPolicyA, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, value1.Clone(), nullptr);

  base::Value::Dict policies3 =
      GetPolicyValues(client, current_map3, policy_schemas);

  const base::Value::Dict* policy_dict3 = policies3.FindDict(kPolicyA);
  ASSERT_NE(nullptr, policy_dict3);
  EXPECT_FALSE(policy_dict3->FindBool("restartRequired").value_or(false));

  // 4. New policy with dynamic refresh support should not require restart.
  PolicyMap current_map4;
  current_map4.Set(kPolicyB, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, value1.Clone(), nullptr);

  base::Value::Dict policies4 =
      GetPolicyValues(client, current_map4, policy_schemas);

  const base::Value::Dict* policy_dict4 = policies4.FindDict(kPolicyB);
  ASSERT_NE(nullptr, policy_dict4);
  EXPECT_FALSE(policy_dict4->FindBool("restartRequired").value_or(false));
}

// Test policy ignored status handling.
TEST_F(PolicyConversionsClientTest, PolicyIgnoredStatus) {
  PolicyMap policy_map;
  base::Value policy_value("policy_value");
  PolicyMap::Entry entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_ENTERPRISE_DEFAULT,
                         std::make_optional(policy_value.Clone()), nullptr);
  entry.SetIgnored();
  policy_map.Set(kPolicyName1, std::move(entry));

  StubPolicyConversionsClient client;

  base::Value::Dict policies =
      GetPolicyValues(client, policy_map, std::nullopt);

  // Ignored policies should show "ignored" flag.
  const base::Value::Dict* policy_dict = policies.FindDict(kPolicyName1);
  ASSERT_NE(nullptr, policy_dict);
  EXPECT_TRUE(policy_dict->FindBool("ignored").value_or(false));
}

}  // namespace policy

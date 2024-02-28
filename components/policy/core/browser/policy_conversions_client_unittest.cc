// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_conversions_client.h"

#include "base/strings/strcat.h"
#include "components/policy/core/common/policy_map.h"
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

 private:
  // PolicyConversionsClient.
  bool HasUserPolicies() const override { return false; }
  base::Value::List GetExtensionPolicies(PolicyDomain policy_domain) override {
    return base::Value::List();
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Value::List GetDeviceLocalAccountPolicies() override {
    return base::Value::List();
  }
  base::Value::Dict GetIdentityFields() override { return base::Value::Dict(); }
#endif
  PolicyService* GetPolicyService() const override { return nullptr; }
  SchemaRegistry* GetPolicySchemaRegistry() const override { return nullptr; }
  const ConfigurationPolicyHandlerList* GetHandlerList() const override {
    return nullptr;
  }
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

}  // namespace policy

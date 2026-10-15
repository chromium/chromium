// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/policy/core/browser/policy_conversions_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {
namespace {
constexpr char kPolicyName1[] = "policy_a";
constexpr char kPolicyName2[] = "policy_b";
constexpr char kPolicyName3[] = "policy_c";
// Same as `kSensitiveValueMask`, which is not exported from the component.
constexpr char kMaskedValue[] = "********";
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
  void SetHasUserPolicies(bool has_user_policies) {
    has_user_policies_ = has_user_policies;
  }
  void SetSchemaRegistry(SchemaRegistry* schema_registry) {
    schema_registry_ = schema_registry;
  }
  void SetHandlerList(const ConfigurationPolicyHandlerList* handler_list) {
    handler_list_ = handler_list;
  }

 private:
  // PolicyConversionsClient.
  bool HasUserPolicies() const override { return has_user_policies_; }
  base::ListValue GetExtensionPolicies(PolicyDomain policy_domain) override {
    return base::ListValue();
  }
#if BUILDFLAG(IS_CHROMEOS)
  base::ListValue GetDeviceLocalAccountPolicies() override {
    return base::ListValue();
  }
  base::DictValue GetIdentityFields() override { return base::DictValue(); }
#endif
  PolicyService* GetPolicyService() const override { return policy_service_; }
  SchemaRegistry* GetPolicySchemaRegistry() const override {
    return schema_registry_;
  }
  const ConfigurationPolicyHandlerList* GetHandlerList() const override {
    return handler_list_;
  }

  bool has_user_policies_ = false;
  raw_ptr<PolicyService> policy_service_ = nullptr;
  raw_ptr<SchemaRegistry> schema_registry_ = nullptr;
  raw_ptr<const ConfigurationPolicyHandlerList> handler_list_ = nullptr;
};

class PolicyConversionsClientTest : public ::testing::Test {
 public:
  PolicyMap::Entry CreateEntry(bool set_is_default) const {
    PolicyMap::Entry entry(policy::POLICY_LEVEL_MANDATORY,
                           policy::POLICY_SCOPE_MACHINE,
                           policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                           base::Value(base::ListValue()), nullptr);
    if (set_is_default) {
      entry.SetIsDefaultValue();
    }
    return entry;
  }

  base::DictValue GetPolicyValues(
      const PolicyConversionsClient& client,
      const PolicyMap& map,
      const std::optional<PolicyConversions::PolicyToSchemaMap>&
          known_policy_schemas) const {
    return client.GetPolicyValues(map, nullptr, PoliciesSet(), PoliciesSet(),
                                  known_policy_schemas);
  }

  base::flat_map<std::string, mojom::PolicyPtr> GetPolicyValuesMojo(
      const PolicyConversionsClient& client,
      const PolicyMap& map,
      const std::optional<PolicyConversions::PolicyToSchemaMap>&
          known_policy_schemas,
      const PoliciesSet& deprecated_policies = PoliciesSet(),
      const PoliciesSet& future_policies = PoliciesSet(),
      PolicyErrorMap* errors = nullptr) const {
    return client.GetPolicyValuesMojo(map, errors, deprecated_policies,
                                      future_policies, known_policy_schemas);
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
  base::DictValue policies1 =
      GetPolicyValues(client, policy_map, policy_schemas);
  EXPECT_EQ(3u, policies1.size());
  EXPECT_NE(nullptr, policies1.FindDict(kPolicyName1));
  EXPECT_NE(nullptr, policies1.FindDict(kPolicyName2));
  EXPECT_NE(nullptr, policies1.FindDict(kPolicyName3));

  // Enable dropping default values.
  client.SetDropDefaultValues(true);
  base::DictValue policies2 =
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

  base::DictValue policies =
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
  EXPECT_CALL(policy_service, IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(testing::Return(true));

  // Policy that does not support dynamic refresh.
  const char* const kPolicyA = policy::key::kComponentUpdatesEnabled;
  // Policy that supports dynamic refresh.
  const char* const kPolicyB = policy::key::kAutofillAddressEnabled;

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

  base::DictValue policies1 =
      GetPolicyValues(client, current_map1, policy_schemas);

  const base::DictValue* policy_dict1 = policies1.FindDict(kPolicyA);
  ASSERT_NE(nullptr, policy_dict1);
  EXPECT_TRUE(policy_dict1->FindBool("restartRequired").value_or(false));

  // 2. Changed policy without dynamic refresh support should require restart.
  EXPECT_CALL(policy_service, GetInitialChromePolicyValueHash(kPolicyA))
      .WillOnce(testing::Return(
          std::optional<size_t>(policy::PolicyValueHash(value1.Clone()))));

  PolicyMap current_map2;
  current_map2.Set(kPolicyA, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, value2.Clone(), nullptr);

  base::DictValue policies2 =
      GetPolicyValues(client, current_map2, policy_schemas);

  const base::DictValue* policy_dict2 = policies2.FindDict(kPolicyA);
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

  base::DictValue policies3 =
      GetPolicyValues(client, current_map3, policy_schemas);

  const base::DictValue* policy_dict3 = policies3.FindDict(kPolicyA);
  ASSERT_NE(nullptr, policy_dict3);
  EXPECT_FALSE(policy_dict3->FindBool("restartRequired").value_or(false));

  // 4. New policy with dynamic refresh support should not require restart.
  PolicyMap current_map4;
  current_map4.Set(kPolicyB, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, value1.Clone(), nullptr);

  base::DictValue policies4 =
      GetPolicyValues(client, current_map4, policy_schemas);

  const base::DictValue* policy_dict4 = policies4.FindDict(kPolicyB);
  ASSERT_NE(nullptr, policy_dict4);
  EXPECT_FALSE(policy_dict4->FindBool("restartRequired").value_or(false));

  // 5. Policy change before initial snapshot is complete should not require
  // restart.
  testing::NiceMock<MockPolicyService> policy_service_no_snapshot;
  StubPolicyConversionsClient client_no_snapshot;
  client_no_snapshot.SetPolicyService(&policy_service_no_snapshot);
  EXPECT_CALL(policy_service_no_snapshot,
              IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(testing::Return(false));

  PolicyMap current_map5;
  current_map5.Set(kPolicyA, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, value1.Clone(), nullptr);

  base::DictValue policies5 =
      GetPolicyValues(client_no_snapshot, current_map5, policy_schemas);

  const base::DictValue* policy_dict5 = policies5.FindDict(kPolicyA);
  ASSERT_NE(nullptr, policy_dict5);
  EXPECT_FALSE(policy_dict5->FindBool("restartRequired").value_or(false));
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

  base::DictValue policies = GetPolicyValues(client, policy_map, std::nullopt);

  // Ignored policies should show "ignored" flag.
  const base::DictValue* policy_dict = policies.FindDict(kPolicyName1);
  ASSERT_NE(nullptr, policy_dict);
  EXPECT_TRUE(policy_dict->FindBool("ignored").value_or(false));
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo) {
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  policy_map.Set(kPolicyName2, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM, base::Value(1), nullptr);
  // `kPolicyName3` is not in the known schemas, i.e. it's an unknown policy.
  policy_map.Set(kPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName1, Schema()}, {kPolicyName2, Schema()}}};
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, policy_schemas,
                          /*deprecated_policies=*/{kPolicyName2},
                          /*future_policies=*/{kPolicyName3});
  ASSERT_EQ(3u, policies.size());

  const mojom::PolicyPtr& policy1 = policies.at(kPolicyName1);
  EXPECT_EQ(kPolicyName1, policy1->name);
  EXPECT_EQ(base::Value("value"), policy1->value);
  EXPECT_EQ(mojom::PolicyLevel::kMandatory, policy1->level);
  EXPECT_EQ(mojom::PolicyScope::kUser, policy1->scope);
  EXPECT_EQ("cloud", policy1->source);
  EXPECT_FALSE(policy1->ignored);
  EXPECT_FALSE(policy1->deprecated);
  EXPECT_FALSE(policy1->future);
  EXPECT_FALSE(policy1->restart_required);
  EXPECT_FALSE(policy1->all_sources_merged.has_value());
  EXPECT_TRUE(policy1->error.empty());
  EXPECT_TRUE(policy1->conflicts.empty());
  EXPECT_TRUE(policy1->superseded.empty());

  const mojom::PolicyPtr& policy2 = policies.at(kPolicyName2);
  EXPECT_EQ(base::Value(1), policy2->value);
  EXPECT_EQ(mojom::PolicyLevel::kRecommended, policy2->level);
  EXPECT_EQ(mojom::PolicyScope::kMachine, policy2->scope);
  EXPECT_EQ("platform", policy2->source);
  EXPECT_TRUE(policy2->deprecated);
  EXPECT_FALSE(policy2->future);

  const mojom::PolicyPtr& policy3 = policies.at(kPolicyName3);
  EXPECT_TRUE(policy3->future);
  // Unknown policies are reported with an error.
  EXPECT_FALSE(policy3->error.empty());
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_DropDefaultValues) {
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, CreateEntry(/*set_is_default=*/false));
  policy_map.Set(kPolicyName2, CreateEntry(/*set_is_default=*/true));
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName1, Schema()}, {kPolicyName2, Schema()}}};
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, policy_schemas);
  EXPECT_EQ(2u, policies.size());
  EXPECT_EQ("sourceDefault", policies.at(kPolicyName2)->source);

  client.SetDropDefaultValues(true);
  policies = GetPolicyValuesMojo(client, policy_map, policy_schemas);
  EXPECT_EQ(1u, policies.size());
  EXPECT_TRUE(policies.contains(kPolicyName1));
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_HideMachineValues) {
  PolicyMap::Entry user_entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_CLOUD, base::Value("value"),
                              nullptr);
  // Machine values are hidden in conflicts too.
  user_entry.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value("machine"), nullptr));
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, std::move(user_entry));
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{{{kPolicyName1, Schema()}}};
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, policy_schemas);
  EXPECT_EQ(base::Value("machine"),
            policies.at(kPolicyName1)->conflicts[0]->value);
  EXPECT_EQ(base::Value("value"), policies.at(kPolicyName2)->value);
  EXPECT_FALSE(policies.at(kPolicyName2)->error.empty());

  client.EnableShowMachineValues(false);
  policies = GetPolicyValuesMojo(client, policy_map, policy_schemas);
  EXPECT_EQ(base::Value("value"), policies.at(kPolicyName1)->value);
  ASSERT_EQ(1u, policies.at(kPolicyName1)->conflicts.size());
  EXPECT_EQ(base::Value(kMaskedValue),
            policies.at(kPolicyName1)->conflicts[0]->value);
  EXPECT_EQ(base::Value(kMaskedValue), policies.at(kPolicyName2)->value);
  EXPECT_TRUE(policies.at(kPolicyName2)->error.empty());
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_Conflicts) {
  PolicyMap::Entry entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  // Different value: overridden.
  entry.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_PLATFORM, base::Value("other"), nullptr));
  // Same value: superseded.
  entry.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_PLATFORM, base::Value("value"), nullptr));
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, std::move(entry));
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, std::nullopt);
  const mojom::PolicyPtr& policy = policies.at(kPolicyName1);

  ASSERT_EQ(1u, policy->conflicts.size());
  EXPECT_EQ(mojom::PolicyLevel::kRecommended, policy->conflicts[0]->level);
  EXPECT_EQ(mojom::PolicyScope::kMachine, policy->conflicts[0]->scope);
  EXPECT_EQ("platform", policy->conflicts[0]->source);
  EXPECT_EQ(base::Value("other"), policy->conflicts[0]->value);

  ASSERT_EQ(1u, policy->superseded.size());
  EXPECT_EQ(mojom::PolicyLevel::kMandatory, policy->superseded[0]->level);
  EXPECT_EQ(mojom::PolicyScope::kMachine, policy->superseded[0]->scope);
  EXPECT_EQ("platform", policy->superseded[0]->source);
  EXPECT_EQ(base::Value("value"), policy->superseded[0]->value);
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_Ignored) {
  PolicyMap::Entry entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_ENTERPRISE_DEFAULT,
                         base::Value("policy_value"), nullptr);
  entry.SetIgnored();
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, std::move(entry));
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, std::nullopt);
  EXPECT_TRUE(policies.at(kPolicyName1)->ignored);
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_RestartRequired) {
  testing::NiceMock<MockPolicyService> policy_service;
  StubPolicyConversionsClient client;
  client.SetPolicyService(&policy_service);
  ON_CALL(policy_service, IsFirstPolicyLoadComplete(POLICY_DOMAIN_CHROME))
      .WillByDefault(testing::Return(true));
  ON_CALL(policy_service, GetInitialChromePolicyValueHash(testing::_))
      .WillByDefault(testing::Return(std::nullopt));

  // Policy that does not support dynamic refresh.
  const char* const kPolicyA = key::kComponentUpdatesEnabled;
  // Policy that supports dynamic refresh.
  const char* const kPolicyB = key::kAutofillAddressEnabled;
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{
          {{kPolicyA, Schema()}, {kPolicyB, Schema()}}};

  PolicyMap policy_map;
  policy_map.Set(kPolicyA, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  policy_map.Set(kPolicyB, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, policy_schemas);
  EXPECT_TRUE(policies.at(kPolicyA)->restart_required);
  EXPECT_FALSE(policies.at(kPolicyB)->restart_required);
}

// The mojo output is strongly typed, so it must not be affected by the
// `EnableConvertValues()` and `EnableConvertTypes()` formatting options.
TEST_F(PolicyConversionsClientTest,
       GetPolicyValuesMojo_IgnoresConvertValuesAndTypes) {
  base::Value dict_value(base::DictValue().Set("key", "value"));
  base::Value list_value(
      base::ListValue().Append(base::DictValue().Set("key", "value")));

  PolicyMap::Entry entry(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD, dict_value.Clone(), nullptr);
  entry.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_PLATFORM, list_value.Clone(), nullptr));
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, std::move(entry));
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, list_value.Clone(), nullptr);
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName1, Schema()}, {kPolicyName2, Schema()}}};

  StubPolicyConversionsClient client;
  client.EnableConvertValues(true);
  client.EnableConvertTypes(false);

  // Sanity check: the `base::Value` output is converted.
  base::DictValue dict_policies =
      GetPolicyValues(client, policy_map, policy_schemas);
  const base::DictValue* dict_policy1 = dict_policies.FindDict(kPolicyName1);
  ASSERT_NE(nullptr, dict_policy1);
  ASSERT_NE(nullptr, dict_policy1->Find("value"));
  EXPECT_TRUE(dict_policy1->Find("value")->is_string());
  EXPECT_EQ(POLICY_LEVEL_RECOMMENDED, dict_policy1->FindInt("level"));

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, policy_schemas);

  const mojom::PolicyPtr& policy1 = policies.at(kPolicyName1);
  EXPECT_EQ(dict_value, policy1->value);
  EXPECT_EQ(mojom::PolicyLevel::kRecommended, policy1->level);
  EXPECT_EQ(mojom::PolicyScope::kUser, policy1->scope);
  EXPECT_EQ("cloud", policy1->source);
  ASSERT_EQ(1u, policy1->conflicts.size());
  EXPECT_EQ(list_value, policy1->conflicts[0]->value);
  EXPECT_EQ(mojom::PolicyLevel::kMandatory, policy1->conflicts[0]->level);
  EXPECT_EQ(mojom::PolicyScope::kMachine, policy1->conflicts[0]->scope);

  EXPECT_EQ(list_value, policies.at(kPolicyName2)->value);
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_AllSourcesMerged) {
  // All conflicts can be merged into the merged value.
  PolicyMap::Entry merged(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value(base::ListValue().Append("a").Append("b")), nullptr);
  merged.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(base::ListValue().Append("a")), nullptr));
  merged.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
      base::Value(base::ListValue().Append("b")), nullptr));

  // One of the conflicts has a different level, so it can't be merged.
  PolicyMap::Entry partially_merged(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value(base::ListValue().Append("a")), nullptr);
  partially_merged.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(base::ListValue().Append("a")), nullptr));
  partially_merged.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
      base::Value(base::ListValue().Append("b")), nullptr));

  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, std::move(merged));
  policy_map.Set(kPolicyName2, std::move(partially_merged));
  // Not merged at all.
  policy_map.Set(kPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM, base::Value("value"), nullptr);
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, std::nullopt);
  EXPECT_EQ(std::optional<bool>(true),
            policies.at(kPolicyName1)->all_sources_merged);
  EXPECT_EQ(std::optional<bool>(false),
            policies.at(kPolicyName2)->all_sources_merged);
  EXPECT_EQ(std::nullopt, policies.at(kPolicyName3)->all_sources_merged);
}

// Messages come from both the `PolicyMap::Entry` and the `PolicyErrorMap`.
TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_Messages) {
  PolicyMap::Entry entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  entry.AddMessage(PolicyMap::MessageType::kError, IDS_POLICY_BLOCKED);
  entry.AddMessage(PolicyMap::MessageType::kWarning, IDS_POLICY_BLOCKED);
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, std::move(entry));
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);

  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyName1, IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR);
  errors.AddError(kPolicyName1, IDS_POLICY_UNKNOWN, /*error_path=*/{},
                  PolicyMap::MessageType::kInfo);
  errors.AddError(kPolicyName2, IDS_POLICY_UNKNOWN, /*error_path=*/{},
                  PolicyMap::MessageType::kWarning);

  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName1, Schema()}, {kPolicyName2, Schema()}}};
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies = GetPolicyValuesMojo(
      client, policy_map, policy_schemas,
      /*deprecated_policies=*/{}, /*future_policies=*/{}, &errors);

  const mojom::PolicyPtr& policy1 = policies.at(kPolicyName1);
  // Entry and error map messages are joined.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_POLICY_BLOCKED) + "\n" +
                l10n_util::GetStringUTF8(IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR),
            policy1->error);
  // Only the entry has a warning.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_POLICY_BLOCKED), policy1->warning);
  // Only the error map has an info.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_POLICY_UNKNOWN), policy1->info);

  const mojom::PolicyPtr& policy2 = policies.at(kPolicyName2);
  EXPECT_TRUE(policy2->error.empty());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_POLICY_UNKNOWN), policy2->warning);
  EXPECT_TRUE(policy2->info.empty());
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_MasksSensitiveValues) {
  base::expected<Schema, std::string> schema = Schema::Parse(R"(
      {
        "type": "object",
        "properties": {
          "policy_a": { "type": "string", "sensitiveValue": true },
          "policy_b": { "type": "string" }
        }
      })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName1, schema->GetProperty(kPolicyName1)},
           {kPolicyName2, schema->GetProperty(kPolicyName2)}}};

  PolicyMap::Entry entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_CLOUD, base::Value("secret"), nullptr);
  entry.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_PLATFORM, base::Value("other"), nullptr));
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, std::move(entry));
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("public"), nullptr);
  StubPolicyConversionsClient client;

  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, policy_schemas);
  const mojom::PolicyPtr& policy1 = policies.at(kPolicyName1);
  EXPECT_EQ(base::Value(kMaskedValue), policy1->value);
  ASSERT_EQ(1u, policy1->conflicts.size());
  EXPECT_EQ(base::Value(kMaskedValue), policy1->conflicts[0]->value);
  EXPECT_EQ(base::Value("public"), policies.at(kPolicyName2)->value);
}

TEST_F(PolicyConversionsClientTest, GetPolicyValuesMojo_UserPoliciesDisabled) {
  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  StubPolicyConversionsClient client;

  EXPECT_EQ(2u, GetPolicyValuesMojo(client, policy_map, std::nullopt).size());

  client.EnableUserPolicies(false);
  base::flat_map<std::string, mojom::PolicyPtr> policies =
      GetPolicyValuesMojo(client, policy_map, std::nullopt);
  EXPECT_EQ(1u, policies.size());
  EXPECT_TRUE(policies.contains(kPolicyName2));
}

// Covers `CopyAndMaybeConvert()` on the `base::Value` path, which now relies on
// `CopyAndMask()` for masking.
TEST_F(PolicyConversionsClientTest, GetPolicyValues_ConvertValues) {
  base::expected<Schema, std::string> schema = Schema::Parse(R"(
      {
        "type": "object",
        "properties": {
          "policy_c": {
            "type": "object",
            "properties": { "key": { "type": "string" } },
            "sensitiveValue": true
          }
        }
      })");
  ASSERT_TRUE(schema.has_value()) << schema.error();

  base::Value dict_value(base::DictValue().Set("key", "value"));
  base::Value list_value(base::ListValue()
                             .Append(base::DictValue().Set("key", "value"))
                             .Append("string"));

  PolicyMap policy_map;
  policy_map.Set(kPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, dict_value.Clone(), nullptr);
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, list_value.Clone(), nullptr);
  // Sensitive value.
  policy_map.Set(kPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, dict_value.Clone(), nullptr);
  // Hidden machine value.
  constexpr char kMachinePolicy[] = "policy_d";
  policy_map.Set(kMachinePolicy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_CLOUD, dict_value.Clone(), nullptr);
  std::optional<PolicyConversions::PolicyToSchemaMap> policy_schemas =
      PolicyConversions::PolicyToSchemaMap{
          {{kPolicyName3, schema->GetProperty(kPolicyName3)}}};

  StubPolicyConversionsClient client;
  client.EnableConvertValues(true);
  client.EnableShowMachineValues(false);

  base::DictValue policies =
      GetPolicyValues(client, policy_map, policy_schemas);
  const std::string json = client.ConvertValueToJSON(dict_value);
  EXPECT_EQ(base::Value(json),
            *policies.FindByDottedPath(base::StrCat({kPolicyName1, ".value"})));
  EXPECT_EQ(base::Value(base::ListValue().Append(json).Append("string")),
            *policies.FindByDottedPath(base::StrCat({kPolicyName2, ".value"})));
  EXPECT_EQ(base::Value(kMaskedValue),
            *policies.FindByDottedPath(base::StrCat({kPolicyName3, ".value"})));
  EXPECT_EQ(
      base::Value(kMaskedValue),
      *policies.FindByDottedPath(base::StrCat({kMachinePolicy, ".value"})));
}

#if !BUILDFLAG(IS_CHROMEOS)
// The precedence order logic is shared with `GetPrecedenceOrder()`, so only
// check that the mojo output matches it for a non-default order.
TEST_F(PolicyConversionsClientTest, GetPrecedenceOrderMojo) {
  PolicyMap policy_map;
  policy_map.Set(key::kCloudPolicyOverridesPlatformPolicy,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  testing::NiceMock<MockPolicyService> policy_service;
  ON_CALL(policy_service, GetPolicies(testing::_))
      .WillByDefault(testing::ReturnRef(policy_map));
  StubPolicyConversionsClient client;
  client.SetHasUserPolicies(true);
  client.SetPolicyService(&policy_service);

  std::vector<std::string> precedence_order = client.GetPrecedenceOrderMojo();
  ASSERT_FALSE(precedence_order.empty());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_POLICY_PRECEDENCE_CLOUD_MACHINE),
            precedence_order[0]);

  base::ListValue expected = client.GetPrecedenceOrder();
  ASSERT_EQ(expected.size(), precedence_order.size());
  for (size_t i = 0; i < precedence_order.size(); ++i) {
    EXPECT_EQ(expected[i].GetString(), precedence_order[i]);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(PolicyConversionsClientTest, ChromePolicyConversionsToMojoPolicyGroups) {
  PolicyMap policy_map;
  // Wrong type, flagged by the handler below.
  policy_map.Set(kPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
  // Not in the schema, i.e. unknown.
  policy_map.Set(kPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
#if !BUILDFLAG(IS_CHROMEOS)
  policy_map.Set(key::kCloudPolicyOverridesPlatformPolicy,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                 POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS)
  testing::NiceMock<MockPolicyService> policy_service;
  ON_CALL(policy_service, GetPolicies(testing::_))
      .WillByDefault(testing::ReturnRef(policy_map));

  SchemaRegistry schema_registry;
  base::expected<Schema, std::string> schema = Schema::Parse(R"(
      {
        "type": "object",
        "properties": {
          "CloudPolicyOverridesPlatformPolicy": { "type": "boolean" },
          "policy_a": { "type": "boolean" }
        }
      })");
  ASSERT_TRUE(schema.has_value()) << schema.error();
  schema_registry.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), *schema);

  ConfigurationPolicyHandlerList handler_list(
      ConfigurationPolicyHandlerList::PopulatePolicyHandlerParametersCallback(),
      GetChromePolicyDetailsCallback(),
      /*are_future_policies_allowed_by_default=*/false);
  // Reports a type error since the value isn't a boolean. No pref is set since
  // `ApplyPolicySettings()` is called without a `PrefValueMap`.
  handler_list.AddHandler(std::make_unique<SimplePolicyHandler>(
      kPolicyName1, "test.pref", base::Value::Type::BOOLEAN));

  auto client = std::make_unique<StubPolicyConversionsClient>();
  client->SetHasUserPolicies(true);
  client->SetPolicyService(&policy_service);
  client->SetSchemaRegistry(&schema_registry);
  client->SetHandlerList(&handler_list);

  base::flat_map<std::string, mojom::PolicyGroupPtr> groups =
      PolicyConversions(std::move(client))
          .UseChromePolicyConversions()
          .ToMojoPolicyGroups();

  const mojom::PolicyGroupPtr& chrome_group = groups.at(kChromePoliciesId);
  EXPECT_EQ(kChromePoliciesName, chrome_group->name);
  EXPECT_FALSE(chrome_group->precedence_order.has_value());
  EXPECT_FALSE(chrome_group->is_extension);
  EXPECT_FALSE(chrome_group->for_signin_screen);
  const mojom::PolicyPtr& policy1 = chrome_group->policies.at(kPolicyName1);
  EXPECT_EQ(base::Value("value"), policy1->value);
  EXPECT_FALSE(policy1->error.empty());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_POLICY_UNKNOWN),
            chrome_group->policies.at(kPolicyName2)->error);

#if !BUILDFLAG(IS_CHROMEOS)
  ASSERT_EQ(2u, groups.size());
  const mojom::PolicyGroupPtr& precedence_group =
      groups.at(kPrecedencePoliciesId);
  EXPECT_EQ(kPrecedencePoliciesName, precedence_group->name);
  // Only precedence policies are included.
  ASSERT_EQ(1u, precedence_group->policies.size());
  EXPECT_EQ(base::Value(true), precedence_group->policies
                                   .at(key::kCloudPolicyOverridesPlatformPolicy)
                                   ->value);
  ASSERT_TRUE(precedence_group->precedence_order.has_value());
  EXPECT_EQ(4u, precedence_group->precedence_order->size());
#else
  EXPECT_EQ(1u, groups.size());
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

TEST_F(PolicyConversionsClientTest,
       ChromePolicyConversionsToMojoPolicyGroups_NoUserPolicies) {
  auto client = std::make_unique<StubPolicyConversionsClient>();
  client->SetHasUserPolicies(false);

  EXPECT_TRUE(PolicyConversions(std::move(client))
                  .UseChromePolicyConversions()
                  .ToMojoPolicyGroups()
                  .empty());
}

}  // namespace policy

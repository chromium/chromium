// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/enterprise_connectors_policy_handler.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/json/json_reader.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

const char kTestPref[] = "enterprise_connectors.test_pref";

const char kTestScopePref[] = "enterprise_connectors.scope.test_pref";

const char kPolicyName[] = "PolicyForTesting";

const char kSchema[] = R"(
      {
        "type": "object",
        "properties": {
          "PolicyForTesting": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "service_provider": { "type": "string" },
                "enable": { "type": "boolean" },
              }
            }
          }
        }
      })";

constexpr char kEmptyPolicy[] = "";

constexpr char kValidPolicy[] = R"(
    [
      {
        "service_provider": "Google",
        "enable": true,
      },
      {
        "service_provider": "Alphabet",
        "enable": false,
      },
    ])";

// The enable field should be an boolean instead of a string.
constexpr char kInvalidPolicy[] = R"(
    [
      {
        "service_provider": "Google",
        "enable": "yes",
      },
      {
        "service_provider": "Alphabet",
        "enable": "no",
      },
    ])";

constexpr char kValidLocalContentAnalysisPolicy[] = R"(
    [
      {
        "service_provider": "local_user_agent",
        "enable": "yes",
      },
    ])";

constexpr char kInvalidProviderLocalContentAnalysisPolicy[] = R"(
    [
      {
        "service_provider": "google",
        "enable": "yes",
      },
    ])";

constexpr char kFakeProviderLocalContentAnalysisPolicy[] = R"(
    [
      {
        "service_provider": "google",
        "enable": "yes",
      },
    ])";

}  // namespace

class EnterpriseConnectorsPolicyHandlerTestBase {
 public:
  virtual const char* policy() const = 0;

  std::optional<base::Value> policy_value() const {
    return base::JSONReader::Read(policy(), base::JSON_ALLOW_TRAILING_COMMAS);
  }
};

class EnterpriseConnectorsPolicyHandlerCloudOnlyTest
    : public EnterpriseConnectorsPolicyHandlerTestBase,
      public testing::TestWithParam<
          std::tuple<const char*, const char*, policy::PolicySource>> {
 public:
  const char* policy_scope() const { return std::get<0>(GetParam()); }

  const char* policy() const override { return std::get<1>(GetParam()); }

  policy::PolicySource source() const { return std::get<2>(GetParam()); }

  bool expect_valid_policy() const {
    if (policy() == kEmptyPolicy)
      return true;
    if (policy() == kInvalidPolicy)
      return false;
    return source() == policy::PolicySource::POLICY_SOURCE_CLOUD ||
           source() == policy::PolicySource::POLICY_SOURCE_CLOUD_FROM_ASH;
  }
};

TEST_P(EnterpriseConnectorsPolicyHandlerCloudOnlyTest, Test) {
  const auto validation_schema = policy::Schema::Parse(kSchema);
  ASSERT_TRUE(validation_schema.has_value()) << validation_schema.error();

  policy::PolicyMap policy_map;
  if (policy() != kEmptyPolicy) {
    policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                   policy::PolicyScope::POLICY_SCOPE_MACHINE, source(),
                   policy_value(), nullptr);
  }

  auto handler = std::make_unique<EnterpriseConnectorsPolicyHandler>(
      kPolicyName, kTestPref, policy_scope(), *validation_schema);
  policy::PolicyErrorMap errors;
  ASSERT_EQ(expect_valid_policy(),
            handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_EQ(expect_valid_policy(), errors.empty());

  // Apply the pref and check it matches the policy.
  // Real code will not call ApplyPolicySettings if CheckPolicySettings returns
  // false, this is just to test that it applies the pref correctly.
  PrefValueMap prefs;
  base::Value* value_set_in_pref;
  int pref_scope = -1;
  handler->ApplyPolicySettings(policy_map, &prefs);

  bool policy_is_set = policy() != kEmptyPolicy;
  ASSERT_EQ(policy_is_set, prefs.GetValue(kTestPref, &value_set_in_pref));
  if (policy_scope())
    EXPECT_EQ(policy_is_set, prefs.GetInteger(policy_scope(), &pref_scope));

  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  auto* value_set_in_map = policy_map.GetValueUnsafe(kPolicyName);
  if (value_set_in_map) {
    ASSERT_EQ(*value_set_in_map, *value_set_in_pref);
    if (policy_scope())
      ASSERT_EQ(policy::POLICY_SCOPE_MACHINE, pref_scope);
  } else {
    ASSERT_FALSE(policy_is_set);
    if (policy_scope())
      ASSERT_EQ(-1, pref_scope);
  }
}

INSTANTIATE_TEST_SUITE_P(
    EnterpriseConnectorsPolicyHandlerCloudOnlyTest,
    EnterpriseConnectorsPolicyHandlerCloudOnlyTest,
    testing::Combine(
        testing::Values(kTestScopePref, nullptr),
        testing::Values(kValidPolicy, kInvalidPolicy, kEmptyPolicy),
        testing::Values(policy::PolicySource::POLICY_SOURCE_CLOUD,
                        policy::PolicySource::POLICY_SOURCE_CLOUD_FROM_ASH,
                        policy::PolicySource::POLICY_SOURCE_ACTIVE_DIRECTORY,
                        policy::PolicySource::POLICY_SOURCE_PLATFORM)));

class EnterpriseConnectorsPolicyHandlerLocalTest
    : public EnterpriseConnectorsPolicyHandlerTestBase,
      public testing::TestWithParam<std::tuple<const char*, const char*>> {
 public:
  EnterpriseConnectorsPolicyHandlerLocalTest() = default;

  const char* policy() const override { return std::get<0>(GetParam()); }
  const char* policy_pref() const { return std::get<1>(GetParam()); }

  bool policy_is_valid() const {
    if (policy() == kEmptyPolicy)
      return true;

    return false;
  }
};

TEST_P(EnterpriseConnectorsPolicyHandlerLocalTest, Test) {
  const auto validation_schema = policy::Schema::Parse(kSchema);
  ASSERT_TRUE(validation_schema.has_value()) << validation_schema.error();

  policy::PolicyMap policy_map;
  if (policy() != kEmptyPolicy) {
    policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                   policy::PolicyScope::POLICY_SCOPE_MACHINE,
                   policy::PolicySource::POLICY_SOURCE_PLATFORM, policy_value(),
                   nullptr);
  }

  auto handler = std::make_unique<EnterpriseConnectorsPolicyHandler>(
      kPolicyName, policy_pref(), kTestScopePref, *validation_schema);
  policy::PolicyErrorMap errors;
  ASSERT_EQ(policy_is_valid(),
            handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_EQ(policy_is_valid(), errors.empty());
}

INSTANTIATE_TEST_SUITE_P(
    EnterpriseConnectorsPolicyHandlerLocalTest,
    EnterpriseConnectorsPolicyHandlerLocalTest,
    testing::Combine(testing::Values(kValidPolicy,
                                     kInvalidPolicy,
                                     kValidLocalContentAnalysisPolicy,
                                     kInvalidProviderLocalContentAnalysisPolicy,
                                     kFakeProviderLocalContentAnalysisPolicy,
                                     kEmptyPolicy),
                     testing::Values(kOnFileAttachedPref,
                                     kOnFileDownloadedPref,
                                     kOnBulkDataEntryPref,
                                     kOnPrintPref,
#if BUILDFLAG(IS_CHROMEOS)
                                     kOnFileTransferPref,
#endif
                                     kOnSecurityEventPref)));

class EnterpriseConnectorsPolicyHandlerMergeTest
    : public EnterpriseConnectorsPolicyHandlerTestBase,
      public testing::Test {
 public:
  const char* policy() const override { return kValidPolicy; }

  policy::Schema schema() {
    ASSIGN_OR_RETURN(const auto validation_schema,
                     policy::Schema::Parse(kSchema), [](const auto& e) {
                       ADD_FAILURE() << e;
                       return policy::Schema();
                     });
    return validation_schema;
  }

  policy::PolicyMap CreatePolicyMap(policy::PolicySource policy_source) {
    policy::PolicyMap policy_map;

    policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                   policy::PolicyScope::POLICY_SCOPE_MACHINE, policy_source,
                   policy_value(), nullptr);

    return policy_map;
  }
};

TEST_F(EnterpriseConnectorsPolicyHandlerMergeTest, AllowMergedCloudSources) {
  policy::PolicyMap map_merged =
      CreatePolicyMap(policy::PolicySource::POLICY_SOURCE_MERGED);
  policy::PolicyMap map_cloud =
      CreatePolicyMap(policy::PolicySource::POLICY_SOURCE_CLOUD);
  map_merged.MergeFrom(map_cloud);
  // The MERGED source is higher precedence than CLOUD, so its value is set in
  // the combined map. The remaining sources are applied as conflicts.
  EXPECT_EQ(policy::PolicySource::POLICY_SOURCE_MERGED,
            map_merged.Get(kPolicyName)->source);
  EXPECT_EQ(1u, map_merged.Get(kPolicyName)->conflicts.size());

  auto handler = std::make_unique<EnterpriseConnectorsPolicyHandler>(
      kPolicyName, kTestPref, kTestScopePref, schema());
  policy::PolicyErrorMap errors;
  ASSERT_TRUE(handler->CheckPolicySettings(map_merged, &errors));
  ASSERT_TRUE(errors.empty());

  PrefValueMap prefs;
  base::Value* value_set_in_pref;
  handler->ApplyPolicySettings(map_merged, &prefs);
  ASSERT_TRUE(prefs.GetValue(kTestPref, &value_set_in_pref));

  auto* value_set_in_map = map_merged.GetValueUnsafe(kPolicyName);
  ASSERT_TRUE(value_set_in_map);
  ASSERT_EQ(*value_set_in_map, *value_set_in_pref);
}

TEST_F(EnterpriseConnectorsPolicyHandlerMergeTest, BlockMergedNonCloudSources) {
  policy::PolicyMap map_merged =
      CreatePolicyMap(policy::PolicySource::POLICY_SOURCE_MERGED);
  policy::PolicyMap map_cloud =
      CreatePolicyMap(policy::PolicySource::POLICY_SOURCE_CLOUD);
  policy::PolicyMap map_platform =
      CreatePolicyMap(policy::PolicySource::POLICY_SOURCE_PLATFORM);
  map_merged.MergeFrom(map_cloud);
  map_merged.MergeFrom(map_platform);
  // The MERGED source is higher precedence than CLOUD, so its value is set in
  // the combined map. The remaining sources are applied as conflicts.
  EXPECT_EQ(policy::PolicySource::POLICY_SOURCE_MERGED,
            map_merged.Get(kPolicyName)->source);
  EXPECT_EQ(2u, map_merged.Get(kPolicyName)->conflicts.size());

  auto handler = std::make_unique<EnterpriseConnectorsPolicyHandler>(
      kPolicyName, kTestPref, kTestScopePref, schema());
  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(map_merged, &errors));
  ASSERT_FALSE(errors.empty());
  ASSERT_TRUE(errors.HasError(kPolicyName));
  std::u16string messages = errors.GetErrorMessages(kPolicyName);
  ASSERT_EQ(messages,
            u"Ignored because the policy is not set by a cloud source.");
}

}  // namespace enterprise_connectors

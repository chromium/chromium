// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/content/copy_prevention_settings_policy_handler.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/enterprise/content/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kPolicyName[] = "CopyPreventionSettings";

const char kSchema[] = R"(
  {
    "type": "object",
    "properties": {
      "CopyPreventionSettings": {
        "type": "object",
        "properties": {
          "disable": {
            "items": {
              "type": "string"
            },
            "type": "array"
          },
          "enable": {
            "items": {
              "type": "string"
            },
            "type": "array"
          },
          "minimum_data_size": {
            "minimum": 0,
            "type": "integer"
          }
        }
      }
    }
  }
)";

const char kValidPolicy[] = R"(
  {
    "enable": ["*"],
    "disable": ["example.com"],
    "minimum_data_size": 100,
  }
)";

const char kValidWithoutMinDataSizePolicy[] = R"(
  {
    "enable": ["*"],
    "disable": ["example.com"],
  }
)";

const char kNegativeMinDataSizeInvalidPolicy[] = R"(
  {
    "enable": ["*"],
    "disable": ["example.com"],
    "minimum_data_size": -1,
  }
)";

const char kMissingEnableListPolicy[] = R"(
  {
    "disable": ["example.com"],
    "minimum_data_size": 100,
  }
)";

const char kMissingDisableListPolicy[] = R"(
  {
    "enable": ["*"],
    "minimum_data_size": 100,
  }
)";

const char kDisableListWithWildcardInvalidPolicy[] = R"(
  {
    "enable": [],
    "disable": ["*"],
    "minimum_data_size": 100,
  }
)";

class CopyPreventionSettingsPolicyHandlerTest : public testing::Test {
 protected:
  policy::Schema& schema() { return schema_; }

 private:
  void SetUp() override {
    ASSIGN_OR_RETURN(schema_, policy::Schema::Parse(kSchema),
                     [](const auto& e) { ADD_FAILURE() << e; });
  }

  policy::Schema schema_;
};

TEST_F(CopyPreventionSettingsPolicyHandlerTest, TestValidPolicy) {
  policy::PolicyMap policy_map;
  policy_map.Set(
      kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
      policy::PolicyScope::POLICY_SCOPE_MACHINE,
      policy::PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kValidPolicy, base::JSON_ALLOW_TRAILING_COMMAS),
      nullptr);

  auto handler = std::make_unique<CopyPreventionSettingsPolicyHandler>(
      kPolicyName, enterprise::content::kCopyPreventionSettings, schema());

  policy::PolicyErrorMap errors;
  ASSERT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_TRUE(errors.empty());

  PrefValueMap prefs;
  handler->ApplyPolicySettings(policy_map, &prefs);

  base::Value* value_in_pref;
  ASSERT_TRUE(prefs.GetValue(enterprise::content::kCopyPreventionSettings,
                             &value_in_pref));

  const base::Value* value_in_map =
      policy_map.GetValue(kPolicyName, base::Value::Type::DICT);
  ASSERT_EQ(*value_in_pref, *value_in_map);
}

TEST_F(CopyPreventionSettingsPolicyHandlerTest,
       TestValidPolicyNotAppliedIfNotFromCloud) {
  policy::PolicyMap policy_map;
  policy_map.Set(
      kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
      policy::PolicyScope::POLICY_SCOPE_MACHINE,
      policy::PolicySource::POLICY_SOURCE_PLATFORM,
      base::JSONReader::Read(kValidPolicy, base::JSON_ALLOW_TRAILING_COMMAS),
      nullptr);

  auto handler = std::make_unique<CopyPreventionSettingsPolicyHandler>(
      kPolicyName, enterprise::content::kCopyPreventionSettings, schema());

  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_FALSE(errors.empty());
}

TEST_F(CopyPreventionSettingsPolicyHandlerTest,
       TestValidPolicyWithoutMinDataSizeDefaultsTo100) {
  policy::PolicyMap policy_map;
  policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kValidWithoutMinDataSizePolicy,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  auto handler = std::make_unique<CopyPreventionSettingsPolicyHandler>(
      kPolicyName, enterprise::content::kCopyPreventionSettings, schema());

  policy::PolicyErrorMap errors;
  ASSERT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_TRUE(errors.empty());

  PrefValueMap prefs;
  handler->ApplyPolicySettings(policy_map, &prefs);

  base::Value* value_in_pref;
  ASSERT_TRUE(prefs.GetValue(enterprise::content::kCopyPreventionSettings,
                             &value_in_pref));

  std::optional<int> min_data_size = value_in_pref->GetDict().FindInt(
      enterprise::content::kCopyPreventionSettingsMinDataSizeFieldName);
  ASSERT_TRUE(min_data_size);
  ASSERT_EQ(100, *min_data_size);
}

TEST_F(CopyPreventionSettingsPolicyHandlerTest, TestMissingEnableListInvalid) {
  policy::PolicyMap policy_map;
  policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kMissingEnableListPolicy,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  auto handler = std::make_unique<CopyPreventionSettingsPolicyHandler>(
      kPolicyName, enterprise::content::kCopyPreventionSettings, schema());

  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_FALSE(errors.empty());
}

TEST_F(CopyPreventionSettingsPolicyHandlerTest, TestMissingDisableListInvalid) {
  policy::PolicyMap policy_map;
  policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kMissingDisableListPolicy,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  auto handler = std::make_unique<CopyPreventionSettingsPolicyHandler>(
      kPolicyName, enterprise::content::kCopyPreventionSettings, schema());

  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_FALSE(errors.empty());
}

TEST_F(CopyPreventionSettingsPolicyHandlerTest,
       TestDisableListContainingWildcardInvalid) {
  policy::PolicyMap policy_map;
  policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kDisableListWithWildcardInvalidPolicy,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  auto handler = std::make_unique<CopyPreventionSettingsPolicyHandler>(
      kPolicyName, enterprise::content::kCopyPreventionSettings, schema());

  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_FALSE(errors.empty());
}

TEST_F(CopyPreventionSettingsPolicyHandlerTest,
       TestNegativeMinDataSizeInvalid) {
  policy::PolicyMap policy_map;
  policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(kNegativeMinDataSizeInvalidPolicy,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  auto handler = std::make_unique<CopyPreventionSettingsPolicyHandler>(
      kPolicyName, enterprise::content::kCopyPreventionSettings, schema());

  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_FALSE(errors.empty());
}

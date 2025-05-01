// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kGenAiPolicy1Name[] = "GenAiPolicy1";
constexpr char kGenAiPolicy2Name[] = "GenAiPolicy2";
constexpr char kGenAiPolicy3Name[] = "GenAiPolicy3";
constexpr char kGenAiPolicy4Name[] = "GenAiPolicy4";

constexpr char kGenAiPolicy1PrefPath[] = "GenAiPolicy1PrefPath";
constexpr char kGenAiPolicy2PrefPath[] = "GenAiPolicy2PrefPath";
constexpr char kGenAiPolicy3PrefPath[] = "GenAiPolicy3PrefPath";
constexpr char kGenAiPolicy4PrefPath[] = "GenAiPolicy4PrefPath";

constexpr int kDefaultValue = 2;

}  // namespace

class GenAiDefaultSettingsPolicyHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
        gen_ai_default_policies;
    gen_ai_default_policies.emplace_back(kGenAiPolicy1Name,
                                         kGenAiPolicy1PrefPath);
    gen_ai_default_policies.emplace_back(kGenAiPolicy2Name,
                                         kGenAiPolicy2PrefPath);
    gen_ai_default_policies.emplace_back(kGenAiPolicy3Name,
                                         kGenAiPolicy3PrefPath);
    gen_ai_default_policies.emplace_back(kGenAiPolicy4Name,
                                         kGenAiPolicy4PrefPath);
    handler_ = std::make_unique<GenAiDefaultSettingsPolicyHandler>(
        std::move(gen_ai_default_policies));

    policies_.Set(kGenAiPolicy1Name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD, base::Value(1), nullptr);
    policies_.Set(kGenAiPolicy2Name, POLICY_LEVEL_RECOMMENDED,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(1),
                  nullptr);
    policies_.Set(kGenAiPolicy3Name, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                  base::Value(1), nullptr);
  }

  void SetGenAiDefaultPolicy(base::Value value,
                             PolicySource source = POLICY_SOURCE_CLOUD) {
    policies_.Set(key::kGenAiDefaultSettings, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, source, std::move(value), nullptr);
  }

  void ApplyPolicies() { handler_->ApplyPolicySettings(policies_, &prefs_); }

  std::unique_ptr<GenAiDefaultSettingsPolicyHandler> handler_;
  PolicyMap policies_;
  PrefValueMap prefs_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, DefaultUnset) {
  ApplyPolicies();
  EXPECT_TRUE(prefs_.empty());
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, DefaultSet) {
  SetGenAiDefaultPolicy(base::Value(kDefaultValue));
  ApplyPolicies();

  int pref_value;
  // The handler doesn't set prefs whose policies are set in `PolicyMap`.
  EXPECT_FALSE(prefs_.GetInteger(kGenAiPolicy1PrefPath, &pref_value));
  EXPECT_FALSE(prefs_.GetInteger(kGenAiPolicy2PrefPath, &pref_value));
  EXPECT_FALSE(prefs_.GetInteger(kGenAiPolicy3PrefPath, &pref_value));
  // The handler sets prefs whose policies are unset in `PolicyMap`.
  EXPECT_TRUE(prefs_.GetInteger(kGenAiPolicy4PrefPath, &pref_value));
  EXPECT_EQ(kDefaultValue, pref_value);
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, DefaultSetToInvalidValue) {
  SetGenAiDefaultPolicy(base::Value("value"));
  ApplyPolicies();
  EXPECT_TRUE(prefs_.empty());
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, FeatureDisabled) {
  // Disable the feature.
  scoped_feature_list_.InitFromCommandLine(
      /* enable_features= */ "",
      /* disable_features= */ "ApplyGenAiPolicyDefaults");

  SetGenAiDefaultPolicy(base::Value(kDefaultValue));
  ApplyPolicies();
  EXPECT_TRUE(prefs_.empty());
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(GenAiDefaultSettingsPolicyHandlerTest, NotCloud) {
  policy::PolicyErrorMap errors;
  SetGenAiDefaultPolicy(base::Value(kDefaultValue), POLICY_SOURCE_PLATFORM);
  EXPECT_FALSE(handler_->CheckPolicySettings(policies_, &errors));
  SetGenAiDefaultPolicy(base::Value(kDefaultValue));
  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
}
#endif // !BUILDFLAG(IS_CHROMEOS)

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, DefaultControlMessage) {
  SetGenAiDefaultPolicy(base::Value(kDefaultValue));
  policy::PolicyErrorMap errors;

  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(errors.GetErrorMessages(key::kGenAiDefaultSettings,
                                    policy::PolicyMap::MessageType::kInfo),
            u"This policy is controlling the default behavior of the following "
            u"policies: GenAiPolicy4");
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, NoDefaultControlWarning) {
  SetGenAiDefaultPolicy(base::Value(kDefaultValue));
  policy::PolicyErrorMap errors;

  // Setting `GenAiPolicy4` so that the metapolicy has no policies to control.
  policies_.Set(kGenAiPolicy4Name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_PLATFORM, base::Value(1), nullptr);
  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(errors.GetErrorMessages(key::kGenAiDefaultSettings,
                                    policy::PolicyMap::MessageType::kInfo),
            u"This policy is not controlling the default behavior of any other "
            u"policies.");
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, IntToIntMapping_FeatureDisabled) {
  // Disable the feature.
  scoped_feature_list_.InitFromCommandLine(
      /* enable_features= */ "",
      /* disable_features= */ "GenAiPolicyDefaultsUsePrefMap");

  // Create a mapping: 0 -> 1, 1 -> 2, 2->3.
  GenAiDefaultSettingsPolicyHandler::PolicyValueToPrefMap
      policy_value_to_pref_map;
  policy_value_to_pref_map.emplace(0, 1);
  policy_value_to_pref_map.emplace(1, 2);
  policy_value_to_pref_map.emplace(2, 3);

  // Set up handler with a policy that uses the policy_value_to_pref_map.
  std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
      gen_ai_default_policies;
  gen_ai_default_policies.emplace_back(kGenAiPolicy4Name, kGenAiPolicy4PrefPath,
                                       std::move(policy_value_to_pref_map));
  handler_ = std::make_unique<GenAiDefaultSettingsPolicyHandler>(
      std::move(gen_ai_default_policies));

  // Set default value to 0.
  SetGenAiDefaultPolicy(base::Value(0));

  // Since feature is disabled, `GenAiPolicy4` should not be controlled by
  // metapolicy.
  policy::PolicyErrorMap errors;
  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(errors.GetErrorMessages(key::kGenAiDefaultSettings,
                                    policy::PolicyMap::MessageType::kInfo),
            u"This policy is not controlling the default behavior of any other "
            u"policies.");

  // Check that prefs do not have Policy4 set.
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(prefs_.GetValue(kGenAiPolicy4PrefPath, &value));
  EXPECT_FALSE(value);
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest, IntToIntMapping_FeaturedEnabled) {
  // Enable the feature.
  scoped_feature_list_.InitFromCommandLine(
      /* enable_features= */ "GenAiPolicyDefaultsUsePrefMap",
      /* disable_features= */ "");

  // Create a mapping: 0 -> 1, 1 -> 2, 2->3.
  GenAiDefaultSettingsPolicyHandler::PolicyValueToPrefMap
      policy_value_to_pref_map;
  policy_value_to_pref_map.emplace(0, 1);
  policy_value_to_pref_map.emplace(1, 2);
  policy_value_to_pref_map.emplace(2, 3);

  // Set up handler with a policy that uses the policy_value_to_pref_map.
  std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
      gen_ai_default_policies;
  gen_ai_default_policies.emplace_back(kGenAiPolicy4Name, kGenAiPolicy4PrefPath,
                                       std::move(policy_value_to_pref_map));
  handler_ = std::make_unique<GenAiDefaultSettingsPolicyHandler>(
      std::move(gen_ai_default_policies));

  // Set default value to 0, which should map to true for `GenAiPolicy4`.
  SetGenAiDefaultPolicy(base::Value(0));

  // Since feature is enabled, `GenAiPolicy4` should be controlled by
  // metapolicy.
  policy::PolicyErrorMap errors;
  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(errors.GetErrorMessages(key::kGenAiDefaultSettings,
                                    policy::PolicyMap::MessageType::kInfo),
            u"This policy is controlling the default behavior of the following "
            u"policies: GenAiPolicy4");

  // Check that prefs match the expected map value.
  ApplyPolicies();
  int pref_value;
  EXPECT_TRUE(prefs_.GetInteger(kGenAiPolicy4PrefPath, &pref_value));
  EXPECT_EQ(1, pref_value);
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest,
       IntToIntMappingUnsupportedKey_FeaturedEnabled) {
  // Enable the feature.
  scoped_feature_list_.InitFromCommandLine(
      /* enable_features= */ "GenAiPolicyDefaultsUsePrefMap",
      /* disable_features= */ "");

  // Create a mapping: 0 -> 1, 1 -> 2, 2->3.
  GenAiDefaultSettingsPolicyHandler::PolicyValueToPrefMap
      policy_value_to_pref_map;
  policy_value_to_pref_map.emplace(0, 1);
  policy_value_to_pref_map.emplace(1, 2);
  policy_value_to_pref_map.emplace(2, 3);

  // Set up handler with a policy that uses the policy_value_to_pref_map.
  std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
      gen_ai_default_policies;
  gen_ai_default_policies.emplace_back(kGenAiPolicy4Name, kGenAiPolicy4PrefPath,
                                       std::move(policy_value_to_pref_map));
  handler_ = std::make_unique<GenAiDefaultSettingsPolicyHandler>(
      std::move(gen_ai_default_policies));

  // Set default value to 3, which does not exist in the map for `GenAiPolicy4`
  // and is not yet supported.
  SetGenAiDefaultPolicy(base::Value(3));

  // Since feature is enabled, `GenAiPolicy4` should be controlled by
  // metapolicy.
  policy::PolicyErrorMap errors;
  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(errors.GetErrorMessages(key::kGenAiDefaultSettings,
                                    policy::PolicyMap::MessageType::kInfo),
            u"This policy is controlling the default behavior of the following "
            u"policies: GenAiPolicy4");

  // Check that prefs do not have Policy4 set.
  ApplyPolicies();
  const base::Value* value = nullptr;
  EXPECT_FALSE(prefs_.GetValue(kGenAiPolicy4PrefPath, &value));
  EXPECT_FALSE(value);
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest,
       IntToIntMappingMissingKey_FeatureDisabled) {
  // Enable the feature.
  scoped_feature_list_.InitFromCommandLine(
      /* enable_features= */ "",
      /* disable_features= */ "GenAiPolicyDefaultsUsePrefMap");

  // Create a mapping: 0 -> 1, 1 -> 2. Policy value 2 is not mapped.
  GenAiDefaultSettingsPolicyHandler::PolicyValueToPrefMap
      policy_value_to_pref_map;
  policy_value_to_pref_map.emplace(0, 1);
  policy_value_to_pref_map.emplace(1, 2);

  // Try to set up handler with a policy that uses the policy_value_to_pref_map.
  // Do not expect a crash since the feature is disabled.
  std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
      gen_ai_default_policies;
  gen_ai_default_policies.emplace_back(kGenAiPolicy4Name, kGenAiPolicy4PrefPath,
                                       std::move(policy_value_to_pref_map));
  handler_ = std::make_unique<GenAiDefaultSettingsPolicyHandler>(
      std::move(gen_ai_default_policies));

  // Set default value to 0.
  SetGenAiDefaultPolicy(base::Value(0));

  // Since feature is disabled, `GenAiPolicy4` should not be controlled by
  // metapolicy and should not crash.
  policy::PolicyErrorMap errors;
  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(errors.GetErrorMessages(key::kGenAiDefaultSettings,
                                    policy::PolicyMap::MessageType::kInfo),
            u"This policy is not controlling the default behavior of any other "
            u"policies.");
}

TEST_F(GenAiDefaultSettingsPolicyHandlerTest,
       IntToIntMappingEmptyMap_FeatureDisabled) {
  // Enable the feature.
  scoped_feature_list_.InitFromCommandLine(
      /* enable_features= */ "",
      /* disable_features= */ "GenAiPolicyDefaultsUsePrefMap");

  // Create an empty mapping.
  GenAiDefaultSettingsPolicyHandler::PolicyValueToPrefMap
      policy_value_to_pref_map;

  // Try to set up handler with a policy that uses the policy_value_to_pref_map.
  std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
      gen_ai_default_policies;
  gen_ai_default_policies.emplace_back(kGenAiPolicy4Name, kGenAiPolicy4PrefPath,
                                       std::move(policy_value_to_pref_map));
  handler_ = std::make_unique<GenAiDefaultSettingsPolicyHandler>(
      std::move(gen_ai_default_policies));

  // Set default value to 0.
  SetGenAiDefaultPolicy(base::Value(0));

  // Since the map is empty, `GenAiPolicy4` should be controlled by
  // metapolicy, similar to a policy without a map.
  policy::PolicyErrorMap errors;
  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors));
  EXPECT_EQ(errors.GetErrorMessages(key::kGenAiDefaultSettings,
                                    policy::PolicyMap::MessageType::kInfo),
            u"This policy is controlling the default behavior of the following "
            u"policies: GenAiPolicy4");

  // Check that prefs match the expected map value.
  ApplyPolicies();
  int pref_value;
  EXPECT_TRUE(prefs_.GetInteger(kGenAiPolicy4PrefPath, &pref_value));
  EXPECT_EQ(0, pref_value);
}

}  // namespace policy

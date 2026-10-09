// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_policy_handler.h"

#include <memory>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;

// Test cases for the Autofill policy setting.
class AutofillPolicyHandlerTest : public testing::Test {};

TEST_F(AutofillPolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillEnabledDeprecated, nullptr));
}

TEST_F(AutofillPolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutoFillEnabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(true), nullptr);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Autofill should not set the pref. Profile and credit card Autofill
  // prefs should also not get set.
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillEnabledDeprecated, nullptr));
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillProfileEnabled, nullptr));
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillCreditCardEnabled, nullptr));
}

TEST_F(AutofillPolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutoFillEnabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Disabling Autofill by policy should set the pref.
  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillEnabledDeprecated, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());

  // Disabling Autofill by policy should set the profile Autofill pref.
  value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillProfileEnabled, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());

  // Disabling Autofill by policy should set the credit card Autofill pref.
  value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillCreditCardEnabled, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());
}

TEST_F(AutofillPolicyHandlerTest, DeprecatedPolicyIgnored_AddressEnabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutoFillEnabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(false), nullptr);
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Settings either of the fine-grained policies should cause the old policy to
  // be ignored. The fine-grained policies should not get set by this handler
  // either.
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillEnabledDeprecated, nullptr));
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillProfileEnabled, nullptr));
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillCreditCardEnabled, nullptr));
}

TEST_F(AutofillPolicyHandlerTest, DeprecatedPolicyIgnored_CreditCardEnabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutoFillEnabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(false), nullptr);
  policy.Set(policy::key::kAutofillCreditCardEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Settings either of the fine-grained policies should cause the old policy to
  // be ignored. The fine-grained policies should not get set by this handler
  // either.
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillEnabledDeprecated, nullptr));
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillProfileEnabled, nullptr));
  EXPECT_FALSE(prefs.GetValue(prefs::kAutofillCreditCardEnabled, nullptr));
}

TEST_F(AutofillPolicyHandlerTest, CheckPolicySettings_AllPoliciesWrongType) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value("true"), nullptr);
  policy.Set(policy::key::kAutofillCreditCardEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(123), nullptr);
  // Set kAutofillSettings with invalid schema (integer instead of list).
  policy.Set(policy::key::kAutofillSettings, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(999), nullptr);

  AutofillSettingsPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  policy::PolicyErrorMap errors;

  EXPECT_FALSE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.HasError(policy::key::kAutofillAddressEnabled));
  EXPECT_TRUE(errors.HasError(policy::key::kAutofillCreditCardEnabled));
  EXPECT_TRUE(errors.HasError(policy::key::kAutofillSettings));
}

TEST_F(AutofillPolicyHandlerTest, CheckPolicySettings_AllPoliciesCorrectType) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy.Set(policy::key::kAutofillCreditCardEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy.Set(
      policy::key::kAutofillSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(base::ListValue().Append(
          base::DictValue()
              .Set("url_pattern", "https://example.com")
              .Set("blocked_types", base::ListValue().Append("travel")))),
      nullptr);

  AutofillSettingsPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  policy::PolicyErrorMap errors;

  EXPECT_TRUE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(AutofillPolicyHandlerTest,
       CheckPolicySettings_InvalidListEntriesAllowedWithWarning) {
  policy::PolicyMap policy;
  policy.Set(
      policy::key::kAutofillSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(
          base::ListValue()
              .Append(base::DictValue()
                          .Set("url_pattern", "invalid-url-pattern-123")
                          .Set("blocked_types",
                               base::ListValue().Append("payments")))
              .Append(base::DictValue()
                          .Set("url_pattern", "https://[*.]thenorthface.com")
                          .Set("blocked_types",
                               base::ListValue()
                                   .Append("contact_info")
                                   .Append("unknown_future_category")))
              .Append(base::DictValue().Set(
                  "blocked_types", base::ListValue().Append("travel")))),
      nullptr);

  AutofillSettingsPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  policy::PolicyErrorMap errors;

  EXPECT_TRUE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_TRUE(errors.HasError(policy::key::kAutofillSettings));
  EXPECT_FALSE(errors.HasFatalError(policy::key::kAutofillSettings));

  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);
  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillTypesBlocked, &value));
  ASSERT_TRUE(value && value->is_list());
  EXPECT_EQ(value->GetList().size(), 3u);
}

TEST_F(AutofillPolicyHandlerTest, MigrationHandler_AddressDisabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillSettingsPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  handler.ApplyPolicySettings(policy, &prefs);

  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillProfileEnabled, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());

  value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillTypesBlocked, &value));
  ASSERT_TRUE(value && value->is_list());
  EXPECT_THAT(value->GetList(), ElementsAre(base::test::IsJson(R"({
            "blocked_types": ["contact_info"],
            "url_pattern": "*"
          })")));
}

TEST_F(AutofillPolicyHandlerTest, MigrationHandler_CreditCardDisabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillCreditCardEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillSettingsPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  handler.ApplyPolicySettings(policy, &prefs);

  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillCreditCardEnabled, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());

  value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillTypesBlocked, &value));
  ASSERT_TRUE(value && value->is_list());
  EXPECT_THAT(value->GetList(), ElementsAre(base::test::IsJson(R"({
            "blocked_types": ["payments"],
            "url_pattern": "*"
          })")));
}

TEST_F(AutofillPolicyHandlerTest, MigrationHandler_BothDisabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy.Set(policy::key::kAutofillCreditCardEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  PrefValueMap prefs;
  AutofillSettingsPolicyHandler handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  handler.ApplyPolicySettings(policy, &prefs);

  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillTypesBlocked, &value));
  ASSERT_TRUE(value && value->is_list());
  EXPECT_THAT(value->GetList(), ElementsAre(base::test::IsJson(R"({
            "blocked_types": ["contact_info", "payments"],
            "url_pattern": "*"
          })")));
}

TEST_F(AutofillPolicyHandlerTest,
       ConfiguredAutofillSettingsMergesWithLegacyPolicies) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutofillAddressEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);

  policy.Set(
      policy::key::kAutofillSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(base::ListValue().Append(
          base::DictValue()
              .Set("url_pattern", "https://example.com")
              .Set("blocked_types", base::ListValue().Append("travel")))),
      nullptr);

  PrefValueMap prefs;
  AutofillSettingsPolicyHandler migration_handler(
      policy::Schema::Wrap(policy::GetChromeSchemaData()));
  migration_handler.ApplyPolicySettings(policy, &prefs);

  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillProfileEnabled, nullptr));

  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kAutofillTypesBlocked, &value));
  ASSERT_TRUE(value && value->is_list());
  EXPECT_THAT(value->GetList(), ElementsAre(base::test::IsJson(R"({
            "blocked_types": ["travel"],
            "url_pattern": "https://example.com"
          })"),
                                            base::test::IsJson(R"({
            "blocked_types": ["contact_info"],
            "url_pattern": "*"
          })")));
}

}  // namespace
}  // namespace autofill

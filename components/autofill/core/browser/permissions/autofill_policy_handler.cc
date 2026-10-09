// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_policy_handler.h"

#include "base/values.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace autofill {

namespace {

// Keys used in the dictionaries stored in `kAutofillTypesBlocked`.
// Must remain identical to `kUrlPatternKey` and `kBlockedTypesKey` in
// `AutofillEnterprisePolicyService` (autofill_enterprise_policy_service.cc).
constexpr char kUrlPatternKey[] = "url_pattern";
constexpr char kBlockedTypesKey[] = "blocked_types";

// Supported values for the blocked data categories in `kAutofillTypesBlocked`.
// Must remain identical to `kContactInfoValue` and `kPaymentsValue` in
// `AutofillEnterprisePolicyService` (autofill_enterprise_policy_service.cc).
constexpr char kContactInfoValue[] = "contact_info";
constexpr char kPaymentsValue[] = "payments";

bool IsPolicySetToFalse(const base::Value* value) {
  return value && value->GetIfBool() == false;
}

}  // namespace

AutofillPolicyHandler::AutofillPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kAutoFillEnabled,
                                        base::Value::Type::BOOLEAN) {}

AutofillPolicyHandler::~AutofillPolicyHandler() = default;

void AutofillPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* autofill_credit_card_policy_value = policies.GetValue(
      policy::key::kAutofillCreditCardEnabled, base::Value::Type::BOOLEAN);
  const base::Value* autofill_address_policy_value = policies.GetValue(
      policy::key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN);
  // Ignore the old policy if either of the new fine-grained policies are set.
  if (autofill_credit_card_policy_value || autofill_address_policy_value) {
    return;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (IsPolicySetToFalse(value)) {
    prefs->SetBoolean(prefs::kAutofillEnabledDeprecated, false);
    // Disable the fine-grained prefs if the main pref is disabled by policy.
    prefs->SetBoolean(prefs::kAutofillCreditCardEnabled, false);
    prefs->SetBoolean(prefs::kAutofillProfileEnabled, false);
  }
}

AutofillSettingsPolicyHandler::AutofillSettingsPolicyHandler(
    policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy::key::kAutofillSettings,
          prefs::kAutofillTypesBlocked,
          schema,
          policy::SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY,
          policy::SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          policy::SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

AutofillSettingsPolicyHandler::~AutofillSettingsPolicyHandler() = default;

bool AutofillSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const bool address_enabled_valid =
      policy::TypeCheckingPolicyHandler::CheckPolicySettings(
          policy::key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN,
          policies.Get(policy::key::kAutofillAddressEnabled), errors);
  const bool credit_card_enabled_valid =
      policy::TypeCheckingPolicyHandler::CheckPolicySettings(
          policy::key::kAutofillCreditCardEnabled, base::Value::Type::BOOLEAN,
          policies.Get(policy::key::kAutofillCreditCardEnabled), errors);
  const bool autofill_settings_valid =
      policy::SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(policies,
                                                                       errors);

  return address_enabled_valid && credit_card_enabled_valid &&
         autofill_settings_valid;
}

void AutofillSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // 1. BACKWARD COMPATIBILITY: Map legacy policies to traditional preferences.
  // We check if the legacy address/credit card policies are set to false. If
  // so, we disable the corresponding profile and credit card preferences.
  const base::Value* legacy_address_enabled = policies.GetValue(
      policy::key::kAutofillAddressEnabled, base::Value::Type::BOOLEAN);
  if (IsPolicySetToFalse(legacy_address_enabled)) {
    prefs->SetBoolean(prefs::kAutofillProfileEnabled, false);
  }

  const base::Value* legacy_credit_card_enabled = policies.GetValue(
      policy::key::kAutofillCreditCardEnabled, base::Value::Type::BOOLEAN);
  if (IsPolicySetToFalse(legacy_credit_card_enabled)) {
    prefs->SetBoolean(prefs::kAutofillCreditCardEnabled, false);
  }

  // 2. CONSOLIDATING BLOCKED TYPES: Map disabled legacy policies to wildcard
  // block rules. Because the AutofillSettings policy enforces block rules under
  // `kAutofillTypesBlocked`, we must translate a disabled legacy boolean policy
  // into a global wildcard block rule ("*") targeting its corresponding data
  // type.
  base::ListValue blocked_types;
  if (IsPolicySetToFalse(legacy_address_enabled)) {
    blocked_types.Append(kContactInfoValue);
  }
  if (IsPolicySetToFalse(legacy_credit_card_enabled)) {
    blocked_types.Append(kPaymentsValue);
  }

  // 3. MERGING RULES: Combine AutofillSettings rules with synthesized wildcard
  // rules. We read the list of site-specific block rules configured under the
  // new AutofillSettings policy, and append any synthesized wildcard block
  // rules created from the legacy policies above.
  base::ListValue merged_rules;
  if (const base::Value* autofill_settings =
          policies.GetValue(policy_name(), base::Value::Type::LIST)) {
    for (const auto& item : autofill_settings->GetList()) {
      merged_rules.Append(item.Clone());
    }
  }

  if (!blocked_types.empty()) {
    merged_rules.Append(base::DictValue()
                            .Set(kUrlPatternKey, "*")
                            .Set(kBlockedTypesKey, std::move(blocked_types)));
  }

  if (!merged_rules.empty()) {
    prefs->SetValue(prefs::kAutofillTypesBlocked,
                    base::Value(std::move(merged_rules)));
  }
}

}  // namespace autofill

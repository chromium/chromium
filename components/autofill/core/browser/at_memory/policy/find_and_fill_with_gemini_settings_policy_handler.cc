// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/policy/find_and_fill_with_gemini_settings_policy_handler.h"

#include "base/values.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

FindAndFillWithGeminiSettingsPolicyHandler::
    FindAndFillWithGeminiSettingsPolicyHandler(
        std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
            gen_ai_default_settings_policy_handler)
    : IntRangePolicyHandler(
          key::kFindAndFillWithGeminiSettings,
          optimization_guide::prefs::kFindAndFillWithGeminiSettings,
          /*min=*/0,
          /*max=*/2,
          /*clamp_=*/false),
      gen_ai_default_settings_policy_handler_(
          std::move(gen_ai_default_settings_policy_handler)),
      gemini_settings_policy_handler_(std::make_unique<SimplePolicyHandler>(
          key::kGeminiSettings,
          optimization_guide::prefs::kGeminiSettings,
          base::Value::Type::INTEGER)) {}

FindAndFillWithGeminiSettingsPolicyHandler::
    ~FindAndFillWithGeminiSettingsPolicyHandler() = default;

bool FindAndFillWithGeminiSettingsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* find_and_fill_policy =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);

  if (!find_and_fill_policy) {
    return true;
  }

  if (!IntRangePolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  // 0 = Allowed, 1 = AllowedWithoutLogging, 2 = Disabled.
  const bool find_and_fill_enabled = find_and_fill_policy->GetInt() != 2;

  if (find_and_fill_enabled) {
    PrefValueMap prefs;
    if (gemini_settings_policy_handler_->CheckPolicySettings(policies,
                                                             errors)) {
      gemini_settings_policy_handler_->ApplyPolicySettings(policies, &prefs);
    }
    if (gen_ai_default_settings_policy_handler_->CheckPolicySettings(policies,
                                                                     errors)) {
      gen_ai_default_settings_policy_handler_->ApplyPolicySettings(policies,
                                                                   &prefs);
    }
    int gemini_settings_pref_value = -1;
    prefs.GetInteger(optimization_guide::prefs::kGeminiSettings,
                     &gemini_settings_pref_value);

    const bool gemini_disabled = gemini_settings_pref_value == 1;
    if (gemini_disabled) {
      if (errors) {
        // We do not return `false` here because we still want
        // `ApplyPolicySettings` to run and set `kFindAndFillWithGeminiSettings`
        // to disabled.
        errors->AddError(policy_name(), IDS_POLICY_DEPENDENCY_ERROR,
                         "GeminiSettings", "Enabled");
      }
    }
  }

  return true;
}

void FindAndFillWithGeminiSettingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  PrefValueMap temp_prefs;
  PolicyErrorMap errors;
  if (gemini_settings_policy_handler_->CheckPolicySettings(policies, &errors)) {
    gemini_settings_policy_handler_->ApplyPolicySettings(policies, &temp_prefs);
  }
  if (gen_ai_default_settings_policy_handler_->CheckPolicySettings(policies,
                                                                   &errors)) {
    gen_ai_default_settings_policy_handler_->ApplyPolicySettings(policies,
                                                                 &temp_prefs);
  }
  int gemini_settings_pref_value = -1;
  temp_prefs.GetInteger(optimization_guide::prefs::kGeminiSettings,
                        &gemini_settings_pref_value);

  // If Gemini is disabled by policy (kGeminiSettings == 1), force
  // FindAndFillWithGeminiSettings pref to disabled (2).
  if (gemini_settings_pref_value == 1) {
    prefs->SetInteger(optimization_guide::prefs::kFindAndFillWithGeminiSettings,
                      2);
    return;
  }

  IntRangePolicyHandler::ApplyPolicySettings(policies, prefs);
}

}  // namespace policy

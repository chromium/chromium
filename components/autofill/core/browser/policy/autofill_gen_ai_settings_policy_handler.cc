// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/policy/autofill_gen_ai_settings_policy_handler.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

namespace {

// Value of the `kGeminiSettings` pref that means the Gemini app is disabled.
constexpr int kGeminiSettingsDisabled = 1;

// Value of `AutofillGenAiSettings` and `GenAiDefaultSettings` that disables the
// features.
constexpr int kAutofillGenAiDisabled = 2;

}  // namespace

AutofillGenAiSettingsPolicyHandler::AutofillGenAiSettingsPolicyHandler(
    std::unique_ptr<GenAiDefaultSettingsPolicyHandler>
        gen_ai_default_settings_policy_handler)
    : IntRangePolicyHandler(
          key::kAutofillGenAiSettings,
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

AutofillGenAiSettingsPolicyHandler::~AutofillGenAiSettingsPolicyHandler() =
    default;

bool AutofillGenAiSettingsPolicyHandler::IsGeminiDisabled(
    const PolicyMap& policies) {
  PrefValueMap prefs;
  if (gemini_settings_policy_handler_->CheckPolicySettings(
          policies, /*errors=*/nullptr)) {
    gemini_settings_policy_handler_->ApplyPolicySettings(policies, &prefs);
  }
  // `GenAiDefaultSettingsPolicyHandler::CheckPolicySettings` unconditionally
  // dereferences `errors` to record info messages, so pass a scratch map that
  // is discarded here.
  PolicyErrorMap scratch_errors;
  if (gen_ai_default_settings_policy_handler_->CheckPolicySettings(
          policies, &scratch_errors)) {
    gen_ai_default_settings_policy_handler_->ApplyPolicySettings(policies,
                                                                 &prefs);
  }
  int gemini_settings_pref_value = -1;
  prefs.GetInteger(optimization_guide::prefs::kGeminiSettings,
                   &gemini_settings_pref_value);
  return gemini_settings_pref_value == kGeminiSettingsDisabled;
}

void AutofillGenAiSettingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  // AtMemory and Ambient Autofill. This runs even when `AutofillGenAiSettings`
  // is unset, so that disabling the Gemini app always disables these features.
  if (IsGeminiDisabled(policies)) {
    prefs->SetInteger(optimization_guide::prefs::kFindAndFillWithGeminiSettings,
                      kAutofillGenAiDisabled);
  } else {
    IntRangePolicyHandler::ApplyPolicySettings(policies, prefs);
  }

  // "Smarter form understanding". Unlike the features above, this does not go
  // through the Gemini app, so it is not affected by `GeminiSettings`.
  if (const base::Value* autofill_gen_ai_policy =
          policies.GetValue(policy_name(), base::Value::Type::INTEGER)) {
    prefs->SetInteger(
        optimization_guide::prefs::
            kAutofillPredictionImprovementsEnterprisePolicyAllowed,
        autofill_gen_ai_policy->GetInt());
  }
}

}  // namespace policy

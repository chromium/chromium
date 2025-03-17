// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

// Kill switch for applying default values to the covered GenAI policies.
BASE_FEATURE(kApplyGenAiPolicyDefaults,
             "ApplyGenAiPolicyDefaults",
             base::FEATURE_ENABLED_BY_DEFAULT);

GenAiDefaultSettingsPolicyHandler::GenAiDefaultSettingsPolicyHandler(
    std::vector<GenAiPolicyDetails>&& gen_ai_policies)
    : TypeCheckingPolicyHandler(key::kGenAiDefaultSettings,
                                base::Value::Type::INTEGER),
      gen_ai_policies_(std::move(gen_ai_policies)) {}

GenAiDefaultSettingsPolicyHandler::~GenAiDefaultSettingsPolicyHandler() =
    default;

bool GenAiDefaultSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!base::FeatureList::GetInstance() ||
      !base::FeatureList::IsEnabled(kApplyGenAiPolicyDefaults)) {
    return true;
  }

  if (!policies.IsPolicySet(policy_name())) {
    return true;
  }

  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (!CloudOnlyPolicyHandler::CheckCloudOnlyPolicySettings(policy_name(),
                                                            policies, errors)) {
    return false;
  }
#endif // !BUILDFLAG(IS_CHROMEOS)

  // If no GenAI policies are being controlled by this policy, add a warning so
  // admins can take action.
  auto unset_gen_ai_policies = GetUnsetGenAiPolicies(policies);
  if (unset_gen_ai_policies.empty()) {
    errors->AddError(policy_name(),
                     IDS_POLICY_GEN_AI_DEFAULT_SETTINGS_NO_CONTROL_MESSAGE,
                     /*error_path=*/{}, PolicyMap::MessageType::kInfo);
    return true;
  }

  // Add info message to the policy describing which GenAI policies have their
  // default behavior controlled by GenAiDefaultSettings.
  std::vector<std::string> unset_gen_ai_policy_names(
      unset_gen_ai_policies.size());
  std::transform(unset_gen_ai_policies.begin(), unset_gen_ai_policies.end(),
                 unset_gen_ai_policy_names.begin(),
                 [](const auto& policy) { return policy.name; });
  errors->AddError(policy_name(),
                   IDS_POLICY_GEN_AI_DEFAULT_SETTINGS_CONTROL_MESSAGE,
                   base::JoinString(unset_gen_ai_policy_names, ", "),
                   /*error_path=*/{}, PolicyMap::MessageType::kInfo);

  return true;
}
void GenAiDefaultSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // The feature check may happen before `FeatureList` is registered, so check
  // whether the instance is ready (i.e. registration is complete) before
  // checking the feature state.
  if (!base::FeatureList::GetInstance() ||
      !base::FeatureList::IsEnabled(kApplyGenAiPolicyDefaults)) {
    return;
  }

  const base::Value* default_value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  if (!default_value) {
    return;
  }

  for (const auto& policy : GetUnsetGenAiPolicies(policies)) {
    // The feature policy isn't set, so apply the default value to the feature
    // policy prefs.
    prefs->SetValue(policy.pref_path, base::Value(default_value->GetInt()));
  }
}

std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
GenAiDefaultSettingsPolicyHandler::GetUnsetGenAiPolicies(
    const PolicyMap& policies) {
  std::vector<GenAiPolicyDetails> unset_gen_ai_policies;

  for (const auto& policy : gen_ai_policies_) {
    // Add all covered policies without a set policy value.
    if (!policies.Get(policy.name)) {
      unset_gen_ai_policies.push_back(policy);
    }
  }

  return unset_gen_ai_policies;
}

}  // namespace policy

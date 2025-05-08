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

// Kill switch for using the policy_value_to_pref_map for translation.
// If disabled, the pref map will not be used and the GenAI policy will not be
// set.
BASE_FEATURE(kGenAiPolicyDefaultsUsePrefMap,
             "GenAiPolicyDefaultsUsePrefMap",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

bool IsApplyGenAiPolicyDefaultsEnabled() {
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(kApplyGenAiPolicyDefaults);
}

bool IsGenAiPolicyDefaultsUsePrefMap() {
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(kGenAiPolicyDefaultsUsePrefMap);
}

// Determines the integer value to apply to a specific GenAI policy's
// preference.
std::optional<int> GetValueToApply(
    const GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails& policy,
    const PolicyMap& policies,
    int default_int_value) {
  // Don't apply the default if the specific policy is already set.
  if (policies.Get(policy.name)) {
    return std::nullopt;
  }

  // If no map exists, use the default value directly.
  if (policy.policy_value_to_pref_map.empty()) {
    return default_int_value;
  }

  // If a map exists, do not use it only if the feature is disabled.
  if (!IsGenAiPolicyDefaultsUsePrefMap()) {
    return std::nullopt;
  }

  // Find the corresponding value in the map. If not found, don't apply.
  auto it = policy.policy_value_to_pref_map.find(default_int_value);
  if (it == policy.policy_value_to_pref_map.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace

// Expected keys for policy_value_to_pref_map. These should correspond to all
// valid GenAiDefaultSettings policy values.
constexpr std::array<int, 3> kExpectedPolicyMapKeys = {0, 1, 2};

GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails::GenAiPolicyDetails(
    std::string name,
    std::string pref_path)
    : name(std::move(name)), pref_path(std::move(pref_path)) {}

GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails::GenAiPolicyDetails(
    std::string name,
    std::string pref_path,
    PolicyValueToPrefMap policy_value_to_pref_map)
    : name(std::move(name)),
      pref_path(std::move(pref_path)),
      policy_value_to_pref_map(std::move(policy_value_to_pref_map)) {}

GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails::GenAiPolicyDetails(
    const GenAiPolicyDetails& other) = default;

GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails::~GenAiPolicyDetails() =
    default;

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
  if (!IsApplyGenAiPolicyDefaultsEnabled()) {
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

  // If the map feature is enabled, check that any maps provided include all
  // expected policy values.
  if (IsGenAiPolicyDefaultsUsePrefMap()) {
    for (const auto& policy : gen_ai_policies_) {
      if (!policy.policy_value_to_pref_map.empty()) {
        for (int key : kExpectedPolicyMapKeys) {
          CHECK(policy.policy_value_to_pref_map.contains(key))
              << "Missing key " << key << " for policy: " << policy.name;
        }
      }
    }
  }

  // If no GenAI policies are being controlled by this policy, add a warning so
  // admins can take action.
  auto controlled_gen_ai_policies = GetControlledGenAiPolicies(policies);
  if (controlled_gen_ai_policies.empty()) {
    errors->AddError(policy_name(),
                     IDS_POLICY_GEN_AI_DEFAULT_SETTINGS_NO_CONTROL_MESSAGE,
                     /*error_path=*/{}, PolicyMap::MessageType::kInfo);
    return true;
  }

  // Add info message to the policy describing which GenAI policies have their
  // default behavior controlled by GenAiDefaultSettings.
  std::vector<std::string> controlled_gen_ai_policy_names(
      controlled_gen_ai_policies.size());
  std::transform(controlled_gen_ai_policies.begin(),
                 controlled_gen_ai_policies.end(),
                 controlled_gen_ai_policy_names.begin(),
                 [](const auto& policy) { return policy.name; });
  errors->AddError(policy_name(),
                   IDS_POLICY_GEN_AI_DEFAULT_SETTINGS_CONTROL_MESSAGE,
                   base::JoinString(controlled_gen_ai_policy_names, ", "),
                   /*error_path=*/{}, PolicyMap::MessageType::kInfo);

  return true;
}

void GenAiDefaultSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // The feature check may happen before `FeatureList` is registered, so check
  // whether the instance is ready (i.e. registration is complete) before
  // checking the feature state.
  if (!IsApplyGenAiPolicyDefaultsEnabled()) {
    return;
  }

  const base::Value* default_value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  if (!default_value) {
    return;
  }

  int default_int_value = default_value->GetInt();
  for (const auto& policy : gen_ai_policies_) {
    std::optional<int> value_to_apply =
        GetValueToApply(policy, policies, default_int_value);
    if (value_to_apply.has_value()) {
      prefs->SetInteger(policy.pref_path, value_to_apply.value());
    }
  }
}

std::vector<GenAiDefaultSettingsPolicyHandler::GenAiPolicyDetails>
GenAiDefaultSettingsPolicyHandler::GetControlledGenAiPolicies(
    const PolicyMap& policies) {
  std::vector<GenAiPolicyDetails> controlled_gen_ai_policies;

  for (const auto& policy : gen_ai_policies_) {
    // If the map feature is disabled, policies with a map should not be
    // included.
    if (!IsGenAiPolicyDefaultsUsePrefMap() &&
        !policy.policy_value_to_pref_map.empty()) {
      continue;
    }

    // Add all covered policies without a set policy value.
    if (!policies.Get(policy.name)) {
      controlled_gen_ai_policies.push_back(policy);
    }
  }

  return controlled_gen_ai_policies;
}

}  // namespace policy

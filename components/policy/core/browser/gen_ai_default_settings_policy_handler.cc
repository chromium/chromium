// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/gen_ai_default_settings_policy_handler.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

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

  for (const auto& policy : gen_ai_policies_) {
    // If a policy value is already set for the feature policy, skip it as
    // it will be mapped to prefs by its own handler.
    if (policies.Get(policy.name)) {
      continue;
    }

    // The feature policy isn't set, so apply the default value to the feature
    // policy prefs.
    prefs->SetValue(policy.pref_path, base::Value(default_value->GetInt()));
  }
}

}  // namespace policy

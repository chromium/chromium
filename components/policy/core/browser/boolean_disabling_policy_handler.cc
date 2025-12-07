// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/boolean_disabling_policy_handler.h"

#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

BooleanDisablingPolicyHandler::BooleanDisablingPolicyHandler(
    const char* policy_name,
    const char* pref_path)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::BOOLEAN),
      pref_path_(pref_path) {}

BooleanDisablingPolicyHandler::~BooleanDisablingPolicyHandler() = default;

void BooleanDisablingPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_) {
    return;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);

  if (value && value->GetBool() == false) {
    // Only apply the policy to the pref if the policy value is false.
    prefs->SetBoolean(pref_path_, false);
  }
}

}  // namespace policy

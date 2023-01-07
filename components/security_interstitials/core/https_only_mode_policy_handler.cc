// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_policy_handler.h"

#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

HttpsOnlyModePolicyHandler::HttpsOnlyModePolicyHandler(
    const char* const pref_name)
    : TypeCheckingPolicyHandler(key::kHttpsOnlyMode, base::Value::Type::STRING),
      pref_name_(pref_name) {}

HttpsOnlyModePolicyHandler::~HttpsOnlyModePolicyHandler() = default;

void HttpsOnlyModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(key::kHttpsOnlyMode, base::Value::Type::STRING);
  if (value && value->GetString() == "disallowed") {
    // Only apply the policy to the pref if it is set to "disallowed".
    prefs->SetBoolean(pref_name_, false);
  }
}

}  // namespace policy

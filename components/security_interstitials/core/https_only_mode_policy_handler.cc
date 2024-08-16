// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_policy_handler.h"

#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

HttpsOnlyModePolicyHandler::HttpsOnlyModePolicyHandler(
    const char* const main_pref_name,
    const char* const incognito_pref_name,
    const char* const balanced_pref_name)
    : TypeCheckingPolicyHandler(key::kHttpsOnlyMode, base::Value::Type::STRING),
      main_pref_name_(main_pref_name),
      incognito_pref_name_(incognito_pref_name),
      balanced_pref_name_(balanced_pref_name) {}

HttpsOnlyModePolicyHandler::~HttpsOnlyModePolicyHandler() = default;

void HttpsOnlyModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(key::kHttpsOnlyMode, base::Value::Type::STRING);
  // The policy supports force-disabling the HTTPS-First Mode pref
  // ("disallowed"), force-enabling strict mode ("force_enabled"), balanced mode
  // ("force_balanced_enabled") or allowing the user to choose (no policy or
  // setting it to "allowed").
  //
  // For backwards compatibility, we're stuck mapping these string-enum values
  // to the boolean pref states, rather than being able to do a simple
  // policy-pref mapping.
  if (!value) {
    return;
  }
  auto str_value = value->GetString();

  // Do not override settings in the default 'allowed' case.
  if (str_value == "allowed") {
    return;
  }

  prefs->SetBoolean(main_pref_name_, str_value == "force_enabled");
  prefs->SetBoolean(balanced_pref_name_, str_value == "force_balanced_enabled");
  prefs->SetBoolean(
      incognito_pref_name_,
      (str_value == "force_enabled" || str_value == "force_balanced_enabled"));
}

}  // namespace policy

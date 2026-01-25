// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/incognito/incognito_mode_policy_handler.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace {
bool IsValidAvailabilityInt(int in_value) {
  return in_value >= 0 &&
         in_value <
             static_cast<int>(policy::IncognitoModeAvailability::kNumTypes);
}
}  // namespace

namespace policy {

IncognitoModePolicyHandler::IncognitoModePolicyHandler() = default;

IncognitoModePolicyHandler::~IncognitoModePolicyHandler() = default;

bool IncognitoModePolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* availability =
      policies.GetValueUnsafe(key::kIncognitoModeAvailability);
  if (availability) {
    if (!availability->is_int()) {
      errors->AddError(key::kIncognitoModeAvailability, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
      return false;
    }
    if (!IsValidAvailabilityInt(availability->GetInt())) {
      errors->AddError(key::kIncognitoModeAvailability,
                       IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::NumberToString(availability->GetInt()));
      return false;
    }
  }

  const base::Value* incognito_allowlist =
      policies.GetValueUnsafe(key::kIncognitoModeUrlAllowlist);
  if (incognito_allowlist && !incognito_allowlist->is_list()) {
    errors->AddError(key::kIncognitoModeUrlAllowlist, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));
    return false;
  }

  const base::Value* incognito_blocklist =
      policies.GetValueUnsafe(key::kIncognitoModeUrlBlocklist);
  if (incognito_blocklist && !incognito_blocklist->is_list()) {
    errors->AddError(key::kIncognitoModeUrlBlocklist, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));
    return false;
  }
  return true;
}

void IncognitoModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* availability = policies.GetValue(
      key::kIncognitoModeAvailability, base::Value::Type::INTEGER);
  std::optional<policy::IncognitoModeAvailability> incognito_mode_availability;
  if (availability && IsValidAvailabilityInt(availability->GetInt())) {
    incognito_mode_availability =
        static_cast<policy::IncognitoModeAvailability>(availability->GetInt());
  }
  ApplyPolicySettings(policies, prefs, incognito_mode_availability);
}

// This method handles the interaction between the incognito mode availability,
// allowlist and blocklist policies. The final pref values are calculated
// based on the following logic:
// 1. If allowlist is set and incognito mode is disabled, then enable incognito
// mode.
// 2. If allowlist is set and (blocklist is not set or empty or incognito mode
// is disabled), then blocklist is set to "*", which effectively blocks all the
// sites besides allowlisted ones.
void IncognitoModePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs,
    std::optional<policy::IncognitoModeAvailability> incognito_availability) {
  const base::Value* incognito_allowlist = policies.GetValue(
      key::kIncognitoModeUrlAllowlist, base::Value::Type::LIST);
  const base::Value* incognito_blocklist = policies.GetValue(
      key::kIncognitoModeUrlBlocklist, base::Value::Type::LIST);

  const bool incognito_allowlist_set =
      incognito_allowlist && !incognito_allowlist->GetList().empty();
  const bool incognito_blocklist_set =
      incognito_blocklist && !incognito_blocklist->GetList().empty();

  if (incognito_allowlist_set) {
    prefs->SetValue(policy_prefs::kIncognitoModeUrlAllowlist,
                    incognito_allowlist->Clone());
  }

  // If allowlist is set and (blocklist is not set or empty or incognito mode
  // is disabled), then blocklist is set to "*", which effectively blocks all
  // the sites besides allowlisted ones.
  if (incognito_allowlist_set &&
      (!incognito_blocklist_set ||
       incognito_availability == IncognitoModeAvailability::kDisabled)) {
    base::ListValue all_blocked_blocklist;
    all_blocked_blocklist.Append("*");
    prefs->SetValue(policy_prefs::kIncognitoModeUrlBlocklist,
                    base::Value(std::move(all_blocked_blocklist)));
  } else if (incognito_blocklist_set) {
    prefs->SetValue(policy_prefs::kIncognitoModeUrlBlocklist,
                    incognito_blocklist->Clone());
  }

  // If allowlist is set and incognito mode is disabled, then enable incognito
  // mode.
  if (incognito_allowlist_set &&
      incognito_availability == IncognitoModeAvailability::kDisabled) {
    prefs->SetInteger(policy::policy_prefs::kIncognitoModeAvailability,
                      static_cast<int>(IncognitoModeAvailability::kEnabled));
  } else if (incognito_availability.has_value()) {
    prefs->SetInteger(policy::policy_prefs::kIncognitoModeAvailability,
                      static_cast<int>(incognito_availability.value()));
  }
}

}  // namespace policy

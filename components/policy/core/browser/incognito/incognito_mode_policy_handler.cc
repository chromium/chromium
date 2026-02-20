// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/incognito/incognito_mode_policy_handler.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_matcher/url_util.h"

namespace {
bool IsValidAvailabilityInt(int in_value) {
  return in_value >= 0 &&
         in_value <
             static_cast<int>(policy::IncognitoModeAvailability::kNumTypes);
}

// ValidateHost checks if the given host string is considered valid based on
// the usage of the asterisk character *. An asterisk * as the entire hostname
// is allowed. An asterisk * within any other hostname string (e.g.,
// *.example.com, dev.*.com, example*com) is disallowed and considered
// invalid.
// It is a common mistake that admins allow sites with * as a wildcard
// in the hostname although it has no effect on the domain and subdomains. Two
// example for such a common mistake are: 1- *.android.com 2- developer.*.com
// which allow neither android.com nor developer.android.com
bool ValidateHost(const std::string& host) {
  return host == "*" || host.find('*') == std::string::npos;
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

  if (!CheckUrlListPolicySettings(key::kIncognitoModeUrlAllowlist, policies,
                                  errors)) {
    return false;
  }
  if (!CheckUrlListPolicySettings(key::kIncognitoModeUrlBlocklist, policies,
                                  errors)) {
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
  std::optional<base::ListValue> incognito_allowlist =
      GetFilteredUrlListPolicyValue(policies, key::kIncognitoModeUrlAllowlist);
  std::optional<base::ListValue> incognito_blocklist =
      GetFilteredUrlListPolicyValue(policies, key::kIncognitoModeUrlBlocklist);

  const bool incognito_allowlist_set =
      incognito_allowlist.has_value() && !incognito_allowlist.value().empty();
  const bool incognito_blocklist_set =
      incognito_blocklist.has_value() && !incognito_blocklist.value().empty();

  if (incognito_allowlist_set) {
    prefs->SetValue(policy_prefs::kIncognitoModeUrlAllowlist,
                    base::Value(std::move(incognito_allowlist.value())));
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
                    base::Value(std::move(incognito_blocklist.value())));
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

bool IncognitoModePolicyHandler::CheckUrlListPolicySettings(
    const char* policy_name,
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  if (!policies.IsPolicySet(policy_name)) {
    return true;
  }

  const base::Value* value =
      policies.GetValue(policy_name, base::Value::Type::LIST);

  if (!value) {
    errors->AddError(policy_name, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));
    return false;
  }

  // Filters more than |policy::kMaxUrlFiltersPerPolicy| are ignored, add a
  // warning message.
  if (value->GetList().size() > kMaxUrlFiltersPerPolicy) {
    errors->AddError(policy_name,
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(kMaxUrlFiltersPerPolicy));
  }

  bool type_error = false;
  std::string policy;
  std::vector<std::string> invalid_patterns;
  for (const auto& policy_iter : value->GetList()) {
    if (!policy_iter.is_string()) {
      type_error = true;
      continue;
    }

    policy = policy_iter.GetString();
    if (!ValidatePolicy(policy)) {
      invalid_patterns.push_back(policy);
    }
  }

  if (type_error) {
    errors->AddError(policy_name, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
  }

  if (invalid_patterns.size()) {
    errors->AddError(policy_name, IDS_POLICY_PROTO_PARSING_ERROR,
                     base::JoinString(invalid_patterns, ","));
  }

  return true;
}

bool IncognitoModePolicyHandler::ValidatePolicy(
    const std::string& url_pattern) {
  url_matcher::util::FilterComponents components;
  return url_matcher::util::FilterToComponents(
             url_pattern, &components.scheme, &components.host,
             &components.match_subdomains, &components.port, &components.path,
             &components.query) &&
         ValidateHost(components.host);
}

std::optional<base::ListValue>
IncognitoModePolicyHandler::GetFilteredUrlListPolicyValue(
    const PolicyMap& policies,
    const char* policy_name) {
  const base::Value* value =
      policies.GetValue(policy_name, base::Value::Type::LIST);
  if (!value) {
    return std::nullopt;
  }

  base::ListValue filtered_list;
  for (const auto& entry : value->GetList()) {
    if (entry.is_string() && ValidatePolicy(entry.GetString())) {
      filtered_list.Append(entry.Clone());
    }
  }

  // Truncate the list of filters if it is too large.
  if (filtered_list.size() > kMaxUrlFiltersPerPolicy) {
    filtered_list.erase(filtered_list.begin() + kMaxUrlFiltersPerPolicy,
                        filtered_list.end());
  }
  return filtered_list;
}

}  // namespace policy

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_scheme_list_policy_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_matcher/url_util.h"

namespace policy {

URLSchemeListPolicyHandler::URLSchemeListPolicyHandler(const char* policy_name,
                                                       const char* pref_path)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path) {}

URLSchemeListPolicyHandler::~URLSchemeListPolicyHandler() = default;

bool URLSchemeListPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const base::Value* schemes =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!schemes || schemes->GetList().empty())
    return true;

  // Filters more than |url_util::kMaxFiltersPerPolicy| are ignored, add a
  // warning message.
  if (schemes->GetList().size() > max_items()) {
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(max_items()));
  }

  std::vector<std::string> invalid_policies;
  for (const auto& entry : schemes->GetList()) {
    if (!ValidatePolicyEntry(entry.GetIfString()))
      invalid_policies.push_back(entry.GetString());
  }

  if (!invalid_policies.empty()) {
    errors->AddError(policy_name(), IDS_POLICY_PROTO_PARSING_ERROR,
                     base::JoinString(invalid_policies, ","));
  }

  return invalid_policies.size() < schemes->GetList().size();
}

void URLSchemeListPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* schemes =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!schemes)
    return;
  base::Value::List filtered_schemes;
  for (const auto& entry : schemes->GetList()) {
    if (filtered_schemes.size() >= max_items()) {
      break;
    }

    if (ValidatePolicyEntry(entry.GetIfString()))
      filtered_schemes.Append(entry.Clone());
  }

  prefs->SetValue(pref_path_, base::Value(std::move(filtered_schemes)));
}

size_t URLSchemeListPolicyHandler::max_items() {
  return kMaxUrlFiltersPerPolicy;
}

// Validates that policy follows official pattern
// https://www.chromium.org/administrators/url-blocklist-filter-format
bool URLSchemeListPolicyHandler::ValidatePolicyEntry(
    const std::string* policy) {
  url_matcher::util::FilterComponents components;
  return policy && url_matcher::util::FilterToComponents(
                       *policy, &components.scheme, &components.host,
                       &components.match_subdomains, &components.port,
                       &components.path, &components.query);
}

}  // namespace policy

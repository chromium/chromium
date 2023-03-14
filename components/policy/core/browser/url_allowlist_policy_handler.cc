// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_allowlist_policy_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
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

namespace {

// Checks if the host contains an * (asterik) that would have no effect on the
// domain or subdomain. It is a common mistake that admins allow sites with * as
// a wildcard in the hostname although it has no effect on the domain and
// subdomains. Two example for such a common mistake are: 1- *.android.com 2-
// developer.*.com which allow neither android.com nor developer.android.com
bool ValidateHost(const std::string& host) {
  return host == "*" || host.find('*') == std::string::npos;
}

}  // namespace

namespace policy {

URLAllowlistPolicyHandler::URLAllowlistPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST) {}

URLAllowlistPolicyHandler::~URLAllowlistPolicyHandler() = default;

bool URLAllowlistPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  if (!policies.IsPolicySet(policy_name()))
    return true;

  const base::Value* url_allowlist =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!url_allowlist) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));
    return true;
  }

  // Filters more than |policy::kMaxUrlFiltersPerPolicy| are ignored, add a
  // warning message.
  if (url_allowlist->GetList().size() > kMaxUrlFiltersPerPolicy) {
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(kMaxUrlFiltersPerPolicy));
  }

  bool type_error = false;
  std::string policy;
  std::vector<std::string> invalid_policies;
  for (const auto& policy_iter : url_allowlist->GetList()) {
    if (!policy_iter.is_string()) {
      type_error = true;
      continue;
    }

    policy = policy_iter.GetString();
    if (!ValidatePolicy(policy))
      invalid_policies.push_back(policy);
  }

  if (type_error) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
  }

  if (invalid_policies.size()) {
    errors->AddError(policy_name(), IDS_POLICY_PROTO_PARSING_ERROR,
                     base::JoinString(invalid_policies, ","));
  }

  return true;
}

void URLAllowlistPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* url_allowlist =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!url_allowlist) {
    return;
  }

  base::Value::List filtered_url_allowlist;
  for (const auto& entry : url_allowlist->GetList()) {
    if (entry.is_string())
      filtered_url_allowlist.Append(entry.Clone());
  }

  prefs->SetValue(policy_prefs::kUrlAllowlist,
                  base::Value(std::move(filtered_url_allowlist)));
}

bool URLAllowlistPolicyHandler::ValidatePolicy(const std::string& url_pattern) {
  url_matcher::util::FilterComponents components;
  return url_matcher::util::FilterToComponents(
             url_pattern, &components.scheme, &components.host,
             &components.match_subdomains, &components.port, &components.path,
             &components.query) &&
         ValidateHost(components.host);
}

}  // namespace policy

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_policy_handler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
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

URLBlocklistPolicyHandler::URLBlocklistPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST) {}

URLBlocklistPolicyHandler::~URLBlocklistPolicyHandler() = default;

bool URLBlocklistPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  size_t disabled_schemes_entries = 0;

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  // This policy is deprecated but still supported so check it first.
  const base::Value* disabled_schemes =
      policies.GetValueUnsafe(key::kDisabledSchemes);
  if (disabled_schemes) {
    if (!disabled_schemes->is_list()) {
      errors->AddError(key::kDisabledSchemes, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::LIST));
    } else {
      disabled_schemes_entries = disabled_schemes->GetList().size();
    }
  }
#endif

  if (!policies.IsPolicySet(policy_name()))
    return true;
  const base::Value* url_blocklist =
      policies.GetValue(policy_name(), base::Value::Type::LIST);

  if (!url_blocklist) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));

    return true;
  }

  // Filters more than |url_util::kMaxFiltersPerPolicy| are ignored, add a
  // warning message.
  if (url_blocklist->GetList().size() + disabled_schemes_entries >
      kMaxUrlFiltersPerPolicy) {
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(kMaxUrlFiltersPerPolicy));
  }

  bool type_error = false;
  std::string policy;
  std::vector<std::string> invalid_policies;
  for (const auto& policy_iter : url_blocklist->GetList()) {
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

void URLBlocklistPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* url_blocklist_policy =
      policies.GetValue(policy_name(), base::Value::Type::LIST);

  std::optional<base::Value::List> merged_url_blocklist;

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  const base::Value* disabled_schemes_policy =
      policies.GetValue(key::kDisabledSchemes, base::Value::Type::LIST);
  // We start with the DisabledSchemes because we have size limit when
  // handling URLBlocklists.
  if (disabled_schemes_policy) {
    merged_url_blocklist.emplace();
    for (const auto& entry : disabled_schemes_policy->GetList()) {
      if (entry.is_string()) {
        merged_url_blocklist->Append(base::StrCat({entry.GetString(), "://*"}));
      }
    }
  }
#endif

  if (url_blocklist_policy) {
    if (!merged_url_blocklist)
      merged_url_blocklist.emplace();

    for (const auto& entry : url_blocklist_policy->GetList()) {
      if (entry.is_string())
        merged_url_blocklist->Append(entry.Clone());
    }
  }

  if (merged_url_blocklist) {
    prefs->SetValue(policy_prefs::kUrlBlocklist,
                    base::Value(std::move(merged_url_blocklist.value())));
  }
}

bool URLBlocklistPolicyHandler::ValidatePolicy(const std::string& url_pattern) {
  url_matcher::util::FilterComponents components;
  return url_matcher::util::FilterToComponents(
             url_pattern, &components.scheme, &components.host,
             &components.match_subdomains, &components.port, &components.path,
             &components.query) &&
         ValidateHost(components.host);
}

}  // namespace policy

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_policy_handler.h"

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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

URLBlocklistPolicyHandler::URLBlocklistPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST) {}

URLBlocklistPolicyHandler::~URLBlocklistPolicyHandler() = default;

bool URLBlocklistPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  size_t disabled_schemes_entries = 0;
  // This policy is deprecated but still supported so check it first.
  const base::Value* disabled_schemes =
      policies.GetValueUnsafe(key::kDisabledSchemes);
  if (disabled_schemes) {
    if (!disabled_schemes->is_list()) {
      errors->AddError(key::kDisabledSchemes, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::LIST));
    } else {
      disabled_schemes_entries = disabled_schemes->GetListDeprecated().size();
    }
  }

  const base::Value* url_blocklist = policies.GetValueUnsafe(policy_name());
  if (!url_blocklist)
    return true;

  if (!url_blocklist->is_list()) {
    errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));

    return true;
  }

  // Filters more than |url_util::kMaxFiltersPerPolicy| are ignored, add a
  // warning message.
  if (url_blocklist->GetListDeprecated().size() + disabled_schemes_entries >
      kMaxUrlFiltersPerPolicy) {
    errors->AddError(policy_name(),
                     IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
                     base::NumberToString(kMaxUrlFiltersPerPolicy));
  }

  bool type_error = false;
  std::string policy;
  std::vector<std::string> invalid_policies;
  for (const auto& policy_iter : url_blocklist->GetListDeprecated()) {
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
      policies.GetValueUnsafe(policy_name());
  const base::Value* disabled_schemes_policy =
      policies.GetValueUnsafe(key::kDisabledSchemes);

  absl::optional<std::vector<base::Value>> merged_url_blocklist;

  // We start with the DisabledSchemes because we have size limit when
  // handling URLBlocklists.
  if (disabled_schemes_policy && disabled_schemes_policy->is_list()) {
    merged_url_blocklist = std::vector<base::Value>();
    for (const auto& entry : disabled_schemes_policy->GetListDeprecated()) {
      if (entry.is_string()) {
        merged_url_blocklist->emplace_back(
            base::StrCat({entry.GetString(), "://*"}));
      }
    }
  }

  if (url_blocklist_policy && url_blocklist_policy->is_list()) {
    if (!merged_url_blocklist)
      merged_url_blocklist = std::vector<base::Value>();

    for (const auto& entry : url_blocklist_policy->GetListDeprecated()) {
      if (entry.is_string())
        merged_url_blocklist->push_back(entry.Clone());
    }
  }

  if (merged_url_blocklist) {
    prefs->SetValue(policy_prefs::kUrlBlocklist,
                    base::Value(std::move(merged_url_blocklist.value())));
  }
}

bool URLBlocklistPolicyHandler::ValidatePolicy(const std::string& policy) {
  url_matcher::util::FilterComponents components;
  return url_matcher::util::FilterToComponents(
      policy, &components.scheme, &components.host,
      &components.match_subdomains, &components.port, &components.path,
      &components.query);
}

}  // namespace policy

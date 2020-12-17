// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_policy_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

URLBlocklistPolicyHandler::URLBlocklistPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST) {}

URLBlocklistPolicyHandler::~URLBlocklistPolicyHandler() = default;

bool URLBlocklistPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  // This policy is deprecated but still supported so check it first.
  const base::Value* disabled_schemes =
      policies.GetValue(key::kDisabledSchemes);
  if (disabled_schemes && !disabled_schemes->is_list()) {
    errors->AddError(key::kDisabledSchemes, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::LIST));
  }

  const base::Value* url_blocklist = policies.GetValue(policy_name());
  if (url_blocklist) {
    if (!url_blocklist->is_list()) {
      errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::LIST));
    }
  }

  return true;
}

void URLBlocklistPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* url_blocklist_policy = policies.GetValue(policy_name());
  const base::ListValue* url_blocklist = nullptr;
  if (url_blocklist_policy) {
    url_blocklist_policy->GetAsList(&url_blocklist);
  }

  const base::Value* disabled_schemes_policy =
      policies.GetValue(key::kDisabledSchemes);
  const base::ListValue* disabled_schemes = nullptr;
  if (disabled_schemes_policy)
    disabled_schemes_policy->GetAsList(&disabled_schemes);

  std::vector<base::Value> merged_url_blocklist;

  // We start with the DisabledSchemes because we have size limit when
  // handling URLBlocklists.
  if (disabled_schemes) {
    for (const auto& entry : *disabled_schemes) {
      std::string entry_value;
      if (entry.GetAsString(&entry_value)) {
        entry_value.append("://*");
        merged_url_blocklist.emplace_back(std::move(entry_value));
      }
    }
  }

  if (url_blocklist) {
    for (const auto& entry : *url_blocklist) {
      if (entry.is_string())
        merged_url_blocklist.push_back(entry.Clone());
    }
  }

  if (disabled_schemes || url_blocklist) {
    prefs->SetValue(policy_prefs::kUrlBlocklist,
                    base::Value(std::move(merged_url_blocklist)));
  }
}

}  // namespace policy

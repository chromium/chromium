// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/data_controls_policy_handler.h"

#include "base/numerics/safe_conversions.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/prefs/pref_value_map.h"

namespace data_controls {

DataControlsPolicyHandler::DataControlsPolicyHandler(const char* policy_name,
                                                     const char* pref_path,
                                                     policy::Schema schema)
    : policy::CloudOnlyPolicyHandler(
          policy_name,
          schema.GetKnownProperty(policy_name),
          policy::SchemaOnErrorStrategy::
              SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY),
      pref_path_(pref_path) {}
DataControlsPolicyHandler::~DataControlsPolicyHandler() = default;

void DataControlsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_) {
    return;
  }

  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());
  if (!policy) {
    return;
  }

  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, /*errors=*/nullptr, &policy_value) ||
      !policy_value || !policy_value->is_list()) {
    return;
  }

  policy_value->GetList().EraseIf([this](const base::Value& rule) {
    return !Rule::ValidateRuleValue(policy_name(), rule.GetDict(),
                                    /*error_path=*/{}, /*errors=*/nullptr);
  });

  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  prefs->SetValue(pref_path_, policy_value->Clone());
  prefs->SetInteger(kDataControlsRulesScopePref, policy->scope);
}

bool DataControlsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policy::CloudOnlyPolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (!value) {
    return true;
  }

  DCHECK(value->is_list());
  const auto& rules_list = value->GetList();

  for (size_t i = 0; i < rules_list.size(); ++i) {
    if (rules_list[i].is_dict()) {
      Rule::ValidateRuleValue(policy_name(), rules_list[i].GetDict(),
                              {base::checked_cast<int>(i)}, errors);
    }
  }
  return true;
}

}  // namespace data_controls

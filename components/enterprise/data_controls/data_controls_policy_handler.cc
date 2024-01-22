// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/data_controls_policy_handler.h"

#include "components/enterprise/data_controls/rule.h"
#include "components/prefs/pref_value_map.h"

namespace data_controls {

DataControlsPolicyHandler::DataControlsPolicyHandler(const char* policy_name,
                                                     const char* pref_path,
                                                     policy::Schema schema)
    : policy::CloudOnlyPolicyHandler(
          policy_name,
          schema.GetKnownProperty(policy_name),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN),
      pref_path_(pref_path) {}
DataControlsPolicyHandler::~DataControlsPolicyHandler() = default;

void DataControlsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_) {
    return;
  }

  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value) {
    prefs->SetValue(pref_path_, value->Clone());
  }
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

  bool valid = true;
  for (size_t i = 0; i < rules_list.size(); ++i) {
    DCHECK(rules_list[i].is_dict());
    valid &= Rule::ValidateRuleValue(policy_name(), rules_list[i].GetDict(),
                                     {i}, errors);
  }
  return valid;
}

}  // namespace data_controls

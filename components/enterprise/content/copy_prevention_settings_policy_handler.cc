// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/content/copy_prevention_settings_policy_handler.h"

#include "components/enterprise/content/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace {
const int kMinDataSizeDefault = 100;
}

CopyPreventionSettingsPolicyHandler::CopyPreventionSettingsPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    policy::Schema schema)
    : SchemaValidatingPolicyHandler(
          policy_name,
          schema.GetKnownProperty(policy_name),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN),
      pref_path_(pref_path) {}

CopyPreventionSettingsPolicyHandler::~CopyPreventionSettingsPolicyHandler() =
    default;

// ConfigurationPolicyHandler:
bool CopyPreventionSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policies.IsPolicySet(policy_name()))
    return true;

  if (!SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());
  if (policy->source != policy::POLICY_SOURCE_CLOUD &&
      policy->source != policy::POLICY_SOURCE_CLOUD_FROM_ASH) {
    errors->AddError(policy_name(), IDS_POLICY_CLOUD_SOURCE_ONLY_ERROR);
    return false;
  }

  const base::Value::Dict& dict =
      policies.GetValue(policy_name(), base::Value::Type::DICT)->GetDict();
  const base::Value::List* enable = dict.FindList(
      enterprise::content::kCopyPreventionSettingsEnableFieldName);
  const base::Value::List* disable = dict.FindList(
      enterprise::content::kCopyPreventionSettingsDisableFieldName);
  if (!enable || !disable) {
    errors->AddError(policy_name(),
                     IDS_ENTERPRISE_COPY_PREVENTION_MISSING_LIST_ERROR);
    return false;
  }

  for (auto& pattern : *disable) {
    if (pattern.GetString() == "*") {
      errors->AddError(
          policy_name(),
          IDS_ENTERPRISE_COPY_PREVENTION_DISABLE_CONTAINS_WILDCARD_ERROR);
      return false;
    }
  }

  return true;
}

void CopyPreventionSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::DICT);
  if (!value)
    return;

  base::Value processed_value = value->Clone();

  // The min data size field is optional. Default to 100 bytes if it's not
  // present.
  std::optional<int> min_data_size = value->GetDict().FindInt(
      enterprise::content::kCopyPreventionSettingsMinDataSizeFieldName);
  if (!min_data_size) {
    processed_value.GetDict().Set(
        enterprise::content::kCopyPreventionSettingsMinDataSizeFieldName,
        kMinDataSizeDefault);
  }

  prefs->SetValue(pref_path_, std::move(processed_value));
}

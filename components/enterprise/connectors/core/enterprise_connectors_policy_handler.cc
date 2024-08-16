// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/enterprise_connectors_policy_handler.h"

#include "base/values.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace enterprise_connectors {

EnterpriseConnectorsPolicyHandler::EnterpriseConnectorsPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    policy::Schema schema)
    : EnterpriseConnectorsPolicyHandler(policy_name,
                                        pref_path,
                                        nullptr,
                                        schema) {}

EnterpriseConnectorsPolicyHandler::EnterpriseConnectorsPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    const char* pref_scope_path,
    policy::Schema schema)
    : policy::CloudOnlyPolicyHandler(
          policy_name,
          schema.GetKnownProperty(policy_name),
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN),
      pref_path_(pref_path),
      pref_scope_path_(pref_scope_path) {}

EnterpriseConnectorsPolicyHandler::~EnterpriseConnectorsPolicyHandler() =
    default;

void EnterpriseConnectorsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;

  const policy::PolicyMap::Entry* policy = policies.Get(policy_name());
  if (!policy)
    return;

  const base::Value* value = policy->value_unsafe();
  if (value) {
    prefs->SetValue(pref_path_, value->Clone());

    if (pref_scope_path_)
      prefs->SetInteger(pref_scope_path_, policy->scope);
  }
}

}  // namespace enterprise_connectors

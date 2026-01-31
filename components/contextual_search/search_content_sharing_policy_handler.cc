// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/search_content_sharing_policy_handler.h"

#include "base/values.h"
#include "components/contextual_search/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace contextual_search {

SearchContentSharingPolicyHandler::SearchContentSharingPolicyHandler(
    std::string pref_path_to_override,
    bool convert_policy_value_to_enabled_boolean)
    : TypeCheckingPolicyHandler(policy::key::kSearchContentSharingSettings,
                                base::Value::Type::INTEGER),
      pref_path_to_override_(std::move(pref_path_to_override)),
      convert_policy_value_to_enabled_boolean_(
          convert_policy_value_to_enabled_boolean) {}

SearchContentSharingPolicyHandler::~SearchContentSharingPolicyHandler() =
    default;

void SearchContentSharingPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  if (!value) {
    return;
  }

  int int_value = value->GetInt();
  if (convert_policy_value_to_enabled_boolean_) {
    prefs->SetBoolean(pref_path_to_override_, int_value == 0);
  } else {
    prefs->SetInteger(pref_path_to_override_, int_value);
  }

  prefs->SetInteger(kSearchContentSharingSettings, int_value);
}

}  // namespace contextual_search

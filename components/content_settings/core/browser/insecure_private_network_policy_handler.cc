// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/insecure_private_network_policy_handler.h"

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace content_settings {
namespace {

ContentSetting ConvertBooleanToContentSetting(bool is_allowed) {
  return is_allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
}

}  // namespace

InsecurePrivateNetworkPolicyHandler::InsecurePrivateNetworkPolicyHandler()
    : TypeCheckingPolicyHandler(
          policy::key::kInsecurePrivateNetworkRequestsAllowed,
          base::Value::Type::BOOLEAN) {}

InsecurePrivateNetworkPolicyHandler::~InsecurePrivateNetworkPolicyHandler() =
    default;

void InsecurePrivateNetworkPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (value) {
    prefs->SetValue(
        prefs::kManagedDefaultInsecurePrivateNetworkSetting,
        base::Value(ConvertBooleanToContentSetting(value->GetBool())));
  }
}

}  // namespace content_settings

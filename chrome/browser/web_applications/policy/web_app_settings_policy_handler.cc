// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_settings_policy_handler.h"

#include "base/check_deref.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace web_app {

WebAppSettingsPolicyHandler::WebAppSettingsPolicyHandler(policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy::key::kWebAppSettings,
          prefs::kWebAppSettings,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

WebAppSettingsPolicyHandler::~WebAppSettingsPolicyHandler() = default;

bool WebAppSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policy::SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(
          policies, errors)) {
    return false;
  }

  const policy::PolicyMap::Entry* policy_entry = policies.Get(policy_name());
  if (!policy_entry)
    return true;

  const auto& web_apps_list =
      policy_entry->value(base::Value::Type::LIST)->GetList();
  const auto it = base::ranges::find(
      web_apps_list, kWildcard, [](const base::Value& entry) {
        return CHECK_DEREF(entry.GetDict().FindString(kManifestId));
      });

  if (it != web_apps_list.end()) {
    const std::string* run_on_os_login_str =
        it->GetDict().FindString(kRunOnOsLogin);
    if (run_on_os_login_str && *run_on_os_login_str != kAllowed &&
        *run_on_os_login_str != kBlocked) {
      errors->AddError(policy_name(), IDS_POLICY_INVALID_SELECTION_ERROR,
                       "run_on_os value", policy::PolicyErrorPath{kWildcard});
      return false;
    }

    // Show error during policy parsing if force_unregister_os_integration is
    // provided with a wildcard manifest id.
    std::optional<bool> force_unregistration_value =
        it->GetDict().FindBool(kForceUnregisterOsIntegration);
    if (force_unregistration_value.has_value()) {
      errors->AddError(policy_name(), IDS_POLICY_INVALID_SELECTION_ERROR,
                       "manifest_id", policy::PolicyErrorPath{kWildcard});
      return false;
    }
  }

  return true;
}

}  // namespace web_app

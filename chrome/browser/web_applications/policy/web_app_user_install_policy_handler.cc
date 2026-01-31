// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_user_install_policy_handler.h"

#include <algorithm>

#include "base/check_deref.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"

namespace web_app {

WebAppUserInstallPolicyHandler::WebAppUserInstallPolicyHandler()
    : policy::TypeCheckingPolicyHandler(
          policy::key::kWebAppInstallByUserEnabled,
          base::Value::Type::BOOLEAN) {}

WebAppUserInstallPolicyHandler::~WebAppUserInstallPolicyHandler() = default;

void WebAppUserInstallPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* allow_web_app_install_by_user_value = policies.GetValue(
      policy::key::kWebAppInstallByUserEnabled, base::Value::Type::BOOLEAN);

  // The policy is not set, so web apps are installable by default.
  if (!allow_web_app_install_by_user_value) {
    return;
  }

  // Set the browser pref according to the policy value to be used by the
  // AreWebAppsUserInstallable check.
  bool policy_value = allow_web_app_install_by_user_value->GetBool();
  prefs->SetBoolean(prefs::kWebAppInstallByUserEnabled, policy_value);

  if (!policy_value) {
    // Disable app sync if user install is disabled - by policy design.
    syncer::SyncPrefs::SetTypeDisabledByPolicy(
        prefs, syncer::UserSelectableType::kApps);
  }
}

}  // namespace web_app

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings_policy_handler.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace content_settings {

CookieSettingsPolicyHandler::CookieSettingsPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kBlockThirdPartyCookies,
                                        base::Value::Type::BOOLEAN) {}

CookieSettingsPolicyHandler::~CookieSettingsPolicyHandler() = default;

void CookieSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* third_party_cookie_blocking =
      policies.GetValue(policy_name());
  if (third_party_cookie_blocking) {
    prefs->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(third_party_cookie_blocking->GetBool()
                             ? CookieControlsMode::kBlockThirdParty
                             : CookieControlsMode::kOff));
    // Copy only the disabled managed state of cookie controls to privacy
    // sandbox while privacy sandbox is an experiment.
    if (third_party_cookie_blocking->GetBool()) {
      prefs->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);
    }
  }
  // Also check against the default cookie content settings policy and disable
  // privacy sandbox if it is set to BLOCK.
  const base::Value* default_cookie_setting =
      policies.GetValue(policy::key::kDefaultCookiesSetting);
  if (default_cookie_setting && default_cookie_setting->is_int() &&
      static_cast<ContentSetting>(default_cookie_setting->GetInt()) ==
          CONTENT_SETTING_BLOCK) {
    prefs->SetBoolean(prefs::kPrivacySandboxApisEnabled, false);
  }
}

}  // namespace content_settings

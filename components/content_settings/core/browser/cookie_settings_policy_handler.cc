// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings_policy_handler.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace content_settings {

CookieSettingsPolicyHandler::CookieSettingsPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kBlockThirdPartyCookies,
                                        base::Value::Type::BOOLEAN) {}

CookieSettingsPolicyHandler::~CookieSettingsPolicyHandler() = default;

void CookieSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* third_party_cookie_blocking =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (third_party_cookie_blocking) {
    prefs->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(third_party_cookie_blocking->GetBool()
                             ? CookieControlsMode::kBlockThirdParty
                             : CookieControlsMode::kOff));
  }

  // If there is a Cookie BLOCK default content setting, then this implicitly
  // also blocks 3PC.
  const base::Value* default_cookie_setting = policies.GetValue(
      policy::key::kDefaultCookiesSetting, base::Value::Type::INTEGER);
  if (default_cookie_setting &&
      static_cast<ContentSetting>(default_cookie_setting->GetInt()) ==
          CONTENT_SETTING_BLOCK) {
    prefs->SetInteger(prefs::kCookieControlsMode,
                      static_cast<int>(CookieControlsMode::kBlockThirdParty));
  }
}

}  // namespace content_settings

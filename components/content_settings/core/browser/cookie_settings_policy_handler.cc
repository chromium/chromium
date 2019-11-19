// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings_policy_handler.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace content_settings {

CookieSettingsPolicyHandler::CookieSettingsPolicyHandler() {}

CookieSettingsPolicyHandler::~CookieSettingsPolicyHandler() {}

bool CookieSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  return true;
}

void CookieSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  // When third party cookie blocking is set by policy, the cookie controls UI
  // can't be enabled.
  const base::Value* third_party_cookie_blocking =
      policies.GetValue(policy::key::kBlockThirdPartyCookies);
  if (third_party_cookie_blocking) {
    prefs->SetInteger(prefs::kCookieControlsMode,
                      static_cast<int>(CookieControlsMode::kOff));
  }
}

}  // namespace content_settings

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_POLICY_HANDLER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace content_settings {

// A ConfigurationPolicyHandler which sets kCookieControlsEnabled to
// kOff/kBlockThirdParty based on the BlockThirdPartyCookies policy.
class CookieSettingsPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  CookieSettingsPolicyHandler();

  CookieSettingsPolicyHandler(const CookieSettingsPolicyHandler&) = delete;
  CookieSettingsPolicyHandler& operator=(const CookieSettingsPolicyHandler&) =
      delete;

  ~CookieSettingsPolicyHandler() override;

  // TypeCheckingPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_POLICY_HANDLER_H_

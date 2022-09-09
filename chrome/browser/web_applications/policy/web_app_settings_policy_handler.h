// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_SETTINGS_POLICY_HANDLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_SETTINGS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace web_app {

// Maps policy to pref like SimpleSchemaValidatingPolicyHandler, with additional
// validation for WebAppSettings policy.
class WebAppSettingsPolicyHandler
    : public policy::SimpleSchemaValidatingPolicyHandler {
 public:
  explicit WebAppSettingsPolicyHandler(policy::Schema schema);
  ~WebAppSettingsPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_SETTINGS_POLICY_HANDLER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_USER_INSTALL_POLICY_HANDLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_USER_INSTALL_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {
class PolicyMap;
}  // namespace policy

namespace web_app {

// PolicyHandler for WebAppInstallByUserEnabled policy.
class WebAppUserInstallPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  WebAppUserInstallPolicyHandler();

  WebAppUserInstallPolicyHandler(const WebAppUserInstallPolicyHandler&) =
      delete;
  WebAppUserInstallPolicyHandler& operator=(
      const WebAppUserInstallPolicyHandler&) = delete;

  ~WebAppUserInstallPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_USER_INSTALL_POLICY_HANDLER_H_

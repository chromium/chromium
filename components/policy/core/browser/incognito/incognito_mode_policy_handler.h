// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_INCOGNITO_INCOGNITO_MODE_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_INCOGNITO_INCOGNITO_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// ConfigurationPolicyHandler for the incognito mode policies.
class POLICY_EXPORT IncognitoModePolicyHandler
    : public ConfigurationPolicyHandler {
 public:
  IncognitoModePolicyHandler();

  IncognitoModePolicyHandler(const IncognitoModePolicyHandler&) = delete;
  IncognitoModePolicyHandler& operator=(const IncognitoModePolicyHandler&) =
      delete;

  ~IncognitoModePolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 protected:
  // Apply policy settings with the given incognito mode availability. This
  // method is used to handle the interaction between the incognito mode
  // availability, allowlist and blocklist policies.
  void ApplyPolicySettings(
      const PolicyMap& policies,
      PrefValueMap* prefs,
      std::optional<policy::IncognitoModeAvailability> incognito_availability);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_INCOGNITO_INCOGNITO_MODE_POLICY_HANDLER_H_

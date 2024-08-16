// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_POLICY_HANDLER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Checks and converts the strings in policy::key::kHttpsOnlyMode to booleans
// pref::kHttpsOnlyModeEnabled, pref::kHttpsFirstModeIncognito, and
// pref::kHttpsFirstBalancedModEnabled.
class HttpsOnlyModePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit HttpsOnlyModePolicyHandler(const char* const main_pref_name,
                                      const char* const incognito_pref_name,
                                      const char* const balanced_pref_name);
  ~HttpsOnlyModePolicyHandler() override;
  HttpsOnlyModePolicyHandler(const HttpsOnlyModePolicyHandler&) = delete;
  HttpsOnlyModePolicyHandler& operator=(const HttpsOnlyModePolicyHandler&) =
      delete;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Name of the HTTPS-First Mode pref.
  const char* const main_pref_name_;
  // Name of the HTTPS-First Mode in Incognito pref.
  const char* const incognito_pref_name_;
  // Name of the HTTPS-First Balanced Mode pref.
  const char* const balanced_pref_name_;
};

}  // namespace policy

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_POLICY_HANDLER_H_

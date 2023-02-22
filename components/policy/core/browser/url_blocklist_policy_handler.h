// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_BLOCKLIST_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_BLOCKLIST_POLICY_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

namespace policy {

// Possible values for kSafeSitesFilterBehavior pref from policy. Values must
// coincide with SafeSitesFilterBehavior from policy_templates.json.
enum class SafeSitesFilterBehavior {
  kSafeSitesFilterDisabled = 0,
  kSafeSitesFilterEnabled = 1,
};

// Handles URLBlocklist policies.
class POLICY_EXPORT URLBlocklistPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  explicit URLBlocklistPolicyHandler(const char* policy_name);
  URLBlocklistPolicyHandler(const URLBlocklistPolicyHandler&) = delete;
  URLBlocklistPolicyHandler& operator=(const URLBlocklistPolicyHandler&) =
      delete;
  ~URLBlocklistPolicyHandler() override;

  // Validates that policy follows official pattern
  // https://www.chromium.org/administrators/url-blocklist-filter-format
  bool ValidatePolicy(const std::string& url_pattern);

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_BLOCKLIST_POLICY_HANDLER_H_

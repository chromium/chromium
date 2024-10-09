// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_GEN_AI_DEFAULT_SETTINGS_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_GEN_AI_DEFAULT_SETTINGS_POLICY_HANDLER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// Handles the pref for the `GenAiDefaultSettings` policy, as well as the prefs
// of other included GenAI policies which are not already set by another source.
class POLICY_EXPORT GenAiDefaultSettingsPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  // Struct containing the necessary info to set the default value of each
  // covered GenAI policy.
  struct GenAiPolicyDetails {
    GenAiPolicyDetails(std::string name, std::string pref_path)
        : name(std::move(name)), pref_path(std::move(pref_path)) {}

    std::string name;
    std::string pref_path;
  };

  explicit GenAiDefaultSettingsPolicyHandler(
      std::vector<GenAiPolicyDetails>&& gen_ai_policies);
  GenAiDefaultSettingsPolicyHandler(const GenAiDefaultSettingsPolicyHandler&) =
      delete;
  GenAiDefaultSettingsPolicyHandler& operator=(
      const GenAiDefaultSettingsPolicyHandler&) = delete;
  ~GenAiDefaultSettingsPolicyHandler() override;

  // policy::TypeCheckingPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // GenAI policies for which the default should be applied when unset.
  std::vector<GenAiPolicyDetails> gen_ai_policies_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_GEN_AI_DEFAULT_SETTINGS_POLICY_HANDLER_H_

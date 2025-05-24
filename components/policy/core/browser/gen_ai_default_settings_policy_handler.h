// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_GEN_AI_DEFAULT_SETTINGS_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_GEN_AI_DEFAULT_SETTINGS_POLICY_HANDLER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
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
  using PolicyValueToPrefMap = base::flat_map<int, int>;

  // Struct containing the necessary info to set the default value of each
  // covered GenAI policy.
  struct POLICY_EXPORT GenAiPolicyDetails {
    explicit GenAiPolicyDetails(std::string name, std::string pref_path);
    explicit GenAiPolicyDetails(std::string name,
                                std::string pref_path,
                                PolicyValueToPrefMap policy_value_to_pref_map);
    GenAiPolicyDetails(const GenAiPolicyDetails& other);
    ~GenAiPolicyDetails();

    std::string name;
    std::string pref_path;
    // Optional map to translate the integer value from `GenAiDefaultSettings`
    // policy to a specific integer value for this policy's preference. If an
    // entry for the `GenAiDefaultSettings` value doesn't exist in this map,
    // the integer value itself will be used for the preference.
    PolicyValueToPrefMap policy_value_to_pref_map;
  };

  explicit GenAiDefaultSettingsPolicyHandler(
      std::vector<GenAiPolicyDetails>&& gen_ai_policies);
  GenAiDefaultSettingsPolicyHandler(const GenAiDefaultSettingsPolicyHandler&) =
      delete;
  GenAiDefaultSettingsPolicyHandler& operator=(
      const GenAiDefaultSettingsPolicyHandler&) = delete;
  ~GenAiDefaultSettingsPolicyHandler() override;

  // policy::TypeCheckingPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Returns a list of covered GenAI policies to which the default value can be
  // applied.
  std::vector<GenAiPolicyDetails> GetControlledGenAiPolicies(
      const PolicyMap& policies);

  // GenAI policies for which the default should be applied when unset.
  std::vector<GenAiPolicyDetails> gen_ai_policies_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_GEN_AI_DEFAULT_SETTINGS_POLICY_HANDLER_H_

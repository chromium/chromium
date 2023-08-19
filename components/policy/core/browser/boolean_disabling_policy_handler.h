// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_BOOLEAN_DISABLING_POLICY_HANDLER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_BOOLEAN_DISABLING_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/policy_export.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

// This policy handler only forces the pref to false when policy value is false.
// Otherwise this handler does not affect the value of the pref. This can be
// used in cases where the policy is something like "AllowFeatureX" and the pref
// is something like "EnableFeatureX". The admin can prevent the feature from
// from being used by setting the policy to false which forces the pref to
// false. However, if the policy is true, the pref is not forced to true and the
// user must still manually enable the feature from settings.
class POLICY_EXPORT BooleanDisablingPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  BooleanDisablingPolicyHandler(const char* policy_name, const char* pref_path);
  ~BooleanDisablingPolicyHandler() override;
  BooleanDisablingPolicyHandler(const BooleanDisablingPolicyHandler&) = delete;
  BooleanDisablingPolicyHandler& operator=(
      const BooleanDisablingPolicyHandler&) = delete;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const char* pref_path_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_BOOLEAN_DISABLING_POLICY_HANDLER_H_

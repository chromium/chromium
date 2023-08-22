// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_IDLE_IDLE_TIMEOUT_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_IDLE_IDLE_TIMEOUT_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/sync/base/user_selectable_type.h"

class PrefValueMap;

namespace policy {
class PolicyErrorMap;
class PolicyMap;
}  // namespace policy

namespace enterprise_idle {

// Handles IdleTimeout policy.
class IdleTimeoutPolicyHandler : public policy::IntRangePolicyHandler {
 public:
  IdleTimeoutPolicyHandler();

  IdleTimeoutPolicyHandler(const IdleTimeoutPolicyHandler&) = delete;
  IdleTimeoutPolicyHandler& operator=(const IdleTimeoutPolicyHandler&) = delete;

  ~IdleTimeoutPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
};

// Handles IdleTimeoutActions policy.
class IdleTimeoutActionsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit IdleTimeoutActionsPolicyHandler(policy::Schema schema);

  IdleTimeoutActionsPolicyHandler(const IdleTimeoutActionsPolicyHandler&) =
      delete;
  IdleTimeoutActionsPolicyHandler& operator=(
      const IdleTimeoutActionsPolicyHandler&) = delete;

  ~IdleTimeoutActionsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void PrepareForDisplaying(policy::PolicyMap* policies) const override;

 private:
  // Caches sync types required when the policy is checked, to
  // avoid recomputing when it is applied or prepared for display.
  syncer::UserSelectableTypeSet forced_disabled_sync_types_;
};

}  // namespace enterprise_idle

#endif  // COMPONENTS_ENTERPRISE_IDLE_IDLE_TIMEOUT_POLICY_HANDLER_H_

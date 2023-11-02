// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_POLICY_HANDLER_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_POLICY_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace syncer {

class PolicyMap;

// ConfigurationPolicyHandler for the SyncDisabled policy.
class SyncPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  SyncPolicyHandler();

  SyncPolicyHandler(const SyncPolicyHandler&) = delete;
  SyncPolicyHandler& operator=(const SyncPolicyHandler&) = delete;

  ~SyncPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_POLICY_HANDLER_H_

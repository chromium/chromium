// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_PROXY_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_PROXY_POLICY_PROVIDER_H_

#include "base/macros.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/policy_export.h"

namespace policy {

// A policy provider implementation that acts as a proxy for another policy
// provider, swappable at any point.
//
// Note that ProxyPolicyProvider correctly forwards RefreshPolicies() calls to
// the delegate if present. If there is no delegate, the refresh results in an
// immediate (empty) policy update.
//
// Furthermore, IsInitializationComplete() is implemented trivially - it always
// returns true. Given that the delegate may be swapped at any point, there's no
// point in trying to carry over initialization status from the delegate.
//
// This policy provider implementation is used to inject browser-global policy
// originating from the user policy configured on the primary Chrome OS user
// (i.e. the user logging in from the login screen). This way, policy settings
// on the primary user propagate into g_browser_process->local_state_().
//
// The bizarre situation of user-scoped policy settings which are implemented
// browser-global wouldn't exist in an ideal world. However, for historic
// and technical reasons there are policy settings that are scoped to the user
// but are implemented to take effect for the entire browser instance. A good
// example for this are policies that affect the Chrome network stack in areas
// where there's no profile-specific context. The meta data in
// policy_templates.json allows to identify the policies in this bucket; they'll
// have per_profile set to False, supported_on including chrome_os, and
// dynamic_refresh set to True.
class POLICY_EXPORT ProxyPolicyProvider
    : public ConfigurationPolicyProvider,
      public ConfigurationPolicyProvider::Observer {
 public:
  ProxyPolicyProvider();
  ~ProxyPolicyProvider() override;

  // Updates the provider this proxy delegates to.
  void SetDelegate(ConfigurationPolicyProvider* delegate);

  // ConfigurationPolicyProvider:
  void Shutdown() override;
  void RefreshPolicies() override;

  // ConfigurationPolicyProvider::Observer:
  void OnUpdatePolicy(ConfigurationPolicyProvider* provider) override;

  // When set to true, this ProxyPolicyProvider will ignore subsequent policy
  // updates.
  void SetBlockPolicyUpdatesForTesting(bool block_policy_updates_for_testing) {
    block_policy_updates_for_testing_ = block_policy_updates_for_testing;
  }

 private:
  ConfigurationPolicyProvider* delegate_;
  bool block_policy_updates_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(ProxyPolicyProvider);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_PROXY_POLICY_PROVIDER_H_

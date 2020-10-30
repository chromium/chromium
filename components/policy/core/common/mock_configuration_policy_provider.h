// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_MOCK_CONFIGURATION_POLICY_PROVIDER_H_

#include "base/macros.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

// Mock ConfigurationPolicyProvider implementation that supplies canned
// values for polices.
// TODO(joaodasilva, mnissler): introduce an implementation that non-policy
// code can use that doesn't require the usual boilerplate.
// http://crbug.com/242087
class MockConfigurationPolicyProvider : public ConfigurationPolicyProvider {
 public:
  MockConfigurationPolicyProvider();
  ~MockConfigurationPolicyProvider() override;

  MOCK_CONST_METHOD1(IsInitializationComplete, bool(PolicyDomain domain));
  MOCK_METHOD0(RefreshPolicies, void());

  // Make public for tests.
  using ConfigurationPolicyProvider::UpdatePolicy;

  // Utility method that invokes UpdatePolicy() with a PolicyBundle that maps
  // the Chrome namespace to a copy of |policy|.
  // Note: Replaces the PolicyBundle, so any policy that has been set previously
  // will be lost when calling this utility method.
  void UpdateChromePolicy(const PolicyMap& policy);

  // Utility method that invokes UpdatePolicy() with a PolicyBundle that maps
  // the extension's |extension_id| namespace to a copy of |policy|.
  // Note: Replaces the PolicyBundle, so any policy that has been set previously
  // will be lost when calling this utility method.
  void UpdateExtensionPolicy(const PolicyMap& policy,
                             const std::string& extension_id);

  // Convenience method so that tests don't need to create a registry to create
  // this mock.
  using ConfigurationPolicyProvider::Init;
  void Init() {
    ConfigurationPolicyProvider::Init(&registry_);
  }

  // Convenience method that installs an expectation on RefreshPolicies that
  // just notifies the observers and serves the same policies.
  void SetAutoRefresh();

 private:
  void RefreshWithSamePolicies();

  SchemaRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(MockConfigurationPolicyProvider);
};

class MockConfigurationPolicyObserver
    : public ConfigurationPolicyProvider::Observer {
 public:
  MockConfigurationPolicyObserver();
  ~MockConfigurationPolicyObserver() override;

  MOCK_METHOD1(OnUpdatePolicy, void(ConfigurationPolicyProvider*));
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MOCK_CONFIGURATION_POLICY_PROVIDER_H_

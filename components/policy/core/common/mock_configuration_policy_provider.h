// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MOCK_CONFIGURATION_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_MOCK_CONFIGURATION_POLICY_PROVIDER_H_

#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

// Mock ConfigurationPolicyProvider implementation that supplies canned
// values for polices.
class MockConfigurationPolicyProvider : public ConfigurationPolicyProvider,
                                        public policy::PolicyService::Observer {
 public:
  MockConfigurationPolicyProvider();
  MockConfigurationPolicyProvider(const MockConfigurationPolicyProvider&) =
      delete;
  MockConfigurationPolicyProvider& operator=(
      const MockConfigurationPolicyProvider&) = delete;
  ~MockConfigurationPolicyProvider() override;

  MOCK_CONST_METHOD1(IsInitializationComplete, bool(PolicyDomain domain));
  MOCK_CONST_METHOD1(IsFirstPolicyLoadComplete, bool(PolicyDomain domain));
  MOCK_METHOD1(RefreshPolicies, void(PolicyFetchReason reason));

  // Use this for a more accurate policy update events. Not using this may
  // may result in flaky tests where we expect an event to be propagated and
  // it is not.
  void SetupPolicyServiceForPolicyUpdates(
      policy::PolicyService* policy_service);

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

  // Utility testing method used to set up boilerplate |ON_CALL| defaults.
  void SetDefaultReturns(bool is_initialization_complete_return,
                         bool is_first_policy_load_complete_return) {
    ON_CALL(*this, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(is_initialization_complete_return));
    ON_CALL(*this, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(is_first_policy_load_complete_return));
  }

  // Convenience method that installs an expectation on RefreshPolicies that
  // just notifies the observers and serves the same policies.
  void SetAutoRefresh();

  void RefreshWithSamePolicies();

  // policy::PolicyService::Observer
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

 private:
  void WaitForPoliciesUpdated(policy::PolicyDomain domain);

  SchemaRegistry registry_;
  raw_ptr<policy::PolicyService> policy_service_ = nullptr;
  base::OnceCallback<void()> extension_policies_updated_callback_;
  base::OnceCallback<void()> chrome_policies_updated_callback_;
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

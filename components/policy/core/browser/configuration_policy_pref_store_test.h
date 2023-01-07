// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_PREF_STORE_TEST_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_PREF_STORE_TEST_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

struct PolicyHandlerParameters;
class PolicyMap;
class ConfigurationPolicyPrefStore;

class ConfigurationPolicyPrefStoreTest : public testing::Test {
 public:
  ConfigurationPolicyPrefStoreTest(const ConfigurationPolicyPrefStoreTest&) =
      delete;
  ConfigurationPolicyPrefStoreTest& operator=(
      const ConfigurationPolicyPrefStoreTest&) = delete;

 protected:
  ConfigurationPolicyPrefStoreTest();
  ~ConfigurationPolicyPrefStoreTest() override;
  void TearDown() override;
  void UpdateProviderPolicy(const PolicyMap& policy);

  // A unit test can override this method to populate the policy handler
  // parameters as suited to its needs.
  virtual void PopulatePolicyHandlerParameters(
      PolicyHandlerParameters* parameters);

  PolicyServiceImpl::Providers providers_;
  ConfigurationPolicyHandlerList handler_list_;
  testing::NiceMock<MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<PolicyServiceImpl> policy_service_;
  scoped_refptr<ConfigurationPolicyPrefStore> store_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_PREF_STORE_TEST_H_

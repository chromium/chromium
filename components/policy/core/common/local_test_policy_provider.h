// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_LOCAL_TEST_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_LOCAL_TEST_POLICY_PROVIDER_H_

#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_loader_local_test.h"
#include "components/policy/policy_export.h"
#include "components/version_info/channel.h"

namespace policy {

// The policy provider for testing policies through the policy test page.
// When provider is in use the policies from all other policy providers
// will be disabled.
class POLICY_EXPORT LocalTestPolicyProvider
    : public ConfigurationPolicyProvider {
 public:
  // Creates the PolicyTestPolicyProvider
  static std::unique_ptr<LocalTestPolicyProvider> CreateIfAllowed(
      version_info::Channel channel);

  LocalTestPolicyProvider(const LocalTestPolicyProvider&) = delete;
  LocalTestPolicyProvider& operator=(const LocalTestPolicyProvider&) = delete;

  ~LocalTestPolicyProvider() override;

  // ConfigurationPolicyProvider implementation
  void RefreshPolicies() override;
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;

 private:
  explicit LocalTestPolicyProvider();

  bool first_policies_loaded_;
  PolicyLoaderLocalTest loader_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_LOCAL_TEST_POLICY_PROVIDER_H_

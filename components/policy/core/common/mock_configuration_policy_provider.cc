// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mock_configuration_policy_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "components/policy/core/common/policy_bundle.h"

using testing::Invoke;

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider() {}

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {}

void MockConfigurationPolicyProvider::UpdateChromePolicy(
    const PolicyMap& policy) {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .CopyFrom(policy);
  UpdatePolicy(std::move(bundle));
  if (base::MessageLoopCurrent::IsSet())
    base::RunLoop().RunUntilIdle();
}

void MockConfigurationPolicyProvider::UpdateExtensionPolicy(
    const PolicyMap& policy,
    const std::string& extension_id) {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, extension_id))
      .CopyFrom(policy);
  UpdatePolicy(std::move(bundle));
  if (base::MessageLoopCurrent::IsSet())
    base::RunLoop().RunUntilIdle();
}

void MockConfigurationPolicyProvider::SetAutoRefresh() {
  EXPECT_CALL(*this, RefreshPolicies()).WillRepeatedly(
      Invoke(this, &MockConfigurationPolicyProvider::RefreshWithSamePolicies));
}

void MockConfigurationPolicyProvider::RefreshWithSamePolicies() {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  bundle->CopyFrom(policies());
  UpdatePolicy(std::move(bundle));
}

MockConfigurationPolicyObserver::MockConfigurationPolicyObserver() {}

MockConfigurationPolicyObserver::~MockConfigurationPolicyObserver() {}

}  // namespace policy

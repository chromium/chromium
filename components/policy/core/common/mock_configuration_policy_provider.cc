// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mock_configuration_policy_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"

using testing::Invoke;

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider() = default;

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {
#if BUILDFLAG(IS_ANDROID)
  ShutdownForTesting();
#endif  // BUILDFLAG(IS_ANDROID)
}

void MockConfigurationPolicyProvider::UpdateChromePolicy(
    const PolicyMap& policy) {
  PolicyBundle bundle;
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
      policy.Clone();
  UpdatePolicy(std::move(bundle));
  WaitForPoliciesUpdated(POLICY_DOMAIN_CHROME);
}

void MockConfigurationPolicyProvider::UpdateExtensionPolicy(
    const PolicyMap& policy,
    const std::string& extension_id) {
  PolicyBundle bundle;
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, extension_id)) =
      policy.Clone();
  UpdatePolicy(std::move(bundle));
  WaitForPoliciesUpdated(POLICY_DOMAIN_EXTENSIONS);
}

void MockConfigurationPolicyProvider::SetAutoRefresh() {
  EXPECT_CALL(*this, RefreshPolicies(testing::_))
      .WillRepeatedly(Invoke(
          this, &MockConfigurationPolicyProvider::RefreshWithSamePolicies));
}

void MockConfigurationPolicyProvider::RefreshWithSamePolicies() {
  UpdatePolicy(policies().Clone());
}

void MockConfigurationPolicyProvider::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& previous,
    const policy::PolicyMap& current) {
  policy_service_->RemoveObserver(ns.domain, this);
  if (ns.domain == POLICY_DOMAIN_CHROME) {
    CHECK(chrome_policies_updated_callback_);
    std::move(chrome_policies_updated_callback_).Run();
  }
  if (ns.domain == POLICY_DOMAIN_EXTENSIONS) {
    CHECK(extension_policies_updated_callback_);
    std::move(extension_policies_updated_callback_).Run();
  }
}

void MockConfigurationPolicyProvider::WaitForPoliciesUpdated(
    policy::PolicyDomain domain) {
  if (!policy_service_) {
    bool spin_run_loop = base::CurrentThread::IsSet();
#if BUILDFLAG(IS_IOS)
    // On iOS, the UI message loop does not support RunUntilIdle().
    spin_run_loop &= !base::CurrentUIThread::IsSet();
#endif  // BUILDFLAG(IS_IOS)
    if (spin_run_loop) {
      base::RunLoop().RunUntilIdle();
    }
    return;
  }

  CHECK(!chrome_policies_updated_callback_);
  CHECK(!extension_policies_updated_callback_);
  base::RunLoop loop;
  if (domain == POLICY_DOMAIN_CHROME) {
    chrome_policies_updated_callback_ = loop.QuitClosure();
  } else if (domain == POLICY_DOMAIN_EXTENSIONS) {
    extension_policies_updated_callback_ = loop.QuitClosure();
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  policy_service_->AddObserver(domain, this);
  loop.Run();
}

void MockConfigurationPolicyProvider::SetupPolicyServiceForPolicyUpdates(
    policy::PolicyService* policy_service) {
  CHECK(!chrome_policies_updated_callback_);
  CHECK(!extension_policies_updated_callback_);
  policy_service_ = policy_service;
}

MockConfigurationPolicyObserver::MockConfigurationPolicyObserver() = default;

MockConfigurationPolicyObserver::~MockConfigurationPolicyObserver() = default;

}  // namespace policy

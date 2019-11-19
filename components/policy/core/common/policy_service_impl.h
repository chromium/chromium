// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_export.h"

namespace policy {

class PolicyMap;

class POLICY_EXPORT PolicyServiceImpl
    : public PolicyService,
      public ConfigurationPolicyProvider::Observer {
 public:
  using Providers = std::vector<ConfigurationPolicyProvider*>;

  // Creates a new PolicyServiceImpl with the list of
  // ConfigurationPolicyProviders, in order of decreasing priority.
  explicit PolicyServiceImpl(Providers providers);

  // Creates a new PolicyServiceImpl with the list of
  // ConfigurationPolicyProviders, in order of decreasing priority.
  // The created PolicyServiceImpl will only notify observers that
  // initialization has completed (for any domain) after
  // |UnthrottleInitialization| has been called.
  static std::unique_ptr<PolicyServiceImpl> CreateWithThrottledInitialization(
      Providers providers);

  ~PolicyServiceImpl() override;

  // PolicyService overrides:
  void AddObserver(PolicyDomain domain,
                   PolicyService::Observer* observer) override;
  void RemoveObserver(PolicyDomain domain,
                      PolicyService::Observer* observer) override;
  void AddProviderUpdateObserver(ProviderUpdateObserver* observer) override;
  void RemoveProviderUpdateObserver(ProviderUpdateObserver* observer) override;
  bool HasProvider(ConfigurationPolicyProvider* provider) const override;
  const PolicyMap& GetPolicies(const PolicyNamespace& ns) const override;
  bool IsInitializationComplete(PolicyDomain domain) const override;
  void RefreshPolicies(const base::Closure& callback) override;

  // If this PolicyServiceImpl has been created using
  // |CreateWithThrottledInitialization|, calling UnthrottleInitialization will
  // allow notification of observers that initialization has completed. If
  // initialization has actually completed previously but observers were not
  // notified yet because it was throttled, will notify observers synchronously.
  // Has no effect if initialization was not throttled.
  void UnthrottleInitialization();

 private:
  using Observers =
      base::ObserverList<PolicyService::Observer, true>::Unchecked;

  // This constructor is not publicly visible so callers that want a
  // PolicyServiceImpl with throttled initialization use
  // |CreateWithInitializationThrottled| for clarity.
  // If |initialization_throttled| is true, this PolicyServiceImpl will only
  // notify observers that initialization has completed (for any domain) after
  // |UnthrottleInitialization| has been called.
  PolicyServiceImpl(Providers providers, bool initialization_throttled);

  // ConfigurationPolicyProvider::Observer overrides:
  void OnUpdatePolicy(ConfigurationPolicyProvider* provider) override;

  // Posts a task to notify observers of |ns| that its policies have changed,
  // passing along the |previous| and the |current| policies.
  void NotifyNamespaceUpdated(const PolicyNamespace& ns,
                              const PolicyMap& previous,
                              const PolicyMap& current);

  void NotifyProviderUpdatesPropagated();

  // Combines the policies from all the providers, and notifies the observers
  // of namespaces whose policies have been modified.
  void MergeAndTriggerUpdates();

  // Checks if all providers are initialized and sets |initialization_complete_|
  // accordingly. If initialization is not throttled, will also notify the
  // observers if the service just became initialized.
  void CheckInitializationComplete();

  // If initialization is complete for |policy_domain| and initialization is not
  // throttled, will notify obserers for |policy_domain| that it has been
  // initialized. This function should only be called when |policy_domain| just
  // became initialized or when initialization has been unthrottled.
  void MaybeNotifyInitializationComplete(PolicyDomain policy_domain);

  // Invokes all the refresh callbacks if there are no more refreshes pending.
  void CheckRefreshComplete();

  // The providers, in order of decreasing priority.
  Providers providers_;

  // Maps each policy namespace to its current policies.
  PolicyBundle policy_bundle_;

  // Maps each policy domain to its observer list.
  std::map<PolicyDomain, std::unique_ptr<Observers>> observers_;

  // True if all the providers are initialized for the indexed policy domain.
  bool initialization_complete_[POLICY_DOMAIN_SIZE];

  // Set of providers that have a pending update that was triggered by a
  // call to RefreshPolicies().
  std::set<ConfigurationPolicyProvider*> refresh_pending_;

  // List of callbacks to invoke once all providers refresh after a
  // RefreshPolicies() call.
  std::vector<base::Closure> refresh_callbacks_;

  // Observers for propagation of policy updates by
  // ConfigurationPolicyProviders.
  base::ObserverList<ProviderUpdateObserver> provider_update_observers_;

  // Contains all ConfigurationPolicyProviders that have signaled a policy
  // update which is still being processed (i.e. for which a notification to
  // |provider_update_observers_| has not been sent out yet).
  // Note that this is intentionally a set - if multiple updates from the same
  // provider come in faster than they can be processed, they should only
  // trigger one notification to |provider_update_observers_|.
  std::set<ConfigurationPolicyProvider*> provider_update_pending_;

  // If this is true, IsInitializationComplete should be returning false for all
  // policy domains because the owner of this PolicyService is delaying the
  // initialization signal.
  bool initialization_throttled_;

  // Used to verify thread-safe usage.
  base::ThreadChecker thread_checker_;

  // Used to create tasks to delay new policy updates while we may be already
  // processing previous policy updates.
  base::WeakPtrFactory<PolicyServiceImpl> update_task_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PolicyServiceImpl);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_IMPL_H_

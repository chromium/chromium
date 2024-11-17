// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"

namespace policy {

class ConfigurationPolicyProvider;

#if BUILDFLAG(IS_ANDROID)
namespace android {
class PolicyServiceAndroid;
}
#endif

// The PolicyService merges policies from all available sources, taking into
// account their priorities. Policy clients can retrieve policy for their domain
// and register for notifications on policy updates.
//
// The PolicyService is available from BrowserProcess as a global singleton.
// There is also a PolicyService for browser-wide policies available from
// BrowserProcess as a global singleton.
class POLICY_EXPORT PolicyService {
 public:
  class POLICY_EXPORT Observer : public base::CheckedObserver {
   public:
    // Invoked whenever policies for the given |ns| namespace are modified.
    // This is only invoked for changes that happen after AddObserver is called.
    // |previous| contains the values of the policies before the update,
    // and |current| contains the current values.
    virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                                 const PolicyMap& previous,
                                 const PolicyMap& current) {}

    // Invoked at most once for each |domain|, when the PolicyService becomes
    // ready. If IsInitializationComplete() is false, then this will be invoked
    // once all the policy providers have finished loading their policies for
    // |domain|. This does not handle failure to load policies from some
    // providers, so it is possible for the policy service to be initialised
    // if the providers failed for example to load its policies cache.
    virtual void OnPolicyServiceInitialized(PolicyDomain domain) {}

    // Invoked at most once for each |domain|, when the PolicyService becomes
    // ready. If IsFirstPolicyLoadComplete() is false, then this will be invoked
    // once all the policy providers have finished loading their policies for
    // |domain|. The difference from |OnPolicyServiceInitialized| is that this
    // will wait for cloud policies to be fetched when the local cache is not
    // available, which may take some time depending on user's network.
    virtual void OnFirstPoliciesLoaded(PolicyDomain domain) {}
  };

  class POLICY_EXPORT ProviderUpdateObserver : public base::CheckedObserver {
   public:
    // Invoked when the contents of a policy update signaled by |provider| are
    // available through PolicyService::GetPolicies.
    // This is intentionally also called if the policy update signaled by
    // |provider| did not change the effective policy values. Note that multiple
    // policy updates by |provider| can result in a single call to this
    // function, e.g. if a subsequent policy update is signaled before the
    // previous one has been processed by the PolicyService.
    // Also note that when this is called, PolicyService's Observers may not
    // have been called with the update that triggered this call yet.
    virtual void OnProviderUpdatePropagated(
        ConfigurationPolicyProvider* provider) = 0;
  };

  virtual ~PolicyService() = default;

  // Observes changes to all components of the given |domain|.
  virtual void AddObserver(PolicyDomain domain, Observer* observer) = 0;

  virtual void RemoveObserver(PolicyDomain domain, Observer* observer) = 0;

  // Observes propagation of policy updates by ConfigurationPolicyProviders.
  virtual void AddProviderUpdateObserver(ProviderUpdateObserver* observer) = 0;
  virtual void RemoveProviderUpdateObserver(
      ProviderUpdateObserver* observer) = 0;

  // Returns true if this PolicyService uses |provider| as one of its sources of
  // policies.
  virtual bool HasProvider(ConfigurationPolicyProvider* provider) const = 0;

  virtual const PolicyMap& GetPolicies(const PolicyNamespace& ns) const = 0;

  // The PolicyService loads policy from several sources, and some require
  // asynchronous loads. IsInitializationComplete() returns true once all
  // sources have been initialized for the given |domain|.
  // It is safe to read policy from the PolicyService even if
  // IsInitializationComplete() is false; there will be an OnPolicyUpdated()
  // notification once new policies become available.
  //
  // OnPolicyServiceInitialized() is called when IsInitializationComplete()
  // becomes true, which happens at most once for each domain.
  // If IsInitializationComplete() is already true for |domain| when an Observer
  // is registered, then that Observer will not receive an
  // OnPolicyServiceInitialized() notification.
  virtual bool IsInitializationComplete(PolicyDomain domain) const = 0;

  // The PolicyService loads policy from several sources, and some require
  // asynchronous loads. IsFirstPolicyLoadComplete() returns true once all
  // sources have loaded their initial policies for the given |domain|.
  // It is safe to read policy from the PolicyService even if
  // IsFirstPolicyLoadComplete() is false; there will be an OnPolicyUpdated()
  // notification once new policies become available.
  //
  // OnFirstPoliciesLoaded() is called when IsFirstPolicyLoadComplete()
  // becomes true, which happens at most once for each domain.
  // If IsFirstPolicyLoadComplete() is already true for |domain| when an
  // Observer is registered, then that Observer will not receive an
  // OnFirstPoliciesLoaded() notification.
  virtual bool IsFirstPolicyLoadComplete(PolicyDomain domain) const = 0;

  // Asks the PolicyService to reload policy from all available policy sources.
  // |callback| is invoked once every source has reloaded its policies, and
  // GetPolicies() is guaranteed to return the updated values at that point.
  virtual void RefreshPolicies(base::OnceClosure callback,
                               PolicyFetchReason reason) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Get the PolicyService JNI bridge instance.
  virtual android::PolicyServiceAndroid* GetPolicyServiceAndroid() = 0;
#endif
  virtual void UseLocalTestPolicyProvider(
      ConfigurationPolicyProvider* provider) = 0;
};

// A registrar that only observes changes to particular policies within the
// PolicyMap for the given policy namespace.
class POLICY_EXPORT PolicyChangeRegistrar : public PolicyService::Observer {
 public:
  typedef base::RepeatingCallback<void(const base::Value*, const base::Value*)>
      UpdateCallback;

  // Observes updates to the given (domain, component_id) namespace in the given
  // |policy_service|, and notifies |observer| whenever any of the registered
  // policy keys changes. Both the |policy_service| and the |observer| must
  // outlive |this|.
  PolicyChangeRegistrar(PolicyService* policy_service,
                        const PolicyNamespace& ns);
  PolicyChangeRegistrar(const PolicyChangeRegistrar&) = delete;
  PolicyChangeRegistrar& operator=(const PolicyChangeRegistrar&) = delete;

  ~PolicyChangeRegistrar() override;

  // Will invoke |callback| whenever |policy_name| changes its value, as long
  // as this registrar exists.
  // Only one callback can be registed per policy name; a second call with the
  // same |policy_name| will overwrite the previous callback.
  void Observe(const std::string& policy_name, const UpdateCallback& callback);

  // Implementation of PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

 private:
  typedef std::map<std::string, UpdateCallback> CallbackMap;

  raw_ptr<PolicyService> policy_service_;
  PolicyNamespace ns_;
  CallbackMap callback_map_;
};

}  // namespace policy

namespace base {

template <>
struct ScopedObservationTraits<policy::PolicyService,
                               policy::PolicyService::ProviderUpdateObserver> {
  static void AddObserver(
      policy::PolicyService* source,
      policy::PolicyService::ProviderUpdateObserver* observer) {
    source->AddProviderUpdateObserver(observer);
  }
  static void RemoveObserver(
      policy::PolicyService* source,
      policy::PolicyService::ProviderUpdateObserver* observer) {
    source->RemoveProviderUpdateObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_H_

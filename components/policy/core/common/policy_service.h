// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list_types.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace policy {

class ConfigurationPolicyProvider;

// The PolicyService merges policies from all available sources, taking into
// account their priorities. Policy clients can retrieve policy for their domain
// and register for notifications on policy updates.
//
// The PolicyService is available from BrowserProcess as a global singleton.
// There is also a PolicyService for browser-wide policies available from
// BrowserProcess as a global singleton.
class POLICY_EXPORT PolicyService {
 public:
  class POLICY_EXPORT Observer {
   public:
    // Invoked whenever policies for the given |ns| namespace are modified.
    // This is only invoked for changes that happen after AddObserver is called.
    // |previous| contains the values of the policies before the update,
    // and |current| contains the current values.
    virtual void OnPolicyUpdated(const PolicyNamespace& ns,
                                 const PolicyMap& previous,
                                 const PolicyMap& current) = 0;

    // Invoked at most once for each |domain|, when the PolicyService becomes
    // ready. If IsInitializationComplete() is false, then this will be invoked
    // once all the policy providers have finished loading their policies for
    // |domain|.
    virtual void OnPolicyServiceInitialized(PolicyDomain domain) {}

   protected:
    virtual ~Observer() {}
  };

  class POLICY_EXPORT ProviderUpdateObserver : public base::CheckedObserver {
   public:
    // Invoked when a policy update signaled by |provider| has been propagated
    // to the PolicyService's Observers and its contents are now available
    // through PolicyService::GetPolicies. This is intentionally also called if
    // the policy update signaled by |provider| did not change the effective
    // policy values. Note that multiple policy updates by |provider| can result
    // in a single call to this function, e.g. if a subsequent policy update is
    // signaled before the previous one has been processed by the PolicyService.
    virtual void OnProviderUpdatePropagated(
        ConfigurationPolicyProvider* provider) = 0;
  };

  virtual ~PolicyService() {}

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
  // sources have loaded their policies for the given |domain|.
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

  // Asks the PolicyService to reload policy from all available policy sources.
  // |callback| is invoked once every source has reloaded its policies, and
  // GetPolicies() is guaranteed to return the updated values at that point.
  virtual void RefreshPolicies(const base::Closure& callback) = 0;
};

// A registrar that only observes changes to particular policies within the
// PolicyMap for the given policy namespace.
class POLICY_EXPORT PolicyChangeRegistrar : public PolicyService::Observer {
 public:
  typedef base::Callback<void(const base::Value*,
                              const base::Value*)> UpdateCallback;

  // Observes updates to the given (domain, component_id) namespace in the given
  // |policy_service|, and notifies |observer| whenever any of the registered
  // policy keys changes. Both the |policy_service| and the |observer| must
  // outlive |this|.
  PolicyChangeRegistrar(PolicyService* policy_service,
                        const PolicyNamespace& ns);

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

  PolicyService* policy_service_;
  PolicyNamespace ns_;
  CallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyChangeRegistrar);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_PREF_STORE_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_PREF_STORE_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_store.h"

class PrefValueMap;

namespace policy {

class BrowserPolicyConnectorBase;
class ConfigurationPolicyHandlerList;

// An implementation of PrefStore that bridges policy settings as read from the
// PolicyService to preferences. Converts POLICY_DOMAIN_CHROME policies a given
// PolicyLevel to their corresponding preferences.
class POLICY_EXPORT ConfigurationPolicyPrefStore
    : public PrefStore,
      public PolicyService::Observer {
 public:
  // Does not take ownership of |service| nor |handler_list|, which must outlive
  // the store. Only policies of the given |level| will be mapped.
  ConfigurationPolicyPrefStore(
      BrowserPolicyConnectorBase* policy_connector,
      PolicyService* service,
      const ConfigurationPolicyHandlerList* handler_list,
      PolicyLevel level);
  ConfigurationPolicyPrefStore(const ConfigurationPolicyPrefStore&) = delete;
  ConfigurationPolicyPrefStore& operator=(const ConfigurationPolicyPrefStore&) =
      delete;

  // PrefStore methods:
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;
  bool GetValue(std::string_view key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;

  // PolicyService::Observer methods:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;
  void OnPolicyServiceInitialized(PolicyDomain domain) override;

 private:
  ~ConfigurationPolicyPrefStore() override;

  // Refreshes policy information, rereading policy from the policy service and
  // sending out change notifications as appropriate.
  void Refresh();

  // Returns a new PrefValueMap containing the preference values that correspond
  // to the policies currently provided by the policy service.
  std::unique_ptr<PrefValueMap> CreatePreferencesFromPolicies();

  // May be null in tests.
  raw_ptr<BrowserPolicyConnectorBase> policy_connector_;

  // The PolicyService from which policy settings are read.
  raw_ptr<PolicyService> policy_service_;

  // The policy handlers used to convert policies into their corresponding
  // preferences.
  raw_ptr<const ConfigurationPolicyHandlerList> handler_list_;

  // The policy level that this PrefStore uses.
  PolicyLevel level_;

  // Current policy preferences.
  std::unique_ptr<PrefValueMap> prefs_;

  base::ObserverList<PrefStore::Observer, true> observers_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CONFIGURATION_POLICY_PREF_STORE_H_

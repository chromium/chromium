// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/enterprise/net/core/proxy_provisioning_domain_manager.h"
#include "components/enterprise/net/core/types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_net {

class EnterpriseNetworkAuthService;

// State-machine service responsible for managing multiple
// ProxyProvisioningDomainManager, each of which maintain up-to-date proxy
// configurations fetched from their configured Provisioning Domain (PvD).
class EnterpriseProxyService : public KeyedService,
                               public ProxyProvisioningDomainManager::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when dynamic proxy route configurations have been resolved.
    virtual void OnAllDynamicProxyConfigsResolved(
        const std::vector<ProvisioningDomainProxyConfig>& configs) = 0;

    // Called when dynamic proxy route updates start and no refresh was
    // previously in progress.
    virtual void OnDynamicProxyConfigsUpdateInProgress() = 0;
  };

  using GetURLLoaderFactoryCallback =
      base::RepeatingCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  EnterpriseProxyService(
      PrefService* pref_service,
      EnterpriseNetworkAuthService* auth_service,
      GetURLLoaderFactoryCallback url_loader_factory_callback,
      enterprise::ProfileIdService* profile_id_service = nullptr);

  EnterpriseProxyService(const EnterpriseProxyService&) = delete;
  EnterpriseProxyService& operator=(const EnterpriseProxyService&) = delete;

  ~EnterpriseProxyService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the list of current PvD configs with their states.
  std::vector<ProvisioningDomainProxyConfig> GetProvisioningDomainConfigs()
      const;

  // Returns true if there is at least one background refresh currently running.
  bool IsRefreshInProgress() const;

  // Looks up the first matching `ProxyEndpoint` across all active Provisioning
  // Domain configs for a given `destination_url` and `proxy_chain`.
  // Returns std::nullopt if no matching endpoint is found.
  std::optional<ProvisioningDomainProxyConfig::ProxyEndpoint>
  FindMatchingProxyEndpoint(const GURL& destination_url,
                            const net::ProxyChain& proxy_chain) const;

  // Returns the `net::ProxyConfig::DynamicRoutingConfig` concatenated
  // from all valid active Provisioning Domain configs, with ordering strictly
  // preserved and all browser-side PvD metadata removed.
  net::ProxyConfig::DynamicRoutingConfig GetDynamicRoutingConfig() const;

  // KeyedService:
  void Shutdown() override;

  // ProxyProvisioningDomainManager::Observer:
  void OnProvisioningDomainStateChanged(
      ProxyProvisioningDomainManager* domain_manager) override;

  // Forces a new fetch for all managed Provisioning Domains.
  void ForceRefreshAllConfigs();

  // Returns a dictionary containing detailed debugging information for all
  // managed Provisioning Domains and active fetch states.
  base::DictValue GetDebugInfo() const;

 private:
  friend class EnterpriseProxyServiceTest;

  void OnPolicyPrefChanged();

  // Recreates the managed `ProxyProvisioningDomainManager` instances from the
  // "ProxyProvisioningDomains" policy list. Clears all current managers and
  // creates new ones for each policy entry.
  // TODO(crbug.com/526587734): Incorporate preference caching to restore cached
  // Provisioning Domain configs on startup and preserve/move cached configs for
  // existing domain managers across policy updates (even if their index
  // changes).
  void RecreateProvisioningDomainManagers(
      const base::ListValue& policy_domains);

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<EnterpriseNetworkAuthService> auth_service_;

  // Callback used to obtain a SharedURLLoaderFactory lazily when creating
  // ProvisioningDomainManagers. A callback is used instead of a static
  // SharedURLLoaderFactory pointer because Profile StoragePartition
  // initialization may not be ready at KeyedService creation time.
  GetURLLoaderFactoryCallback url_loader_factory_callback_;

  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;

  // In-memory list of provisioning domain state machines matching policy.
  std::vector<std::unique_ptr<ProxyProvisioningDomainManager>>
      provisioning_domain_managers_;

  // Set of managers currently executing a background fetch.
  base::flat_set<raw_ptr<ProxyProvisioningDomainManager>> refreshing_managers_;

  base::WeakPtrFactory<EnterpriseProxyService> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_

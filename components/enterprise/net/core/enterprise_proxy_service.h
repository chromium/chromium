// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/enterprise/net/core/types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_net {

// Service responsible for managing the Provisioning Domain (PvD) JSON
// configurations from the corresponding well-known endpoints, defined in the
// "ProxyProvisioningDomains" policy.
class EnterpriseProxyService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the active PvD configurations change, or when background
    // fetching starts.
    virtual void OnProvisioningDomainConfigsChanged(
        const std::vector<ProvisioningDomainProxyConfig>& configs,
        bool fetch_in_progress) = 0;
  };

  using GetURLLoaderFactoryCallback =
      base::RepeatingCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  EnterpriseProxyService(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
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

  // Returns true if there is at least one background fetch currently running.
  bool IsFetchInProgress() const;

 private:
  class ProvisioningDomainFetcher;

  struct ProxyProvisioningDomain {
    ProxyProvisioningDomain();
    ProxyProvisioningDomain(ProxyProvisioningDomain&&) noexcept;
    ProxyProvisioningDomain& operator=(ProxyProvisioningDomain&&) noexcept;
    ~ProxyProvisioningDomain();

    ProvisioningDomainConfig policy;
    std::unique_ptr<ProvisioningDomainFetcher> fetcher;

    ProvisioningDomainProxyConfig fetched_config;
  };

  void Shutdown() override;

  void OnPolicyPrefChanged();
  void OnFetchComplete(
      ProvisioningDomainFetcher* fetcher,
      std::optional<ProvisioningDomainProxyConfig> parsed_config);

  // Updates the cached configurations in preferences and notifies observers of
  // any changes.
  void NotifyAndUpdateCachedConfigs();

  // Rebuilds the in-memory storage to make sure it's accurate and reuse caches.
  void RebuildProvisioningDomains(const base::ListValue& policy_domains);

  raw_ptr<PrefService> pref_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  GetURLLoaderFactoryCallback url_loader_factory_callback_;
  raw_ptr<enterprise::ProfileIdService> profile_id_service_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;

  // In-memory list of provisioning domains matching the policy.
  std::vector<ProxyProvisioningDomain> provisioning_domains_;

  // TODO(crbug.com/507060663): We should have an in-memory copy of the routing
  // rules with network-defined C++ type as well.
  // This prevents us from excessive parsing/type casting for values that are
  // not updated.

  int in_progress_fetches_ = 0;

  base::WeakPtrFactory<EnterpriseProxyService> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_

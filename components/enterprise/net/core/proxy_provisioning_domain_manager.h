// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_PROXY_PROVISIONING_DOMAIN_MANAGER_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_PROXY_PROVISIONING_DOMAIN_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/enterprise/net/core/provisioning_domain_fetcher.h"
#include "components/enterprise/net/core/types.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise_net {

class EnterpriseNetworkAuthService;
class ProvisioningDomainFetcher;

// Autonomous state machine managing the lifecycle, refreshing, and route
// preservation of a single Provisioning Domain (PvD).
class ProxyProvisioningDomainManager {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the managed Provisioning Domain's state changes.
    virtual void OnProvisioningDomainStateChanged(
        ProxyProvisioningDomainManager* domain_manager) = 0;
  };

  using GetURLLoaderFactoryCallback =
      base::RepeatingCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  ProxyProvisioningDomainManager(
      const ProvisioningDomainConfig& policy,
      EnterpriseNetworkAuthService* auth_service,
      GetURLLoaderFactoryCallback url_loader_factory_callback);
  ProxyProvisioningDomainManager(const ProxyProvisioningDomainManager&) =
      delete;
  ProxyProvisioningDomainManager& operator=(
      const ProxyProvisioningDomainManager&) = delete;
  ~ProxyProvisioningDomainManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Forces a new refresh for this Provisioning Domain.
  // Cancels any in-flight refresh before starting a new one.
  // Used in cases such as refreshing configs upon network change.
  void ForceRefresh();

  // Cancels any in-flight refresh workflow.
  void CancelRefresh();

  const ProvisioningDomainConfig& policy() const { return policy_; }
  const ProvisioningDomainProxyConfig& fetched_config() const {
    return fetched_config_;
  }
  ProvisioningDomainProxyConfig::State state() const {
    return fetched_config_.state;
  }
  bool is_refresh_in_progress() const { return fetcher_ != nullptr; }

  // Returns a dictionary containing detailed information (policy config,
  // fetched config, and state) for the PvD maintained by this manager.
  base::DictValue GetDebugInfo() const;

 private:
  // Initiates a casual refresh for this Provisioning Domain if one is not
  // already in-progress. Scheduled internally on TTL expiration or creation.
  void Refresh();

  void StartRefreshInternal();
  void OnRefreshComplete(ProvisioningDomainFetchResult result);
  void NotifyIfStateChanged();

  const ProvisioningDomainConfig policy_;
  ProvisioningDomainProxyConfig fetched_config_;

  const raw_ptr<EnterpriseNetworkAuthService> auth_service_;
  GetURLLoaderFactoryCallback url_loader_factory_callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ProvisioningDomainFetcher> fetcher_;

  std::optional<ProvisioningDomainProxyConfig::State> last_notified_state_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<ProxyProvisioningDomainManager> weak_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_PROXY_PROVISIONING_DOMAIN_MANAGER_H_

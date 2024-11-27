// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkAnonymizationKey;
class ProxyChain;

}  // namespace net

namespace ip_protection {

class IpProtectionProxyConfigManager;
class IpProtectionTokenManager;
class MaskedDomainListManager;
enum class ProxyLayer;

// The generic implementation of IpProtectionCore. Subclasses provide additional
// functionality for specific circumstances, such as interaction with other
// processes via IPC.
class IpProtectionCoreImpl
    : public IpProtectionCore,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  IpProtectionCoreImpl(
      MaskedDomainListManager* masked_domain_list_manager,
      std::unique_ptr<IpProtectionProxyConfigManager>
          ip_protection_proxy_config_manager,
      std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>
          ip_protection_token_managers,
      bool is_ip_protection_enabled);
  ~IpProtectionCoreImpl() override;

  // IpProtectionCore implementation.
  bool IsMdlPopulated() override;
  bool RequestShouldBeProxied(
      const GURL& request_url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;
  bool IsIpProtectionEnabled() override;
  bool AreAuthTokensAvailable() override;
  bool WereTokenCachesEverFilled() override;
  std::optional<BlindSignedAuthToken> GetAuthToken(size_t chain_index) override;
  bool IsProxyListAvailable() override;
  void QuicProxiesFailed() override;
  std::vector<net::ProxyChain> GetProxyChainList() override;
  void RequestRefreshProxyList() override;
  void GeoObserved(const std::string& geo_id) override;

  IpProtectionTokenManager* GetIpProtectionTokenManagerForTesting(
      ProxyLayer proxy_layer);
  IpProtectionProxyConfigManager* GetIpProtectionProxyConfigManagerForTesting();

  // `NetworkChangeNotifier::NetworkChangeObserver` implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 protected:
  // Set the enabled status of IP Protection.
  void set_ip_protection_enabled(bool enabled);
  bool is_ip_protection_enabled() { return is_ip_protection_enabled_; }

  std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>&
  ip_protection_token_managers() {
    return ipp_token_managers_;
  }

 private:
  // The MDL manager, owned by the NetworkService.
  raw_ptr<MaskedDomainListManager> masked_domain_list_manager_;

  // A manager for the list of currently cached proxy hostnames.
  std::unique_ptr<IpProtectionProxyConfigManager> ipp_proxy_config_manager_;

  // Proxy layer managers for cache of blind-signed auth tokens.
  std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>
      ipp_token_managers_;

  bool is_ip_protection_enabled_;

  // If true, this class will try to connect to IP Protection proxies via QUIC.
  // Once this value becomes false, it stays false until a network change or
  // browser restart.
  bool ipp_over_quic_;

  // Feature flag to safely introduce token caching by geo.
  const bool enable_token_caching_by_geo_;

  base::WeakPtrFactory<IpProtectionCoreImpl> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_H_

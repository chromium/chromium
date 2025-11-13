// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "net/base/network_change_notifier.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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
  using ProxyTokenManagerMap =
      absl::flat_hash_map<ProxyLayer,
                          std::unique_ptr<IpProtectionTokenManager>>;
  using InitialTokensMap =
      base::flat_map<ProxyLayer, std::vector<BlindSignedAuthToken>>;

  IpProtectionCoreImpl(
      MaskedDomainListManager* masked_domain_list_manager,
      std::unique_ptr<IpProtectionProxyConfigManager>
          ip_protection_proxy_config_manager,
      ProxyTokenManagerMap ip_protection_token_managers,
      bool is_ip_protection_enabled,
      bool ip_protection_incognito);
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
  bool HasTrackingProtectionException(
      const GURL& first_party_url) const override;
  void SetTrackingProtectionContentSetting(
      const ContentSettingsForOneType& settings) override;

  IpProtectionTokenManager* GetIpProtectionTokenManagerForTesting(
      ProxyLayer proxy_layer);
  IpProtectionProxyConfigManager* GetIpProtectionProxyConfigManagerForTesting();
  std::optional<BlindSignedAuthToken> GetAuthTokenForTesting(
      ProxyLayer proxy_layer,
      const std::string& geo_id);

  // `NetworkChangeNotifier::NetworkChangeObserver` implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  void RecordTokenDemand(size_t chain_index) override;

 protected:
  // Set the enabled status of IP Protection.
  void set_ip_protection_enabled(bool enabled);
  bool is_ip_protection_enabled() { return is_ip_protection_enabled_; }

  ProxyTokenManagerMap& ip_protection_token_managers() {
    return ipp_token_managers_;
  }

 private:
  // The MDL manager, owned by the NetworkService.
  raw_ptr<MaskedDomainListManager> masked_domain_list_manager_;

  // A manager for the list of currently cached proxy hostnames.
  std::unique_ptr<IpProtectionProxyConfigManager> ipp_proxy_config_manager_;

  // Proxy layer managers for cache of blind-signed auth tokens.
  ProxyTokenManagerMap ipp_token_managers_;

  bool is_ip_protection_enabled_;

  // If true, this class will try to connect to IP Protection proxies via QUIC.
  // Once this value becomes false, it stays false until a network change or
  // browser restart.
  bool ipp_over_quic_;

  // Number of requests made with QUIC proxies. This is used to generate metrics
  // regarding fallback to H2/H1.
  int quic_requests_ = 0;

  MdlType mdl_type_;

  // List of TRACKING_PROTECTION content setting exceptions.
  std::vector<content_settings::HostIndexedContentSettings>
      tp_content_settings_;

  base::WeakPtrFactory<IpProtectionCoreImpl> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_H_

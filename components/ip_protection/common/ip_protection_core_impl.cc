// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_core_impl.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "components/ip_protection/common/ip_protection_token_manager_impl.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace ip_protection {

namespace {
// Rewrite the proxy list to use SCHEME_QUIC. In order to fall back to HTTPS
// quickly if QUIC is broken, the first chain is included once with
// SCHEME_QUIC and once with SCHEME_HTTPS. The remaining chains are included
// only with SCHEME_QUIC.
std::vector<net::ProxyChain> MakeQuicProxyList(
    const std::vector<net::ProxyChain>& proxy_list,
    bool include_https_fallback) {
  if (proxy_list.empty()) {
    return proxy_list;
  }
  auto to_quic = [](const net::ProxyChain& proxy_chain) {
    std::vector<net::ProxyServer> quic_servers;
    quic_servers.reserve(proxy_chain.length());
    for (auto& proxy_server : proxy_chain.proxy_servers()) {
      CHECK(proxy_server.is_https());
      quic_servers.emplace_back(net::ProxyServer::Scheme::SCHEME_QUIC,
                                proxy_server.host_port_pair());
    }
    auto quic_proxy_chain = net::ProxyChain::ForIpProtection(
        std::move(quic_servers), proxy_chain.ip_protection_chain_id());
    // The proxy chains passed to this function are assumed to be valid (
    // validated by the `IpProtectionProxyConfigFetcher()` that created them),
    // so creating a new QUIC proxy chain from those should also result in valid
    // proxy chains.
    CHECK(quic_proxy_chain.IsValid());
    return quic_proxy_chain;
  };

  std::vector<net::ProxyChain> quic_proxy_list;
  quic_proxy_list.reserve(proxy_list.size() + (include_https_fallback ? 1 : 0));
  quic_proxy_list.push_back(to_quic(proxy_list[0]));
  if (include_https_fallback) {
    quic_proxy_list.push_back(proxy_list[0]);
  }

  for (size_t i = 1; i < proxy_list.size(); i++) {
    quic_proxy_list.push_back(to_quic(proxy_list[i]));
  }

  return quic_proxy_list;
}

}  // namespace

IpProtectionCoreImpl::IpProtectionCoreImpl(
    MaskedDomainListManager* masked_domain_list_manager,
    std::unique_ptr<IpProtectionProxyConfigManager>
        ip_protection_proxy_config_manager,
    ProxyTokenManagerMap ip_protection_token_managers,
    bool is_ip_protection_enabled,
    bool ip_protection_incognito)
    : masked_domain_list_manager_(masked_domain_list_manager),
      ipp_proxy_config_manager_(std::move(ip_protection_proxy_config_manager)),
      ipp_token_managers_(std::move(ip_protection_token_managers)),
      is_ip_protection_enabled_(is_ip_protection_enabled),
      ipp_over_quic_(net::features::kIpPrivacyUseQuicProxies.Get()),
      mdl_type_(network::features::kSplitMaskedDomainList.Get()
                    ? (ip_protection_incognito ? MdlType::kIncognito
                                               : MdlType::kRegularBrowsing)
                    : MdlType::kIncognito) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

IpProtectionCoreImpl::~IpProtectionCoreImpl() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

bool IpProtectionCoreImpl::IsMdlPopulated() {
  return masked_domain_list_manager_->IsPopulated();
}

bool IpProtectionCoreImpl::RequestShouldBeProxied(
    const GURL& request_url,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  base::ElapsedTimer matches_call;
  bool should_be_proxied = masked_domain_list_manager_->Matches(
      request_url, network_anonymization_key, mdl_type_);
  Telemetry().MdlMatchesTime(matches_call.Elapsed());
  return should_be_proxied;
}

bool IpProtectionCoreImpl::IsIpProtectionEnabled() {
  return is_ip_protection_enabled_;
}

bool IpProtectionCoreImpl::AreAuthTokensAvailable() {
  // If proxy list is not available, tokens cannot be available. Also if there
  // are no token cache managers, there are no tokens.
  if (!IsProxyListAvailable() || ipp_token_managers_.empty()) {
    return false;
  }

  bool all_caches_have_tokens = true;
  for (const auto& manager : ipp_token_managers_) {
    if (!manager.second->IsAuthTokenAvailable(
            ipp_proxy_config_manager_->CurrentGeo())) {
      // Only emit metric if the cache was ever filled.
      if (manager.second->WasTokenCacheEverFilled()) {
        Telemetry().EmptyTokenCache(manager.first);
      }
      all_caches_have_tokens = false;
    }
  }

  return all_caches_have_tokens;
}

bool IpProtectionCoreImpl::WereTokenCachesEverFilled() {
  // If proxy list is not available, tokens cannot be available. Also if there
  // are no token cache managers, there are no tokens.
  if (!IsProxyListAvailable() || ipp_token_managers_.empty()) {
    return false;
  }

  bool all_caches_have_been_filled = true;
  for (const auto& manager : ipp_token_managers_) {
    if (!manager.second->WasTokenCacheEverFilled()) {
      all_caches_have_been_filled = false;
    }
  }
  return all_caches_have_been_filled;
}

std::optional<BlindSignedAuthToken> IpProtectionCoreImpl::GetAuthToken(
    size_t chain_index) {
  std::optional<BlindSignedAuthToken> result;

  // If the proxy list is empty, there cannot be any matching tokens.
  if (!IsProxyListAvailable() || ipp_token_managers_.empty()) {
    return result;
  }

  auto it = ipp_token_managers_.find(chain_index == 0 ? ProxyLayer::kProxyA
                                                      : ProxyLayer::kProxyB);
  if (it != ipp_token_managers_.end()) {
    result = it->second->GetAuthToken(ipp_proxy_config_manager_->CurrentGeo());
  }
  return result;
}

IpProtectionTokenManager*
IpProtectionCoreImpl::GetIpProtectionTokenManagerForTesting(
    ProxyLayer proxy_layer) {
  return ipp_token_managers_[proxy_layer].get();
}

IpProtectionProxyConfigManager*
IpProtectionCoreImpl::GetIpProtectionProxyConfigManagerForTesting() {
  return ipp_proxy_config_manager_.get();
}

std::optional<BlindSignedAuthToken>
IpProtectionCoreImpl::GetAuthTokenForTesting(ProxyLayer proxy_layer,
                                             const std::string& geo_id) {
  auto it = ipp_token_managers_.find(proxy_layer);
  if (it == ipp_token_managers_.end()) {
    return std::nullopt;
  }
  return it->second->GetAuthToken(geo_id);
}

bool IpProtectionCoreImpl::IsProxyListAvailable() {
  return ipp_proxy_config_manager_ &&
         ipp_proxy_config_manager_->IsProxyListAvailable();
}

void IpProtectionCoreImpl::QuicProxiesFailed() {
  if (ipp_over_quic_) {
    Telemetry().QuicProxiesFailed(quic_requests_);
  }
  ipp_over_quic_ = false;
}

std::vector<net::ProxyChain> IpProtectionCoreImpl::GetProxyChainList() {
  if (ipp_proxy_config_manager_ == nullptr) {
    return {};
  }
  std::vector<net::ProxyChain> proxy_list =
      ipp_proxy_config_manager_->ProxyList();

  bool ipp_over_quic_only = net::features::kIpPrivacyUseQuicProxiesOnly.Get();
  if (ipp_over_quic_ || ipp_over_quic_only) {
    quic_requests_++;
    proxy_list = MakeQuicProxyList(
        proxy_list, /*include_https_fallback=*/!ipp_over_quic_only);
  }

  return proxy_list;
}

void IpProtectionCoreImpl::RequestRefreshProxyList() {
  if (ipp_proxy_config_manager_ != nullptr) {
    ipp_proxy_config_manager_->RequestRefreshProxyList();
  }
}

void IpProtectionCoreImpl::GeoObserved(const std::string& geo_id) {
  if (ipp_proxy_config_manager_ != nullptr &&
      ipp_proxy_config_manager_->CurrentGeo() != geo_id) {
    ipp_proxy_config_manager_->RequestRefreshProxyList();
  }

  for (auto& [_, token_manager] : ipp_token_managers_) {
    if (token_manager->CurrentGeo() != geo_id) {
      token_manager->SetCurrentGeo(geo_id);
    }
  }
}

void IpProtectionCoreImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // When the network changes, but there is still a network, reset the
  // tracking of whether QUIC proxies work, and try to fetch a new proxy list.
  if (type != net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE) {
    ipp_over_quic_ = net::features::kIpPrivacyUseQuicProxies.Get();
    quic_requests_ = 0;
    RequestRefreshProxyList();
  }
}

void IpProtectionCoreImpl::set_ip_protection_enabled(bool enabled) {
  is_ip_protection_enabled_ = enabled;
  // TODO(crbug.com/41494110): Tear down all existing proxied
  // HTTP/SPDY/QUIC sessions if the settings goes from being enabled to being
  // disabled. For HTTP and SPDY we could just simulate an IP address change and
  // tear down all connections rather easily, but for QUIC it's more complicated
  // because with network change session migration the connections might still
  // persist. More investigation is needed here.
  // TODO(crbug.com/41494110): Propagate this change to the config cache,
  // proxy list manager, and token cache manager to cancel further requests or
  // reschedule them. Note that as currently implemented, the token cache
  // manager will already stop requesting tokens soon after IP Protection is
  // disabled via the try again after time returned by the next TryGetAuthToken
  // call, but the GetProxyConfig calls will continue and receive failures until
  // the feature is re-enabled.
}

bool IpProtectionCoreImpl::HasTrackingProtectionException(
    const GURL& first_party_url) const {
  for (const content_settings::HostIndexedContentSettings& index :
       tp_content_settings_) {
    if (const content_settings::RuleEntry* result =
            index.Find(GURL(), first_party_url);
        result != nullptr) {
      return content_settings::ValueToContentSetting(result->second.value) ==
             CONTENT_SETTING_ALLOW;
    }
  }
  return false;
}

void IpProtectionCoreImpl::SetTrackingProtectionContentSetting(
    const ContentSettingsForOneType& settings) {
  tp_content_settings_ =
      content_settings::HostIndexedContentSettings::Create(settings);
}

void IpProtectionCoreImpl::RecordTokenDemand(size_t chain_index) {
  auto it = ipp_token_managers_.find(chain_index == 0 ? ProxyLayer::kProxyA
                                                      : ProxyLayer::kProxyB);
  if (it != ipp_token_managers_.end()) {
    it->second->RecordTokenDemand();
  }
}

}  // namespace ip_protection

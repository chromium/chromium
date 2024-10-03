// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_core_impl.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "components/ip_protection/common/ip_protection_token_manager_impl.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"

namespace ip_protection {

namespace {
// Rewrite the proxy list to use SCHEME_QUIC. In order to fall back to HTTPS
// quickly if QUIC is broken, the first chain is included once with
// SCHEME_QUIC and once with SCHEME_HTTPS. The remaining chains are included
// only with SCHEME_QUIC.
std::vector<net::ProxyChain> MakeQuicProxyList(
    const std::vector<net::ProxyChain>& proxy_list,
    bool include_https_fallback = true) {
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
    return net::ProxyChain::ForIpProtection(
        std::move(quic_servers), proxy_chain.ip_protection_chain_id());
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
    std::unique_ptr<IpProtectionConfigGetter> config_getter,
    bool is_ip_protection_enabled)
    : is_ip_protection_enabled_(is_ip_protection_enabled),
      ipp_over_quic_(net::features::kIpPrivacyUseQuicProxies.Get()),
      enable_token_caching_by_geo_(
          net::features::kIpPrivacyCacheTokensByGeo.Get()) {
  ipp_proxy_config_manager_ = nullptr;

  // This type may be constructed with the `config_getter` being a `nullptr`,
  // for testing/experimental purposes. In that case, the list manager and cache
  // managers should not be created.
  if (config_getter.get() != nullptr && config_getter->IsAvailable()) {
    config_getter_ = std::move(config_getter);

    ipp_proxy_config_manager_ =
        std::make_unique<IpProtectionProxyConfigManagerImpl>(this,
                                                             *config_getter_);

    ipp_token_managers_[ProxyLayer::kProxyA] =
        std::make_unique<IpProtectionTokenManagerImpl>(
            this, config_getter_.get(), ProxyLayer::kProxyA);

    ipp_token_managers_[ProxyLayer::kProxyB] =
        std::make_unique<IpProtectionTokenManagerImpl>(
            this, config_getter_.get(), ProxyLayer::kProxyB);
  }

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

IpProtectionCoreImpl::~IpProtectionCoreImpl() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
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
      Telemetry().EmptyTokenCache(manager.first);
      all_caches_have_tokens = false;
    }
  }

  return all_caches_have_tokens;
}

std::optional<BlindSignedAuthToken> IpProtectionCoreImpl::GetAuthToken(
    size_t chain_index) {
  std::optional<BlindSignedAuthToken> result;

  // If the proxy list is empty, there cannot be any matching tokens.
  if (!IsProxyListAvailable() || ipp_token_managers_.empty()) {
    return result;
  }

  auto proxy_layer =
      chain_index == 0 ? ProxyLayer::kProxyA : ProxyLayer::kProxyB;
  if (ipp_token_managers_.count(proxy_layer) > 0) {
    result = ipp_token_managers_[proxy_layer]->GetAuthToken(
        ipp_proxy_config_manager_->CurrentGeo());
  }
  return result;
}

void IpProtectionCoreImpl::
    InvalidateIpProtectionConfigCacheTryAgainAfterTime() {
  for (const auto& manager : ipp_token_managers_) {
    manager.second->InvalidateTryAgainAfterTime();
  }
  // If OAuth tokens are applied to GetProxyConfig requests (i.e. when
  // `kIpPrivacyIncludeOAuthTokenInGetProxyConfig` is enabled), refresh the
  // proxy list to try to obtain a new OAuth token.
  if (net::features::kIpPrivacyIncludeOAuthTokenInGetProxyConfig.Get()) {
    RequestRefreshProxyList();
  }
}

void IpProtectionCoreImpl::SetIpProtectionTokenManagerForTesting(
    ProxyLayer proxy_layer,
    std::unique_ptr<IpProtectionTokenManager> ipp_token_manager) {
  ipp_token_managers_[proxy_layer] = std::move(ipp_token_manager);
}

IpProtectionTokenManager*
IpProtectionCoreImpl::GetIpProtectionTokenManagerForTesting(
    ProxyLayer proxy_layer) {
  return ipp_token_managers_[proxy_layer].get();
}

void IpProtectionCoreImpl::SetIpProtectionProxyConfigManagerForTesting(
    std::unique_ptr<IpProtectionProxyConfigManager> ipp_proxy_config_manager) {
  ipp_proxy_config_manager_ = std::move(ipp_proxy_config_manager);
}

IpProtectionProxyConfigManager*
IpProtectionCoreImpl::GetIpProtectionProxyConfigManagerForTesting() {
  return ipp_proxy_config_manager_.get();
}

bool IpProtectionCoreImpl::IsProxyListAvailable() {
  return ipp_proxy_config_manager_ != nullptr
             ? ipp_proxy_config_manager_->IsProxyListAvailable()
             : false;
}

void IpProtectionCoreImpl::QuicProxiesFailed() {
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
  // If token caching by geo is disabled, short-circuit and don't do anything.
  if (!enable_token_caching_by_geo_) {
    return;
  }

  if (ipp_proxy_config_manager_ != nullptr &&
      ipp_proxy_config_manager_->CurrentGeo() != geo_id) {
    ipp_proxy_config_manager_->RefreshProxyListForGeoChange();
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
    RequestRefreshProxyList();
  }
}

void IpProtectionCoreImpl::VerifyIpProtectionConfigGetterForTesting(
    VerifyIpProtectionConfigGetterForTestingCallback callback) {
  auto* ipp_token_manager_impl = static_cast<IpProtectionTokenManagerImpl*>(
      GetIpProtectionTokenManagerForTesting(  // IN-TEST
          ProxyLayer::kProxyA));
  CHECK(ipp_token_manager_impl);

  // If active cache management is enabled (the default), disable it and do a
  // one-time reset of the state. Since the browser process will be driving this
  // test, this makes it easier to reason about our state (for instance, if the
  // browser process sends less than the requested number of tokens, the network
  // service won't immediately request more).
  if (ipp_token_manager_impl->IsCacheManagementEnabledForTesting()) {
    ipp_token_manager_impl->DisableCacheManagementForTesting(  // IN-TEST
        base::BindOnce(
            [](base::WeakPtr<IpProtectionCoreImpl> ipp_core,
               VerifyIpProtectionConfigGetterForTestingCallback callback) {
              DCHECK(ipp_core);
              // Drain auth tokens.
              ipp_core->InvalidateIpProtectionConfigCacheTryAgainAfterTime();
              while (ipp_core->AreAuthTokensAvailable()) {
                ipp_core->GetAuthToken(0);  // kProxyA.
              }
              // Call `PostTask()` instead of invoking the Verify method again
              // directly so that if `DisableCacheManagementForTesting()` needed
              // to wait for a `TryGetAuthTokens()` call to finish, then we
              // ensure that the stored callback has been cleared before the
              // Verify method tries to call `TryGetAuthTokens()` again.
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&IpProtectionCoreImpl::
                                     VerifyIpProtectionConfigGetterForTesting,
                                 ipp_core, std::move(callback)));
            },
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // If there is a cooldown in effect, then don't send any tokens and instead
  // send back the try again after time.
  base::Time try_auth_tokens_after =
      ipp_token_manager_impl
          ->try_get_auth_tokens_after_for_testing();  // IN-TEST
  if (!try_auth_tokens_after.is_null()) {
    std::move(callback).Run(std::nullopt, try_auth_tokens_after);
    return;
  }

  ipp_token_manager_impl->SetOnTryGetAuthTokensCompletedForTesting(  // IN-TEST
      base::BindOnce(
          &IpProtectionCoreImpl::OnIpProtectionConfigAvailableForTesting,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  ipp_token_manager_impl->CallTryGetAuthTokensForTesting();  // IN-TEST
}

void IpProtectionCoreImpl::SetIpProtectionEnabled(bool enabled) {
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

bool IpProtectionCoreImpl::IsIpProtectionEnabledForTesting() {
  return is_ip_protection_enabled_;
}

void IpProtectionCoreImpl::OnIpProtectionConfigAvailableForTesting(
    VerifyIpProtectionConfigGetterForTestingCallback callback) {
  auto* ipp_token_manager_impl = static_cast<IpProtectionTokenManagerImpl*>(
      GetIpProtectionTokenManagerForTesting(  // IN-TEST
          ProxyLayer::kProxyA));
  auto* ipp_proxy_config_manager_impl =
      static_cast<IpProtectionProxyConfigManagerImpl*>(
          GetIpProtectionProxyConfigManagerForTesting());  // IN-TEST
  CHECK(ipp_proxy_config_manager_impl);
  ipp_proxy_config_manager_impl->SetProxyListForTesting(  // IN-TEST
      std::vector{net::ProxyChain::ForIpProtection(
          std::vector{net::ProxyServer::FromSchemeHostAndPort(
              net::ProxyServer::SCHEME_HTTPS, "proxy-a", std::nullopt)})},
      GetGeoHintFromGeoIdForTesting(  // IN-TEST
          ipp_token_manager_impl->CurrentGeo()));
  std::optional<BlindSignedAuthToken> result = GetAuthToken(0);  // kProxyA.
  if (result.has_value()) {
    std::move(callback).Run(std::move(result.value()), std::nullopt);
    return;
  }
  base::Time try_auth_tokens_after =
      ipp_token_manager_impl
          ->try_get_auth_tokens_after_for_testing();  // IN-TEST
  std::move(callback).Run(std::nullopt, try_auth_tokens_after);
}

}  // namespace ip_protection

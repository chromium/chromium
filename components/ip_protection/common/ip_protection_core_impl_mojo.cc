// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_core_impl_mojo.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_core_host_remote.h"
#include "components/ip_protection/common/ip_protection_core_impl.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/common/ip_protection_proxy_config_mojo_fetcher.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "components/ip_protection/common/ip_protection_token_manager_impl.h"
#include "components/ip_protection/common/ip_protection_token_mojo_fetcher.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"

namespace ip_protection {

namespace {

// Make a map-by-layer of token managers. This is a utility for the constructor.
IpProtectionCoreImpl::ProxyTokenManagerMap MakeTokenManagerMap(
    IpProtectionCore* ip_protection_core,
    scoped_refptr<IpProtectionCoreHostRemote> core_host_remote,
    IpProtectionCoreImpl::InitialTokensMap initial_tokens) {
  IpProtectionCoreImpl::ProxyTokenManagerMap managers;
  for (ProxyLayer proxy_layer : {ProxyLayer::kProxyA, ProxyLayer::kProxyB}) {
    std::vector<BlindSignedAuthToken> initial_tokens_for_layer;
    if (auto it = initial_tokens.find(proxy_layer);
        it != initial_tokens.end()) {
      initial_tokens_for_layer = std::move(it->second);
    }
    managers.insert(
        {proxy_layer,
         std::make_unique<IpProtectionTokenManagerImpl>(
             ip_protection_core, core_host_remote,
             std::make_unique<IpProtectionTokenMojoFetcher>(core_host_remote),
             proxy_layer, std::move(initial_tokens_for_layer))});
  }
  return managers;
}

}  // namespace

IpProtectionCoreImplMojo::IpProtectionCoreImplMojo(
    mojo::PendingReceiver<ip_protection::mojom::CoreControl> pending_receiver,
    scoped_refptr<IpProtectionCoreHostRemote> core_host_remote,
    MaskedDomainListManager* masked_domain_list_manager,
    bool is_ip_protection_enabled,
    bool ip_protection_incognito,
    InitialTokensMap initial_tokens)
    : IpProtectionCoreImpl(
          masked_domain_list_manager,
          core_host_remote
              ? std::make_unique<IpProtectionProxyConfigManagerImpl>(
                    this,
                    std::make_unique<IpProtectionProxyConfigMojoFetcher>(
                        core_host_remote))
              : nullptr,
          core_host_remote ? MakeTokenManagerMap(this,
                                                 core_host_remote,
                                                 std::move(initial_tokens))
                           : IpProtectionCoreImpl::ProxyTokenManagerMap(),
          is_ip_protection_enabled,
          ip_protection_incognito),
      receiver_(this, std::move(pending_receiver)) {}

IpProtectionCoreImplMojo::IpProtectionCoreImplMojo(
    MaskedDomainListManager* masked_domain_list_manager,
    std::unique_ptr<IpProtectionProxyConfigManager>
        ip_protection_proxy_config_manager,
    IpProtectionCoreImpl::ProxyTokenManagerMap ip_protection_token_managers,
    bool is_ip_protection_enabled,
    bool ip_protection_incognito)
    : IpProtectionCoreImpl(masked_domain_list_manager,
                           std::move(ip_protection_proxy_config_manager),
                           std::move(ip_protection_token_managers),
                           is_ip_protection_enabled,
                           ip_protection_incognito),
      receiver_(this) {}

IpProtectionCoreImplMojo::~IpProtectionCoreImplMojo() = default;

// static
IpProtectionCoreImplMojo IpProtectionCoreImplMojo::CreateForTesting(
    MaskedDomainListManager* masked_domain_list_manager,
    std::unique_ptr<IpProtectionProxyConfigManager>
        ip_protection_proxy_config_manager,
    IpProtectionCoreImpl::ProxyTokenManagerMap ip_protection_token_managers,
    bool is_ip_protection_enabled,
    bool ip_protection_incognito) {
  return IpProtectionCoreImplMojo(
      masked_domain_list_manager, std::move(ip_protection_proxy_config_manager),
      std::move(ip_protection_token_managers), is_ip_protection_enabled,
      ip_protection_incognito);
}

void IpProtectionCoreImplMojo::BindTestInterfaceForTesting(
    mojo::PendingReceiver<ip_protection::mojom::CoreControlTest> receiver) {
  test_receivers_for_testing_.Add(this, std::move(receiver));
}

void IpProtectionCoreImplMojo::VerifyIpProtectionCoreHostForTesting(
    ip_protection::mojom::CoreControlTest::
        VerifyIpProtectionCoreHostForTestingCallback callback) {
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
            [](base::WeakPtr<IpProtectionCoreImplMojo> ipp_core,
               VerifyIpProtectionCoreHostForTestingCallback callback) {
              // Call `PostTask()` instead of invoking the Verify method again
              // directly so that if `DisableCacheManagementForTesting()` needed
              // to wait for a `TryGetAuthTokens()` call to finish, then we
              // ensure that the stored callback has been cleared before the
              // Verify method tries to call `TryGetAuthTokens()` again.
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&IpProtectionCoreImplMojo::
                                     VerifyIpProtectionCoreHostForTesting,
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
          &IpProtectionCoreImplMojo::OnIpProtectionConfigAvailableForTesting,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  ipp_token_manager_impl->CallTryGetAuthTokensForTesting();  // IN-TEST
}

void IpProtectionCoreImplMojo::AuthTokensMayBeAvailable() {
  for (const auto& manager : ip_protection_token_managers()) {
    manager.second->InvalidateTryAgainAfterTime();
  }

  // If OAuth tokens are applied to GetProxyConfig requests (i.e. when
  // `kIpPrivacyIncludeOAuthTokenInGetProxyConfig` is enabled), refresh the
  // proxy list to try to obtain a new OAuth token.
  if (net::features::kIpPrivacyIncludeOAuthTokenInGetProxyConfig.Get()) {
    RequestRefreshProxyList();
  }
}

void IpProtectionCoreImplMojo::SetIpProtectionEnabled(bool enabled) {
  set_ip_protection_enabled(enabled);
}

void IpProtectionCoreImplMojo::IsIpProtectionEnabledForTesting(
    ip_protection::mojom::CoreControlTest::
        IsIpProtectionEnabledForTestingCallback callback) {
  std::move(callback).Run(is_ip_protection_enabled());
}

void IpProtectionCoreImplMojo::GetAuthTokenForTesting(
    ProxyLayer proxy_layer,
    const std::string& geo_id,
    ip_protection::mojom::CoreControlTest::GetAuthTokenForTestingCallback
        callback) {
  std::move(callback).Run(
      IpProtectionCoreImpl::GetAuthTokenForTesting(proxy_layer, geo_id));
}

void IpProtectionCoreImplMojo::HasTrackingProtectionExceptionForTesting(
    const GURL& first_party_url,
    ip_protection::mojom::CoreControlTest::
        HasTrackingProtectionExceptionForTestingCallback callback) {
  std::move(callback).Run(HasTrackingProtectionException(first_party_url));
}

void IpProtectionCoreImplMojo::OnIpProtectionConfigAvailableForTesting(
    VerifyIpProtectionCoreHostForTestingCallback callback) {
  auto* ipp_token_manager_impl = static_cast<IpProtectionTokenManagerImpl*>(
      GetIpProtectionTokenManagerForTesting(  // IN-TEST
          ProxyLayer::kProxyA));
  auto* ip_protection_proxy_config_manager_impl =
      static_cast<IpProtectionProxyConfigManagerImpl*>(
          GetIpProtectionProxyConfigManagerForTesting());  // IN-TEST
  CHECK(ip_protection_proxy_config_manager_impl);
  ip_protection_proxy_config_manager_impl->SetProxyListForTesting(  // IN-TEST
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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_config_getter_mojo_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "components/ip_protection/common/ip_protection_config_cache_impl.h"
#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ip_protection {

namespace {

// TODO(abhijithnair): Replace the below with EnumTraits.
network::mojom::IpProtectionProxyLayer convertToMojo(
    const ip_protection::ProxyLayer& layer) {
  switch (layer) {
    case ip_protection::ProxyLayer::kProxyA:
      return network::mojom::IpProtectionProxyLayer::kProxyA;
    case ip_protection::ProxyLayer::kProxyB:
      return network::mojom::IpProtectionProxyLayer::kProxyB;
  }
}
}  // namespace

IpProtectionConfigGetterMojoImpl::IpProtectionConfigGetterMojoImpl(
    mojo::PendingRemote<network::mojom::IpProtectionConfigGetter>
        config_getter) {
  if (config_getter.is_valid()) {
    config_getter_.Bind(std::move(config_getter));
    is_available_ = true;
  }
}

IpProtectionConfigGetterMojoImpl::~IpProtectionConfigGetterMojoImpl() = default;

bool IpProtectionConfigGetterMojoImpl::IsAvailable() {
  return is_available_;
}

void IpProtectionConfigGetterMojoImpl::TryGetAuthTokens(
    uint32_t batch_size,
    ip_protection::ProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  config_getter_->TryGetAuthTokens(
      batch_size, convertToMojo(proxy_layer),
      base::BindOnce(&IpProtectionConfigGetterMojoImpl::OnGotAuthTokens,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IpProtectionConfigGetterMojoImpl::GetProxyList(
    GetProxyListCallback callback) {
  config_getter_->GetProxyList(
      base::BindOnce(&IpProtectionConfigGetterMojoImpl::OnGotProxyList,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IpProtectionConfigGetterMojoImpl::OnGotProxyList(
    GetProxyListCallback callback,
    const std::optional<std::vector<net::ProxyChain>>& proxy_list,
    const std::optional<ip_protection::GeoHint>& geo_hint) {
  std::move(callback).Run(proxy_list, geo_hint);
}

void IpProtectionConfigGetterMojoImpl::OnGotAuthTokens(
    TryGetAuthTokensCallback callback,
    const std::optional<std::vector<ip_protection::BlindSignedAuthToken>>&
        tokens,
    std::optional<::base::Time> expiration_time) {
  std::move(callback).Run(tokens, expiration_time);
}

}  // namespace ip_protection

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_config_getter_mojo_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_core_impl.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "components/ip_protection/mojom/data_types.mojom.h"

namespace ip_protection {

namespace {

// TODO(abhijithnair): Replace the below with EnumTraits.
ip_protection::mojom::ProxyLayer convertToMojo(const ProxyLayer& layer) {
  switch (layer) {
    case ProxyLayer::kProxyA:
      return ip_protection::mojom::ProxyLayer::kProxyA;
    case ProxyLayer::kProxyB:
      return ip_protection::mojom::ProxyLayer::kProxyB;
  }
}
}  // namespace

IpProtectionConfigGetterMojoImpl::IpProtectionConfigGetterMojoImpl(
    mojo::PendingRemote<ip_protection::mojom::CoreHost> config_getter) {
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
    ProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  config_getter_->TryGetAuthTokens(
      batch_size, convertToMojo(proxy_layer),
      base::BindOnce(&IpProtectionConfigGetterMojoImpl::OnGotAuthTokens,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IpProtectionConfigGetterMojoImpl::GetProxyConfig(
    GetProxyConfigCallback callback) {
  config_getter_->GetProxyConfig(
      base::BindOnce(&IpProtectionConfigGetterMojoImpl::OnGotProxyList,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IpProtectionConfigGetterMojoImpl::OnGotProxyList(
    GetProxyConfigCallback callback,
    const std::optional<std::vector<net::ProxyChain>>& proxy_list,
    const std::optional<GeoHint>& geo_hint) {
  std::move(callback).Run(proxy_list, geo_hint);
}

void IpProtectionConfigGetterMojoImpl::OnGotAuthTokens(
    TryGetAuthTokensCallback callback,
    const std::optional<std::vector<BlindSignedAuthToken>>& tokens,
    std::optional<::base::Time> expiration_time) {
  std::move(callback).Run(tokens, expiration_time);
}

}  // namespace ip_protection

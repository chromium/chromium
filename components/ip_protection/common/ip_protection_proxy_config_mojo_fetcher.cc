// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_mojo_fetcher.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/ip_protection/common/ip_protection_core_host_remote.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/proxy_chain.h"

namespace ip_protection {

IpProtectionProxyConfigMojoFetcher::IpProtectionProxyConfigMojoFetcher(
    scoped_refptr<IpProtectionCoreHostRemote> core_host)
    : core_host_remote_(std::move(core_host)) {}

IpProtectionProxyConfigMojoFetcher::~IpProtectionProxyConfigMojoFetcher() =
    default;

void IpProtectionProxyConfigMojoFetcher::GetProxyConfig(
    GetProxyConfigCallback callback) {
  core_host_remote_->core_host()->GetProxyConfig(base::BindOnce(
      [](GetProxyConfigCallback callback,
         const std::optional<std::vector<net::ProxyChain>>& proxy_chains,
         const std::optional<ip_protection::GeoHint>& geo_hint) {
        std::move(callback).Run(proxy_chains, geo_hint);
      },
      std::move(callback)));
}

}  // namespace ip_protection

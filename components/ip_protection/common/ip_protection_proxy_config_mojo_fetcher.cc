// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_mojo_fetcher.h"

#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ip_protection {

IpProtectionProxyConfigMojoFetcher::IpProtectionProxyConfigMojoFetcher(
    scoped_refptr<IpProtectionConfigGetter> config_getter)
    : config_getter_(std::move(config_getter)) {}

IpProtectionProxyConfigMojoFetcher::~IpProtectionProxyConfigMojoFetcher() =
    default;

void IpProtectionProxyConfigMojoFetcher::GetProxyConfig(
    GetProxyConfigCallback callback) {
  config_getter_->GetProxyConfig(std::move(callback));
}

}  // namespace ip_protection

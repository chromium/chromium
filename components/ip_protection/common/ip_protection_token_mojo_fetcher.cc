// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_mojo_fetcher.h"

#include <string>
#include <vector>

#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ip_protection {

IpProtectionTokenMojoFetcher::IpProtectionTokenMojoFetcher(
    scoped_refptr<IpProtectionConfigGetter> config_getter)
    : config_getter_(std::move(config_getter)) {}

IpProtectionTokenMojoFetcher::~IpProtectionTokenMojoFetcher() = default;

void IpProtectionTokenMojoFetcher::TryGetAuthTokens(
    uint32_t batch_size,
    ProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  config_getter_->TryGetAuthTokens(batch_size, proxy_layer,
                                   std::move(callback));
}

}  // namespace ip_protection

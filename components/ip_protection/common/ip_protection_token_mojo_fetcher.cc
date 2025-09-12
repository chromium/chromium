// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_mojo_fetcher.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_core_host_remote.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ip_protection {

IpProtectionTokenMojoFetcher::IpProtectionTokenMojoFetcher(
    scoped_refptr<IpProtectionCoreHostRemote> core_host_remote)
    : core_host_remote_(std::move(core_host_remote)) {}

IpProtectionTokenMojoFetcher::~IpProtectionTokenMojoFetcher() = default;

void IpProtectionTokenMojoFetcher::TryGetAuthTokens(
    uint32_t batch_size,
    ProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  core_host_remote_->core_host()->TryGetAuthTokens(batch_size, proxy_layer,
                                                   std::move(callback));
}

}  // namespace ip_protection

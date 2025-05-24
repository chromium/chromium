// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_mojo_fetcher.h"

#include "components/ip_protection/common/ip_protection_core_host_remote.h"

namespace ip_protection {

IpProtectionProbabilisticRevealTokenMojoFetcher::
    IpProtectionProbabilisticRevealTokenMojoFetcher(
        scoped_refptr<IpProtectionCoreHostRemote> core_host)
    : core_host_remote_(core_host) {}

IpProtectionProbabilisticRevealTokenMojoFetcher::
    ~IpProtectionProbabilisticRevealTokenMojoFetcher() = default;

void IpProtectionProbabilisticRevealTokenMojoFetcher::
    TryGetProbabilisticRevealTokens(
        TryGetProbabilisticRevealTokensCallback callback) {
  core_host_remote_->core_host()->TryGetProbabilisticRevealTokens(
      base::BindOnce(
          [](TryGetProbabilisticRevealTokensCallback callback,
             const std::optional<TryGetProbabilisticRevealTokensOutcome>&
                 outcome,
             const TryGetProbabilisticRevealTokensResult& result) {
            std::move(callback).Run(outcome, result);
          },
          std::move(callback)));
}

}  // namespace ip_protection

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_MOJO_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_MOJO_FETCHER_H_

#include "base/memory/scoped_refptr.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"

namespace ip_protection {

class IpProtectionCoreHostRemote;

// Mojo implementation of PRT fetcher abstract base class. This is a simple
// wrapper around `IpProtectionCoreHostRemote`.
class IpProtectionProbabilisticRevealTokenMojoFetcher
    : public IpProtectionProbabilisticRevealTokenFetcher {
 public:
  explicit IpProtectionProbabilisticRevealTokenMojoFetcher(
      scoped_refptr<IpProtectionCoreHostRemote> core_host);
  ~IpProtectionProbabilisticRevealTokenMojoFetcher() override;

  // IpProtectionProbabilisticRevealTokenFetcher implementation.
  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override;

 private:
  scoped_refptr<IpProtectionCoreHostRemote> core_host_remote_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_MOJO_FETCHER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_FETCHER_H_

#include "base/functional/callback.h"
#include "components/ip_protection/common/ip_protection_data_types.h"

namespace ip_protection {

// IpProtectionProbabilisticRevealTokenFetcher is an abstract base class for
// probabilistic reveal token fetchers.
class IpProtectionProbabilisticRevealTokenFetcher {
 public:
  using TryGetProbabilisticRevealTokensCallback = base::OnceCallback<void(
      std::optional<TryGetProbabilisticRevealTokensOutcome>,
      TryGetProbabilisticRevealTokensResult)>;

  virtual ~IpProtectionProbabilisticRevealTokenFetcher() = default;
  IpProtectionProbabilisticRevealTokenFetcher(
      const IpProtectionProbabilisticRevealTokenFetcher&) = delete;
  IpProtectionProbabilisticRevealTokenFetcher& operator=(
      const IpProtectionProbabilisticRevealTokenFetcher&) = delete;

  // Get probabilistic reveal tokens. On success, the response callback contains
  // a vector of tokens, public key, expiration and next start timestamps and
  // the number of tokens with the signal. On failure all callback values are
  // null and error is stored in function return value.
  virtual void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) = 0;

 protected:
  IpProtectionProbabilisticRevealTokenFetcher() = default;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_FETCHER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace quiche {
enum class ProxyLayer;
struct BlindSignToken;
}  // namespace quiche

namespace ip_protection {

// Interface that manages requesting and fetching blind-signed authentication
// tokens for IP Protection.
class IpProtectionTokenFetcher {
 public:
  using TryGetAuthTokensCallback =
      base::OnceCallback<void(std::optional<std::vector<BlindSignedAuthToken>>,
                              std::optional<::base::Time>)>;

  virtual ~IpProtectionTokenFetcher() = default;
  IpProtectionTokenFetcher(const IpProtectionTokenFetcher&) = delete;
  IpProtectionTokenFetcher& operator=(const IpProtectionTokenFetcher&) = delete;

  // Try to get a batch of auth tokens. The response callback contains either
  // a vector of tokens or, on error, a time before which the method should not
  // be called again.
  virtual void TryGetAuthTokens(uint32_t batch_size,
                                ProxyLayer proxy_layer,
                                TryGetAuthTokensCallback callback) = 0;

 protected:
  IpProtectionTokenFetcher() = default;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace quiche {
class BlindSignAuthInterface;
enum class ProxyLayer;
enum class BlindSignAuthServiceType;
struct BlindSignToken;
}  // namespace quiche

namespace ip_protection {

using FetchBlindSignedTokenCallback = base::OnceCallback<void(
    absl::StatusOr<std::vector<quiche::BlindSignToken>>)>;

// Interface that manages requesting and fetching blind-signed authentication
// tokens for IP Protection using the `quiche::BlindSignAuth` library.
class IpProtectionTokenFetcher {
 public:
  virtual ~IpProtectionTokenFetcher() = default;
  IpProtectionTokenFetcher(const IpProtectionTokenFetcher&) = delete;
  IpProtectionTokenFetcher& operator=(const IpProtectionTokenFetcher&) = delete;

  // `FetchBlindSignedToken()` calls into the `quiche::BlindSignAuth` library to
  // request a blind-signed auth token for use at the IP Protection proxies.
  virtual void FetchBlindSignedToken(
      std::optional<std::string> access_token,
      uint32_t batch_size,
      quiche::ProxyLayer proxy_layer,
      FetchBlindSignedTokenCallback callback) = 0;

  // Calls `GetTokens()` on provided `blind_sign_auth` to fetch blind signed
  // tokens.
  static void GetTokensFromBlindSignAuth(
      quiche::BlindSignAuthInterface* blind_sign_auth,
      quiche::BlindSignAuthServiceType service_type,
      std::optional<std::string> access_token,
      uint32_t batch_size,
      quiche::ProxyLayer proxy_layer,
      FetchBlindSignedTokenCallback callback);

 protected:
  IpProtectionTokenFetcher() = default;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_

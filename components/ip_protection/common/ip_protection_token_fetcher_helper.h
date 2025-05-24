// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_HELPER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_HELPER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace quiche {
class BlindSignAuthInterface;
enum class ProxyLayer;
enum class BlindSignAuthServiceType;
struct BlindSignToken;
}  // namespace quiche

namespace ip_protection {

// Helper for invoking BSA to generate tokens. This class is typically wrapped
// in `base::SequenceBound` in order to run off the main thread.
class IpProtectionTokenFetcherHelper {
 public:
  using FetchBlindSignedTokenCallback = base::OnceCallback<void(
      absl::StatusOr<std::vector<quiche::BlindSignToken>>)>;

  IpProtectionTokenFetcherHelper();
  ~IpProtectionTokenFetcherHelper() = default;
  IpProtectionTokenFetcherHelper(const IpProtectionTokenFetcherHelper&) =
      delete;
  IpProtectionTokenFetcherHelper& operator=(
      const IpProtectionTokenFetcherHelper&) = delete;

  // Calls `GetTokens()` on provided `blind_sign_auth` to fetch blind signed
  // tokens.
  void GetTokensFromBlindSignAuth(
      quiche::BlindSignAuthInterface* blind_sign_auth,
      quiche::BlindSignAuthServiceType service_type,
      std::optional<std::string> access_token,
      uint32_t batch_size,
      quiche::ProxyLayer proxy_layer,
      FetchBlindSignedTokenCallback callback);

  // Converts a batch of `quiche::BlindSignToken` into
  // `BlindSignedAuthToken`, returning nullopt on failure.
  static std::optional<std::vector<ip_protection::BlindSignedAuthToken>>
  QuicheTokensToIpProtectionAuthTokens(std::vector<quiche::BlindSignToken>&);

  // Creates a blind-signed auth token by converting token fetched using the
  // `quiche::BlindSignAuth` library to a `BlindSignedAuthToken`.
  static std::optional<BlindSignedAuthToken> CreateBlindSignedAuthToken(
      const quiche::BlindSignToken& bsa_token);

  // Creates a `quiche::BlindSignToken()` in the format that the BSA library
  // will return them.
  static quiche::BlindSignToken CreateBlindSignTokenForTesting(
      std::string token_value,
      base::Time expiration,
      const GeoHint& geo_hint);

  // Converts a mock token value and expiration time into the struct that will
  // be passed to the network service.
  static std::optional<BlindSignedAuthToken>
  CreateMockBlindSignedAuthTokenForTesting(std::string token_value,
                                           base::Time expiration,
                                           const GeoHint& geo_hint);

  // Service types used for GetProxyConfigRequest.
  static constexpr char kChromeIpBlinding[] = "chromeipblinding";
  static constexpr char kWebViewIpBlinding[] = "webviewipblinding";

 protected:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_HELPER_H_

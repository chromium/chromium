// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_HELPER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_HELPER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
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

 protected:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_HELPER_H_

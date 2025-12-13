// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PHOSPHOR_TOKEN_FETCHER_HELPER_H_
#define COMPONENTS_LEGION_PHOSPHOR_TOKEN_FETCHER_HELPER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/legion/phosphor/data_types.h"
#include "components/legion/proto/legion.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace quiche {
class BlindSignAuthInterface;
enum class ProxyLayer;
enum class BlindSignAuthServiceType;
struct BlindSignToken;
}  // namespace quiche

namespace legion::phosphor {

// Helper for invoking BSA to generate tokens. This class is typically wrapped
// in `base::SequenceBound` in order to run off the main thread.
class TokenFetcherHelper {
 public:
  using FetchBlindSignedTokenCallback = base::OnceCallback<void(
      base::expected<std::vector<quiche::BlindSignToken>, absl::Status>)>;

  TokenFetcherHelper();
  ~TokenFetcherHelper() = default;
  TokenFetcherHelper(const TokenFetcherHelper&) = delete;
  TokenFetcherHelper& operator=(const TokenFetcherHelper&) = delete;

  // Calls `GetTokens()` on provided `blind_sign_auth` to fetch blind signed
  // tokens.
  void GetTokensFromBlindSignAuth(
      quiche::BlindSignAuthInterface* blind_sign_auth,
      quiche::BlindSignAuthServiceType service_type,
      std::optional<std::string> access_token,
      int batch_size,
      quiche::ProxyLayer proxy_layer,
      FetchBlindSignedTokenCallback callback);

  // Converts a batch of `quiche::BlindSignToken` into
  // `BlindSignedAuthToken`, returning nullopt on failure.
  static std::optional<std::vector<legion::phosphor::BlindSignedAuthToken>>
  QuicheTokensToPhosphorAuthTokens(std::vector<quiche::BlindSignToken>&);

  // Creates a blind-signed auth token by converting token fetched using the
  // `quiche::BlindSignAuth` library to a `BlindSignedAuthToken`.
  static std::optional<BlindSignedAuthToken> CreateBlindSignedAuthToken(
      const quiche::BlindSignToken& bsa_token);

  // Service type used for GetInitialData and AuthAndSign requests.
  // TODO(b:457425177): Make sure this is the correct service type to use for
  // GetInitialData and AuthAndSign requests.
  static constexpr char kPrivateAratea[] = "privateAratea";

 protected:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace legion::phosphor

#endif  // COMPONENTS_LEGION_PHOSPHOR_TOKEN_FETCHER_HELPER_H_

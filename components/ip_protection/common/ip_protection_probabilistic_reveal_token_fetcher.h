// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_FETCHER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"

namespace ip_protection {

// Declares possible return status for TryGetProbabilisticRevealTokens().
enum class TryGetProbabilisticRevealTokensStatus {
  kSuccess = 0,
  kNetNotOk = 1,
  kNetOkNullResponse = 2,
  kNullResponse = 3,
  kResponseParsingFailed = 4,
  kInvalidTokenVersion = 5,
  kInvalidTokenSize = 6,
  kTooFewTokens = 7,
  kTooManyTokens = 8,
  kExpirationTooSoon = 9,
  kExpirationTooLate = 10,
  kInvalidPublicKey = 11,
  kInvalidNumTokensWithSignal = 12,
  kRequestBackedOff = 13,
  kMaxValue = kRequestBackedOff,
};

// Stores return status of TryGetProbabilisticRevealTokens() together with
// NetError() returned by url loader.
struct TryGetProbabilisticRevealTokensResult {
  // Stores return status of TryGetProbabilisticRevealTokens().
  TryGetProbabilisticRevealTokensStatus status;
  // Stores url_loader->NetError() after calling url_loader->DownloadToString()
  // in Retriever::RetrieveProbabilisticRevealTokens(). `network_error_code` is
  // not net::OK if `result` is kNetNotOk. `network_error_code` is net::OK for
  // all other `result` values. `network_error_code` is net::OK if
  // url_loader->DownloadToString() is not called yet, for example when
  // `status` is kProxyDisabled and TryGetProbabilisticRevealTokens returned
  // before making a network call.
  int network_error_code;
  // Stores the time when the next TryGetProbabilisticRevealTokens() call should
  // be made. `try_again_after` is set on network errors (i.e. when `status` is
  // kNetNotOk or kNetOkNullResponse), nullopt otherwise.
  std::optional<base::Time> try_again_after;
};

// Stores parsed TryGetProbabilisticRevealTokensResponse for successfully parsed
// responses.
struct TryGetProbabilisticRevealTokensOutcome {
  TryGetProbabilisticRevealTokensOutcome();
  TryGetProbabilisticRevealTokensOutcome(
      const TryGetProbabilisticRevealTokensOutcome&);
  TryGetProbabilisticRevealTokensOutcome(
      TryGetProbabilisticRevealTokensOutcome&&);
  TryGetProbabilisticRevealTokensOutcome& operator=(
      const TryGetProbabilisticRevealTokensOutcome&);
  TryGetProbabilisticRevealTokensOutcome& operator=(
      TryGetProbabilisticRevealTokensOutcome&&);
  ~TryGetProbabilisticRevealTokensOutcome();

  std::vector<ProbabilisticRevealToken> tokens;
  std::string public_key;
  std::uint64_t expiration_time_seconds;
  std::uint64_t next_epoch_start_time_seconds;
  std::int32_t num_tokens_with_signal;
};

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

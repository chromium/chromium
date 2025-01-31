// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_ISSUER_TOKEN_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_ISSUER_TOKEN_FETCHER_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/ip_protection/common/ip_protection_data_types.h"

namespace ip_protection {

// Declares possible return status for TryGetIssuerTokens().
enum class TryGetIssuerTokensStatus {
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

// Stores return status of TryGetIssuerTokens() together with
// NetError() returned by url loader.
struct TryGetIssuerTokensResult {
  // Stores return status of TryGetIssuerTokens().
  TryGetIssuerTokensStatus status;
  // Stores url_loader->NetError() after calling url_loader->DownloadToString()
  // in Retriever::RetrieveIssuerToken(). `network_error_code` is not net::OK
  // if `result` is kNetNotOk. `network_error_code` is net::OK for all other
  // `result` values. `network_error_code` is net::OK if
  // url_loader->DownloadToString() is not called yet, for example when
  // `status` is kProxyDisabled and TryGetIssuerTokens returned before making
  // a network call.
  int network_error_code;
  // Stores the time when the next TryGetIssuerTokens() call should be made.
  // `try_again_after` is set on network errors (i.e. when `status` is kNetNotOk
  // or kNetOkNullResponse), nullopt otherwise.
  std::optional<base::Time> try_again_after;
};

// Stores parsed TryGetIssuerTokensResponse for successfully parsed
// responses.
struct TryGetIssuerTokensOutcome {
  TryGetIssuerTokensOutcome();
  TryGetIssuerTokensOutcome(const TryGetIssuerTokensOutcome&);
  TryGetIssuerTokensOutcome(TryGetIssuerTokensOutcome&&);
  TryGetIssuerTokensOutcome& operator=(const TryGetIssuerTokensOutcome&);
  TryGetIssuerTokensOutcome& operator=(TryGetIssuerTokensOutcome&&);
  ~TryGetIssuerTokensOutcome();

  std::vector<IssuerToken> tokens;
  std::string public_key;
  std::uint64_t expiration_time_seconds;
  std::uint64_t next_epoch_start_time_seconds;
  std::int32_t num_tokens_with_signal;
};

// IpProtectionIssuerTokenFetcher is an abstract base class for issuer
// token fetchers.
class IpProtectionIssuerTokenFetcher {
 public:
  using TryGetIssuerTokensCallback =
      base::OnceCallback<void(std::optional<TryGetIssuerTokensOutcome>,
                              TryGetIssuerTokensResult)>;

  virtual ~IpProtectionIssuerTokenFetcher() = default;
  IpProtectionIssuerTokenFetcher(const IpProtectionIssuerTokenFetcher&) =
      delete;
  IpProtectionIssuerTokenFetcher& operator=(
      const IpProtectionIssuerTokenFetcher&) = delete;

  // Get issuer tokens. On success, the response callback contains
  // a vector of tokens, public key, expiration and next start timestamps and
  // the number of tokens with the signal. On failure all callback values are
  // null and error is stored in function return value.
  virtual void TryGetIssuerTokens(TryGetIssuerTokensCallback callback) = 0;

 protected:
  IpProtectionIssuerTokenFetcher() = default;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_ISSUER_TOKEN_FETCHER_H_

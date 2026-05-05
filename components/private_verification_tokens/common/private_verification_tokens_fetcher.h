// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_FETCHER_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace private_verification_tokens {

// Declares possible error states for TryGetTokens().
enum class TryGetTokensError {
  // Indicates url_loader->NetError() is not net::OK post
  // url_loader->DownloadToString() call. All other statuses mean
  // url_loader->NetError() is net::OK.
  kNetNotOk = 1,
  kNullResponse = 2,
  kMaxValue = kNullResponse,
};

// Stores error statuses of TryGetTokens() together with NetError() returned by
// URL loader.
//
// Following are all possible states post TryGetTokens() call.
// - url_loader->NetError() is not net::OK, result will be
//   TryGetTokensResult(kNetNotOk, url_loader->NetError()).
// - url_loader->NetError() is net::OK,
//   - response_body.has_value() is false, result will be
//     TryGetTokensResult(kNullResponse, net::OK).
//   - response_body.has_value() is true, all good, no error.
struct TryGetTokensResult {
  TryGetTokensError error;
  // Stores url_loader->NetError() after calling url_loader->DownloadToString()
  // in PrivateVerificationTokensFetcher::TryGetTokens(). `network_error_code`
  // is not net::OK if `error` is kNetNotOk. `network_error_code` is net::OK
  // for all other `error` values.
  int network_error_code;
};

// Implements fetching a token for a given issuer URL. The fetcher does not
// parse the returned response, only fetches the response. Fetcher will be
// created and called from the PVT keyed service in browser process.
class PrivateVerificationTokensFetcher {
 public:
  using TryGetTokensCallback =
      base::OnceCallback<void(base::expected<std::string, TryGetTokensResult>)>;
  static std::unique_ptr<PrivateVerificationTokensFetcher> Create(
      GURL issue_url,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);
  ~PrivateVerificationTokensFetcher();
  void TryGetTokens(const std::string& request_body,
                    TryGetTokensCallback callback);

 private:
  PrivateVerificationTokensFetcher(
      GURL issue_url,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);
  void OnGetTokensCompleted(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      TryGetTokensCallback callback,
      std::optional<std::string> response);

  const network::ResourceRequest request_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PrivateVerificationTokensFetcher> weak_ptr_factory_{
      this};
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_FETCHER_H_

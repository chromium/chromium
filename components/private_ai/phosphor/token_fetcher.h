// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_FETCHER_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_FETCHER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/phosphor/data_types.h"

namespace quiche {
enum class ProxyLayer;
}  // namespace quiche

namespace private_ai::phosphor {

// Interface that manages requesting and fetching blind-signed authentication
// tokens for PrivateAI. The primary user of this interface will be a token
// manager responsible for caching tokens and providing them to the PrivateAI
// client.
class TokenFetcher {
 public:
  // The callback for the `GetAuthnTokens` method.
  //
  // On success, this callback receives a vector of `BlindSignedAuthToken`
  // structs. Each token is single-use and has its own TTL, indicated by the
  // `expiration` field.
  //
  // On failure, the callback receives a `base::Time`, which indicates the
  // earliest time the client should attempt to call `GetAuthnTokens` again.
  using GetAuthnTokensCallback = base::OnceCallback<void(
      base::expected<std::vector<BlindSignedAuthToken>, base::Time>)>;

  virtual ~TokenFetcher() = default;
  TokenFetcher(const TokenFetcher&) = delete;
  TokenFetcher& operator=(const TokenFetcher&) = delete;

  // Gets a batch of auth tokens. The response callback contains either a
  // vector of tokens or, on error, a time before which the method should not be
  // called again.
  virtual void GetAuthnTokens(int batch_size,
                              quiche::ProxyLayer proxy_layer,
                              GetAuthnTokensCallback callback) = 0;

 protected:
  TokenFetcher() = default;
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_FETCHER_H_

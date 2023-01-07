// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct CoreAccountId;

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

namespace syncer {

// Allows asynchronous OAuth2 access token fetching from sequences other than
// the UI thread.
class TrustedVaultAccessTokenFetcher {
 public:
  using TokenCallback = base::OnceCallback<void(
      absl::optional<signin::AccessTokenInfo> access_token_info)>;

  TrustedVaultAccessTokenFetcher() = default;
  TrustedVaultAccessTokenFetcher(const TrustedVaultAccessTokenFetcher& other) =
      delete;
  TrustedVaultAccessTokenFetcher& operator=(
      const TrustedVaultAccessTokenFetcher& other) = delete;
  virtual ~TrustedVaultAccessTokenFetcher() = default;

  // Asynchronously fetches vault service access token for |account_id|. May be
  // called from arbitrary sequence that owns |this|. |callback| will be called
  // on the caller sequence.
  virtual void FetchAccessToken(const CoreAccountId& account_id,
                                TokenCallback callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_

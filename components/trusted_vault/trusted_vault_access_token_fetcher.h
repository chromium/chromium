// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/types/expected.h"

struct CoreAccountId;

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

namespace trusted_vault {

// Allows asynchronous OAuth2 access token fetching from sequences other than
// the UI thread.
class TrustedVaultAccessTokenFetcher {
 public:
  enum class FetchingError {
    // Used for all transient GoogleServiceAuthErrors.
    kTransientAuthError,
    // Used for all persistent GoogleServiceAuthError.
    kPersistentAuthError,
    // Used when requested account is not primary or became not primary during
    // the fetching.
    kNotPrimaryAccount,
  };

  using AccessTokenInfoOrError =
      base::expected<signin::AccessTokenInfo, FetchingError>;
  using TokenCallback = base::OnceCallback<void(AccessTokenInfoOrError)>;

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

  // May be called on any sequence.
  virtual std::unique_ptr<TrustedVaultAccessTokenFetcher> Clone() = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_

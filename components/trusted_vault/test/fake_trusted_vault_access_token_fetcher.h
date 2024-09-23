// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_

#include <memory>

#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"

namespace trusted_vault {

class FakeTrustedVaultAccessTokenFetcher
    : public TrustedVaultAccessTokenFetcher {
 public:
  explicit FakeTrustedVaultAccessTokenFetcher(
      const AccessTokenInfoOrError& access_token_info_or_error);
  ~FakeTrustedVaultAccessTokenFetcher() override;

  void FetchAccessToken(const CoreAccountId& account_id,
                        TokenCallback callback) override;

  std::unique_ptr<TrustedVaultAccessTokenFetcher> Clone() override;

 private:
  const AccessTokenInfoOrError access_token_info_or_error_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_H_

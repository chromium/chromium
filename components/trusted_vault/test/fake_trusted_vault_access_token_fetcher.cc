// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_trusted_vault_access_token_fetcher.h"

namespace trusted_vault {

FakeTrustedVaultAccessTokenFetcher::FakeTrustedVaultAccessTokenFetcher(
    const AccessTokenInfoOrError& access_token_info_or_error)
    : access_token_info_or_error_(access_token_info_or_error) {}

FakeTrustedVaultAccessTokenFetcher::~FakeTrustedVaultAccessTokenFetcher() =
    default;

void FakeTrustedVaultAccessTokenFetcher::FetchAccessToken(
    const CoreAccountId& account_id,
    TokenCallback callback) {
  std::move(callback).Run(access_token_info_or_error_);
}

std::unique_ptr<TrustedVaultAccessTokenFetcher>
FakeTrustedVaultAccessTokenFetcher::Clone() {
  return std::make_unique<FakeTrustedVaultAccessTokenFetcher>(
      access_token_info_or_error_);
}

}  // namespace trusted_vault

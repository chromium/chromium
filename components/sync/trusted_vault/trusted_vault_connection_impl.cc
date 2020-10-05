// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"

#include <utility>

#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

TrustedVaultConnectionImpl::TrustedVaultConnectionImpl(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher)
    : pending_url_loader_factory_(std::move(url_loader_factory)),
      access_token_fetcher_(std::move(access_token_fetcher)) {}

TrustedVaultConnectionImpl::~TrustedVaultConnectionImpl() = default;

void TrustedVaultConnectionImpl::RegisterAuthenticationFactor(
    const CoreAccountInfo& account_info,
    const std::vector<uint8_t>& last_trusted_vault_key,
    int last_trusted_vault_key_version,
    const SecureBoxPublicKey& authentication_factor_public_key,
    RegisterAuthenticationFactorCallback callback) {
  NOTIMPLEMENTED();
}

void TrustedVaultConnectionImpl::DownloadKeys(
    const CoreAccountInfo& account_info,
    const std::vector<uint8_t>& last_trusted_vault_key,
    int last_trusted_vault_key_version,
    std::unique_ptr<SecureBoxKeyPair> device_key_pair,
    DownloadKeysCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace syncer

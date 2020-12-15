// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_IMPL_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "url/gurl.h"

namespace network {
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}  // namespace network

namespace syncer {

// This class is created on UI thread and used/destroyed on trusted vault
// backend thread.
class TrustedVaultConnectionImpl : public TrustedVaultConnection {
 public:
  TrustedVaultConnectionImpl(
      const GURL& trusted_vault_service_url,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher);

  TrustedVaultConnectionImpl(const TrustedVaultConnectionImpl& other) = delete;
  TrustedVaultConnectionImpl& operator=(
      const TrustedVaultConnectionImpl& other) = delete;
  ~TrustedVaultConnectionImpl() override;

  std::unique_ptr<Request> RegisterAuthenticationFactor(
      const CoreAccountInfo& account_info,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      const SecureBoxPublicKey& authentication_factor_public_key,
      RegisterAuthenticationFactorCallback callback) override;

  std::unique_ptr<Request> DownloadKeys(
      const CoreAccountInfo& account_info,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      std::unique_ptr<SecureBoxKeyPair> device_key_pair,
      DownloadKeysCallback callback) override;

 private:
  // SharedURLLoaderFactory is created lazily, because it needs to be done on
  // the backend sequence, while this class ctor is called on UI thread.
  scoped_refptr<network::SharedURLLoaderFactory> GetOrCreateURLLoaderFactory();

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  const std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher_;

  // Instantiated upon first need using |pending_url_loader_factory_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  GURL trusted_vault_service_url_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_IMPL_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_IMPL_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_CONNECTION_IMPL_H_

#include <memory>
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
  using JoinSecurityDomainsCallback =
      base::OnceCallback<void(TrustedVaultRegistrationStatus,
                              int /*last_key_version=*/)>;

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
      const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
      int last_trusted_vault_key_version,
      const SecureBoxPublicKey& authentication_factor_public_key,
      AuthenticationFactorType authentication_factor_type,
      absl::optional<int> authentication_factor_type_hint,
      RegisterAuthenticationFactorCallback callback) override;

  std::unique_ptr<Request> RegisterDeviceWithoutKeys(
      const CoreAccountInfo& account_info,
      const SecureBoxPublicKey& device_public_key,
      RegisterDeviceWithoutKeysCallback callback) override;

  std::unique_ptr<Request> DownloadNewKeys(
      const CoreAccountInfo& account_info,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      std::unique_ptr<SecureBoxKeyPair> device_key_pair,
      DownloadNewKeysCallback callback) override;

  std::unique_ptr<Request> DownloadIsRecoverabilityDegraded(
      const CoreAccountInfo& account_info,
      IsRecoverabilityDegradedCallback callback) override;

 private:
  std::unique_ptr<Request> SendJoinSecurityDomainsRequest(
      const CoreAccountInfo& account_info,
      const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
      int last_trusted_vault_key_version,
      const SecureBoxPublicKey& authentication_factor_public_key,
      AuthenticationFactorType authentication_factor_type,
      absl::optional<int> authentication_factor_type_hint,
      JoinSecurityDomainsCallback callback);

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

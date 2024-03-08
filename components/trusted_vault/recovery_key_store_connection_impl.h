// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_IMPL_H_
#define COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

struct CoreAccountInfo;

namespace network {
class SharedURLLoaderFactory;
}

namespace trusted_vault {

class RecoveryKeyStoreConnectionImpl : public RecoveryKeyStoreConnection {
 public:
  RecoveryKeyStoreConnectionImpl(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher);
  ~RecoveryKeyStoreConnectionImpl() override;

  std::unique_ptr<Request> UpdateRecoveryKeyStore(
      const CoreAccountInfo& account_info,
      const trusted_vault_pb::Vault& request,
      UpdateRecoveryKeyStoreCallback callback) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> URLLoaderFactory();

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_IMPL_H_

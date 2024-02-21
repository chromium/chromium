// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/trusted_vault_connection.h"

struct CoreAccountInfo;

namespace trusted_vault {

enum class UpdateRecoveryKeyStoreStatus {
  kSuccess,
  kTransientAccessTokenFetchError,
  kPersistentAccessTokenFetchError,
  kPrimaryAccountChangeAccessTokenFetchError,
  kNetworkError,
  kOtherError,
};

// RecoveryKeyStoreConnection supports interaction with the recovery key store
// service (internally named Vault).
class RecoveryKeyStoreConnection {
 public:
  using Request = TrustedVaultConnection::Request;
  using UpdateRecoveryKeyStoreCallback =
      base::OnceCallback<void(UpdateRecoveryKeyStoreStatus)>;

  RecoveryKeyStoreConnection() = default;
  RecoveryKeyStoreConnection(const RecoveryKeyStoreConnection& other) = delete;
  RecoveryKeyStoreConnection& operator=(
      const RecoveryKeyStoreConnection& other) = delete;
  virtual ~RecoveryKeyStoreConnection() = default;

  // Updates the recovery key store for a (user, device) pair. `request`
  // contains an identifier for the particular recovery key store to update.
  virtual std::unique_ptr<Request> UpdateRecoveryKeyStore(
      const CoreAccountInfo& account_info,
      const trusted_vault_pb::Vault& request,
      UpdateRecoveryKeyStoreCallback callback) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_H_

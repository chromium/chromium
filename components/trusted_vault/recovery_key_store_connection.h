// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_H_
#define COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/recovery_key_store_certificate.h"
#include "components/trusted_vault/trusted_vault_connection.h"

struct CoreAccountInfo;

namespace base {
class Clock;
}  // namespace base

namespace trusted_vault {

enum class RecoveryKeyStoreStatus {
  kSuccess,
  kTransientAccessTokenFetchError,
  kPersistentAccessTokenFetchError,
  kPrimaryAccountChangeAccessTokenFetchError,
  kNetworkError,
  kOtherError,
};

enum class RecoveryKeyStoreCertificateFetchStatus {
  // Fetching and parsing the certificates succeeded.
  kSuccess,

  // An error occurred when downloading either of the XML files.
  kNetworkError,

  // The files were downloaded successfully, but failed to parse, e.g. because a
  // certificate chain could not be built.
  kParseError,
};

// This is the subset of information from the recovery key store that Chrome
// uses.
struct RecoveryKeyStoreEntry {
  RecoveryKeyStoreEntry();
  RecoveryKeyStoreEntry(RecoveryKeyStoreEntry&&);
  RecoveryKeyStoreEntry& operator=(RecoveryKeyStoreEntry&&);
  ~RecoveryKeyStoreEntry();
  bool operator==(const RecoveryKeyStoreEntry&) const;

  // The identifier for the vault.
  std::vector<uint8_t> vault_handle;

  // The backend cohort public key.
  std::vector<uint8_t> backend_public_key;
};

// RecoveryKeyStoreConnection supports interaction with the recovery key store
// service (internally named Vault).
class RecoveryKeyStoreConnection {
 public:
  using Request = TrustedVaultConnection::Request;
  using UpdateRecoveryKeyStoreCallback =
      base::OnceCallback<void(RecoveryKeyStoreStatus)>;
  using ListRecoveryKeyStoresCallback =
      base::OnceCallback<void(base::expected<std::vector<RecoveryKeyStoreEntry>,
                                             RecoveryKeyStoreStatus>)>;
  using FetchRecoveryKeyStoreCertificatesCallback = base::OnceCallback<void(
      base::expected<RecoveryKeyStoreCertificate,
                     RecoveryKeyStoreCertificateFetchStatus>)>;

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

  // Lists the recovery key stores for a user.
  virtual std::unique_ptr<Request> ListRecoveryKeyStores(
      const CoreAccountInfo& account_info,
      ListRecoveryKeyStoresCallback callback) = 0;

  // Fetches and parses the recovery key store certificates, i.e. cert.xml and
  // cert.sig.xml files.
  // Certificates are validated against the current time, hence tests should
  // pass a fake |clock| so certificate validation does not break in the future.
  virtual std::unique_ptr<Request> FetchRecoveryKeyStoreCertificates(
      base::Clock* clock,
      FetchRecoveryKeyStoreCertificatesCallback callback) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_RECOVERY_KEY_STORE_CONNECTION_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_SYNC_ENCRYPTION_HANDLER_OBSERVER_LIST_H_
#define COMPONENTS_SYNC_NIGORI_SYNC_ENCRYPTION_HANDLER_OBSERVER_LIST_H_

#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/sync_encryption_handler.h"

namespace syncer {

class CustomPassphraseBootstrapToken;
class Cryptographer;
class RequiredPassphraseVerifier;

// Helper wrapper list in charge of broadcasting Sync encryption status updates
// to registered client observers. Bypasses SyncEncryptionHandler::Observer
// inheritance clashing.
class SyncEncryptionHandlerObserverList {
 public:
  SyncEncryptionHandlerObserverList();

  SyncEncryptionHandlerObserverList(const SyncEncryptionHandlerObserverList&) =
      delete;
  SyncEncryptionHandlerObserverList& operator=(
      const SyncEncryptionHandlerObserverList&) = delete;

  ~SyncEncryptionHandlerObserverList();

  void AddObserver(SyncEncryptionHandler::Observer* observer);
  void RemoveObserver(SyncEncryptionHandler::Observer* observer);

  void NotifyPassphraseRequired(
      const RequiredPassphraseVerifier& verifier) const;
  void NotifyPassphraseAccepted(
      const CustomPassphraseBootstrapToken& bootstrap_token) const;
  void NotifyTrustedVaultKeyRequired() const;
  void NotifyTrustedVaultKeyAccepted() const;
  void NotifyKeystoreKeysRequired() const;
  void NotifyKeystoreKeysAccepted() const;
  void NotifyEncryptedTypesChanged(DataTypeSet encrypted_types,
                                   bool encrypt_everything) const;
  void NotifyCryptographerStateChanged(Cryptographer* cryptographer,
                                       bool has_pending_keys) const;
  void NotifyPassphraseTypeChanged(PassphraseType type,
                                   base::Time passphrase_time) const;

 private:
  base::ObserverList<SyncEncryptionHandler::Observer> observers_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_SYNC_ENCRYPTION_HANDLER_OBSERVER_LIST_H_

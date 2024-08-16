// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENCRYPTION_HANDLER_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENCRYPTION_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/nigori/key_derivation_params.h"

namespace sync_pb {
class EncryptedData;
class NigoriSpecifics_TrustedVaultDebugInfo;
}  // namespace sync_pb

namespace syncer {

class Cryptographer;
class KeystoreKeysHandler;
class Nigori;
enum class PassphraseType;

// Sync's encryption handler. Handles tracking encrypted types, ensuring the
// cryptographer encrypts with the proper key and has the most recent keybag,
// and keeps the nigori node up to date.
// All methods must be invoked on the sync sequence.
class SyncEncryptionHandler {
 public:
  // All Observer methods are called on the sync sequence.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // Called when user interaction is required to obtain a valid passphrase for
    // decryption.
    // |key_derivation_params| are the parameters that should be used to obtain
    // the key from the passphrase.
    // |pending_keys| is a copy of the cryptographer's pending keys, that may be
    // cached by the frontend for subsequent use by the UI.
    virtual void OnPassphraseRequired(
        const KeyDerivationParams& key_derivation_params,
        const sync_pb::EncryptedData& pending_keys) = 0;

    // Called when the passphrase provided by the user has been accepted and is
    // now used to encrypt sync data. This gets invoked last, relative to other
    // relevant notifications corresponding to the same event, e.g.
    // OnCryptographerStateChanged().
    virtual void OnPassphraseAccepted() = 0;

    // Called when decryption keys are required in order to decrypt pending
    // Nigori keys and resume sync, for the TRUSTED_VAULT_PASSPHRASE case. This
    // can be resolved by calling AddTrustedVaultDecryptionKeys() with the
    // appropriate keys.
    virtual void OnTrustedVaultKeyRequired() = 0;

    // Called when the keys provided via AddTrustedVaultDecryptionKeys have been
    // accepted and there are no longer pending keys.
    virtual void OnTrustedVaultKeyAccepted() = 0;

    // Called when the set of encrypted types or the encrypt
    // everything flag has been changed. Note that this doesn't imply the
    // encryption is complete.
    //
    // |encrypted_types| will always be a superset of
    // AlwaysEncryptedUserTypes().  If |encrypt_everything| is
    // true, |encrypted_types| will be the set of all encryptable types.
    //
    // Until this function is called, observers can assume that the
    // set of encrypted types is AlwaysEncryptedUserTypes() and that the
    // encrypt everything flag is false.
    virtual void OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                                         bool encrypt_everything) = 0;

    // The cryptographer has been updated and/or the presence of pending keys
    // changed.
    virtual void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                             bool has_pending_keys) = 0;

    // The passphrase type has changed. |type| is the new type,
    // |passphrase_time| is the time the passphrase was set (unset if |type|
    // is KEYSTORE_PASSPHRASE or the passphrase was set before we started
    // recording the time).
    virtual void OnPassphraseTypeChanged(PassphraseType type,
                                         base::Time passphrase_time) = 0;
  };

  SyncEncryptionHandler() = default;
  virtual ~SyncEncryptionHandler() = default;

  // Add/Remove SyncEncryptionHandler::Observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void NotifyInitialStateToObservers() = 0;

  virtual DataTypeSet GetEncryptedTypes() = 0;

  virtual Cryptographer* GetCryptographer() = 0;

  virtual PassphraseType GetPassphraseType() = 0;

  // Attempts to re-encrypt encrypted data types using the passphrase provided.
  // Notifies observers of the result of the operation via
  // OnPassphraseAccepted() or OnPassphraseRequired(), updates the nigori node,
  // and triggers re-encryption as appropriate. If an explicit password has been
  // set previously, we drop subsequent requests to set a passphrase.
  // |passphrase| shouldn't be empty.
  virtual void SetEncryptionPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params) = 0;

  // Provides a key for decrypting the user's existing sync data.
  // Notifies observers of the result of the operation via
  // OnPassphraseAccepted() or OnPassphraseRequired() and triggers re-encryption
  // as appropriate. It is an error to call this when we don't have pending
  // keys.
  virtual void SetExplicitPassphraseDecryptionKey(
      std::unique_ptr<Nigori> key) = 0;

  // Analogous to SetExplicitPassphraseDecryptionKey() but specifically for
  // TRUSTED_VAULT_PASSPHRASE: it provides new decryption keys that could
  // allow decrypting pending Nigori keys. Notifies observers of the result of
  // the operation via OnTrustedVaultKeyAccepted if the provided keys
  // successfully decrypted pending keys.
  virtual void AddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys) = 0;

  // Returns the time when Nigori was migrated to keystore or when it was
  // initialized in case it happens after migration was introduced. Returns
  // base::Time() in case migration isn't completed.
  virtual base::Time GetKeystoreMigrationTime() = 0;

  // Returns KeystoreKeysHandler, allowing to pass new keystore keys and to
  // check whether keystore keys need to be requested from the server.
  virtual KeystoreKeysHandler* GetKeystoreKeysHandler() = 0;

  // Returns debug information related to trusted vault passphrase type.
  virtual const sync_pb::NigoriSpecifics_TrustedVaultDebugInfo&
  GetTrustedVaultDebugInfo() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENCRYPTION_HANDLER_H_

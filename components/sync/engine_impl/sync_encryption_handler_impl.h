// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_SYNC_ENCRYPTION_HANDLER_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_SYNC_ENCRYPTION_HANDLER_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/nigori/keystore_keys_handler.h"
#include "components/sync/syncable/directory_cryptographer.h"
#include "components/sync/syncable/nigori_handler.h"

namespace syncer {

class Encryptor;
struct UserShare;
class WriteNode;
class WriteTransaction;

// Sync encryption handler implementation.
//
// This class acts as the respository of all sync encryption state, and handles
// encryption related changes/queries coming from both the chrome side and
// the sync side (via NigoriHandler). It is capable of modifying all sync data
// (re-encryption), updating the encrypted types, changing the encryption keys,
// and creating/receiving nigori node updates.
//
// The class should live as long as the directory itself in order to ensure
// any data read/written is properly decrypted/encrypted.
//
// |random_salt_generator| is a callback that accepts no arguments and returns a
// random salt. Used with scrypt key derivation method.
//
// Note: See sync_encryption_handler.h for a description of the chrome visible
// methods and what they do, and nigori_handler.h and keystore_keys_handler.h
// for a description of the sync methods.
// All methods are non-thread-safe and should only be called from the sync
// thread unless explicitly noted otherwise.
class SyncEncryptionHandlerImpl : public KeystoreKeysHandler,
                                  public SyncEncryptionHandler,
                                  public syncable::NigoriHandler {
 public:
  // |encryptor| and |user_share| must outlive this object.
  SyncEncryptionHandlerImpl(
      UserShare* user_share,
      const Encryptor* encryptor,
      const std::string& restored_key_for_bootstrapping,
      const std::string& restored_keystore_key_for_bootstrapping,
      const base::RepeatingCallback<std::string()>& random_salt_generator);
  ~SyncEncryptionHandlerImpl() override;

  // SyncEncryptionHandler implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool Init() override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  void SetDecryptionPassphrase(const std::string& passphrase) override;
  void AddTrustedVaultDecryptionKeys(
      const std::vector<std::string>& keys) override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  base::Time GetKeystoreMigrationTime() const override;
  KeystoreKeysHandler* GetKeystoreKeysHandler() override;
  std::string GetLastKeystoreKey() const override;

  // NigoriHandler implementation.
  // Note: all methods are invoked while the caller holds a transaction.
  bool ApplyNigoriUpdate(const sync_pb::NigoriSpecifics& nigori,
                         syncable::BaseTransaction* const trans) override;
  void UpdateNigoriFromEncryptedTypes(
      sync_pb::NigoriSpecifics* nigori,
      const syncable::BaseTransaction* const trans) const override;
  // Can be called from any thread.
  const Cryptographer* GetCryptographer(
      const syncable::BaseTransaction* const trans) const override;
  const DirectoryCryptographer* GetDirectoryCryptographer(
      const syncable::BaseTransaction* const trans) const override;
  ModelTypeSet GetEncryptedTypes(
      const syncable::BaseTransaction* const trans) const override;
  PassphraseType GetPassphraseType(
      const syncable::BaseTransaction* const trans) const override;

  // KeystoreKeysHandler implementation.
  bool NeedKeystoreKey() const override;
  bool SetKeystoreKeys(const std::vector<std::string>& keys) override;

  // Unsafe getters. Use only if sync is not up and running and there is no risk
  // of other threads calling this.

  ModelTypeSet GetEncryptedTypesUnsafe();

  bool MigratedToKeystore();
  base::Time custom_passphrase_time() const;

  // Restore a saved nigori obtained from OnLocalSetPassphraseEncryption.
  //
  // Writes the nigori to the Directory and updates the Cryptographer.
  void RestoreNigoriForTesting(
      const sync_pb::NigoriSpecifics& nigori_specifics);

  // Returns mutable DirectoryCryptographer, used only in tests to manipulate it
  // directly.
  DirectoryCryptographer* GetMutableCryptographerForTesting();

 private:
  friend class SyncEncryptionHandlerImplTest;
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           NigoriEncryptionTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           EncryptEverythingExplicit);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           EncryptEverythingImplicit);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           UnknownSensitiveTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest, GetKeystoreDecryptor);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           ReceiveMigratedNigoriKeystorePass);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           ReceiveUmigratedNigoriAfterMigration);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           ReceiveOldMigratedNigori);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           SetKeystoreAfterReceivingMigratedNigori);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           SetCustomPassAfterMigration);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           SetCustomPassAfterMigrationNoKeystoreKey);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           SetImplicitPassAfterMigrationNoKeystoreKey);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           MigrateOnEncryptEverythingKeystorePassphrase);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           ReceiveMigratedNigoriWithOldPassphrase);

  // Container for members that require thread safety protection.  All members
  // that can be accessed from more than one thread should be held here and
  // accessed via UnlockVault(..) and UnlockVaultMutable(..), which enforce
  // that a transaction is held.
  struct Vault {
    Vault(ModelTypeSet encrypted_types, PassphraseType passphrase_type);
    ~Vault();

    // Sync's cryptographer. Used for encrypting and decrypting sync data.
    DirectoryCryptographer cryptographer;
    // The set of types that require encryption.
    ModelTypeSet encrypted_types;
    // The current state of the passphrase required to decrypt the encryption
    // keys stored in the nigori node.
    PassphraseType passphrase_type;

   private:
    DISALLOW_COPY_AND_ASSIGN(Vault);
  };

  // Enumeration of methods, which can trigger Nigori migration to keystore
  // either directly (by calling AttemptToMigrateNigoriToKeystore) or indirectly
  // (by calling WriteEncryptionStateToNigori or RewriteNigori). Used only for
  // UMA metrics. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class NigoriMigrationTrigger {
    kApplyNigoriUpdate = 0,
    kEnableEncryptEverything = 1,
    kFinishSetPassphrase = 2,
    kInit = 3,
    kSetKeystoreKeys = 4,
    kMaxValue = kSetKeystoreKeys
  };

  // Enumeration of possible reasons to trigger Nigori migration to keystore
  // (see GetMigrationReason). These values are persisted to logs. Entries
  // should not be renumbered and numeric values should never be reused.
  enum class NigoriMigrationReason {
    kNoReason = 0,
    kCannotDecryptUsingDefaultKey = 1,
    kEncryptEverythingWithKeystorePassphrase = 2,
    KNigoriNotMigrated = 3,
    kNotEncryptEverythingWithExplicitPassphrase = 4,
    kOldPassphraseType = 5,
    kServerKeyRotation = 6,
    kInitialization = 7,
    kMaxValue = kInitialization
  };

  // Enumeration of possible outcomes of ApplyNigoriUpdateImpl.
  enum class ApplyNigoriUpdateResult {
    kSuccess,
    kUnsupportedRemoteState,
    kRemoteMustBeCorrected,
  };

  // Iterate over all encrypted types ensuring each entry is properly encrypted.
  void ReEncryptEverything(WriteTransaction* trans);

  // Updates internal and cryptographer state.
  //
  // Assumes |nigori| is already present in the Sync Directory.
  //
  // Note: must be called from within a transaction.
  ApplyNigoriUpdateResult ApplyNigoriUpdateImpl(
      const sync_pb::NigoriSpecifics& nigori,
      syncable::BaseTransaction* const trans);

  // Wrapper around WriteEncryptionStateToNigori that creates a new write
  // transaction. Because this function can trigger a migration,
  // |migration_trigger| allows distinguishing the "cause" of the migration,
  // for UMA purposes.
  void RewriteNigori(NigoriMigrationTrigger migration_trigger);

  // Write the current encryption state into the nigori node. This includes
  // the encrypted types/encrypt everything state, as well as the keybag/
  // explicit passphrase state (if the cryptographer is ready). Because this
  // function can trigger a migration, |migration_trigger| allows
  // distinguishing the "cause" of the migration, for UMA purposes.
  void WriteEncryptionStateToNigori(WriteTransaction* trans,
                                    NigoriMigrationTrigger migration_trigger);

  // Updates local encrypted types from |nigori|.
  // Returns true if the local set of encrypted types either matched or was
  // a subset of that in |nigori|. Returns false if the local state already
  // had stricter encryption than |nigori|, and the nigori node needs to be
  // updated with the newer encryption state.
  // Note: must be called from within a transaction.
  bool UpdateEncryptedTypesFromNigori(const sync_pb::NigoriSpecifics& nigori,
                                      syncable::BaseTransaction* const trans);

  // If the Nigori node doesn't contain an explicit custom passphrase key
  // derivation method, it means it was committed with a previous version
  // which was unaware of this field and implicitly used PBKDF2. This method
  // checks for this condition and explicitly writes PBKDF2 as the key
  // derivation method.
  void ReplaceImplicitKeyDerivationMethodInNigori(WriteTransaction* trans);

  // Same as ReplaceImplicitKeyDerivationMethodInNigori, just
  // wrapped in a write transaction.
  void ReplaceImplicitKeyDerivationMethodInNigoriWithTransaction();

  // TODO(zea): make these public and have them replace SetEncryptionPassphrase
  // and SetDecryptionPassphrase.
  // Helper methods for handling passphrases once keystore migration has taken
  // place.
  //
  // Sets a new custom passphrase. Should only be called if a custom passphrase
  // is not already set.
  // Triggers OnPassphraseAccepted on success, OnPassphraseRequired if a custom
  // passphrase already existed.
  void SetCustomPassphrase(const std::string& passphrase,
                           WriteTransaction* trans,
                           WriteNode* nigori_node);

  // Decrypt the encryption keybag using a user provided passphrase.
  // Should only be called if the current passphrase is a frozen implicit
  // passphrase or a custom passphrase.
  // Triggers OnPassphraseAccepted on success, OnPassphraseRequired on failure.
  void DecryptPendingKeysWithExplicitPassphrase(
      const std::string& passphrase,
      WriteTransaction* trans,
      WriteNode* nigori_node);

  // The final step of SetEncryptionPassphrase and SetDecryptionPassphrase that
  // notifies observers of the result of the set passphrase operation, updates
  // the nigori node, and does re-encryption.
  // |success|: true if the operation was successful and false otherwise. If
  //            success == false, we send an OnPassphraseRequired notification.
  // |bootstrap_token|: used to inform observers if the cryptographer's
  //                    bootstrap token was updated.
  // |is_explicit|: used to differentiate between a custom passphrase (true) and
  //                a GAIA passphrase that is implicitly used for encryption
  //                (false).
  void FinishSetPassphrase(bool success,
                           const std::string& bootstrap_token,
                           WriteTransaction* trans,
                           WriteNode* nigori_node);

  // Merges the given set of encrypted types with the existing set and emits a
  // notification if necessary.
  // Note: must be called from within a transaction.
  void MergeEncryptedTypes(ModelTypeSet new_encrypted_types,
                           syncable::BaseTransaction* const trans);

  // Helper methods for ensuring transactions are held when accessing
  // |vault_unsafe_|.
  Vault* UnlockVaultMutable(const syncable::BaseTransaction* const trans);
  const Vault& UnlockVault(const syncable::BaseTransaction* const trans) const;

  // Helper method for determining if migration of a nigori node should be
  // triggered or not. In case migration shouldn't be triggered the method will
  // return kNoReason value. Other values mean migration should be triggered
  // and used itself only for UMA.
  // Conditions for triggering migration:
  // 1. Cryptographer has no pending keys
  // 2. Nigori node isn't already properly migrated or we need to rotate keys.
  // 3. Keystore key is available.
  // Note: if the nigori node is migrated but has an invalid state, migration
  // should be triggered (e.g. node has KEYSTORE_PASSPHRASE, local is
  // CUSTOM_PASSPHRASE).
  NigoriMigrationReason GetMigrationReason(
      const sync_pb::NigoriSpecifics& nigori,
      const DirectoryCryptographer& cryptographer,
      PassphraseType passphrase_type) const;

  // Tries to perform the actual migration of the |nigori_node| to support
  // keystore encryption unless GetMigrationReason(..) returns kNoReason.
  // Returns true if migration was attempted and successful.
  bool AttemptToMigrateNigoriToKeystore(
      WriteTransaction* trans,
      WriteNode* nigori_node,
      NigoriMigrationTrigger migration_trigger);

  // Fill |encrypted_blob| with the keystore decryptor token if
  // |encrypted_blob|'s contents didn't already contain the key.
  // The keystore decryptor token is the serialized current default encryption
  // key, encrypted with the keystore key.
  bool GetKeystoreDecryptor(const DirectoryCryptographer& cryptographer,
                            const std::string& keystore_key,
                            sync_pb::EncryptedData* encrypted_blob);

  // Helper method for installing the keys encrypted in |encryption_keybag|
  // into |cryptographer|.
  // Returns true on success, false if we were unable to install the keybag.
  // Will not update the default key.
  bool AttemptToInstallKeybag(const sync_pb::EncryptedData& keybag,
                              bool update_default,
                              DirectoryCryptographer* cryptographer);

  // Helper method for decrypting pending keys with the keystore bootstrap.
  // If successful, the default will become the key encrypted in the keystore
  // bootstrap, and will return true. Else will return false.
  bool DecryptPendingKeysWithKeystoreKey(
      const sync_pb::EncryptedData& keystore_bootstrap,
      DirectoryCryptographer* cryptographer);

  // Helper to enable encrypt everything, notifying observers if necessary.
  // Will not perform re-encryption.
  void EnableEncryptEverythingImpl(syncable::BaseTransaction* const trans);

  // If an explicit passphrase is in use, returns the time at which it was set
  // (if known). Else return base::Time().
  base::Time GetExplicitPassphraseTime(PassphraseType passphrase_type) const;

  // Notify observers when a custom passphrase is set by this device.
  void NotifyObserversOfLocalCustomPassphrase(WriteTransaction* trans);

  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<SyncEncryptionHandler::Observer>::Unchecked observers_;

  // The current user share (for creating transactions).
  UserShare* const user_share_;

  // Used for encryption/decryption of keystore keys and the key derived from
  // custom passphrase in order to store them locally.
  const Encryptor* const encryptor_;

  // Container for all data that can be accessed from multiple threads. Do not
  // access this object directly. Instead access it via UnlockVault(..) and
  // UnlockVaultMutable(..).
  Vault vault_unsafe_;

  // Sync encryption state that is only modified and accessed from the sync
  // thread.
  // Whether all current and future types should be encrypted.
  bool encrypt_everything_;

  // The current keystore key provided by the server.
  std::string keystore_key_;

  // The set of old keystore keys. Every time a key rotation occurs, the server
  // sends down all previous keystore keys as well as the new key. We preserve
  // the old keys so that when we re-encrypt we can ensure they're all added to
  // the keybag (and to detect that a key rotation has occurred).
  std::vector<std::string> old_keystore_keys_;

  // The number of times we've automatically (i.e. not via SetPassphrase or
  // conflict resolver) updated the nigori's encryption keys in this chrome
  // instantiation.
  int nigori_overwrite_count_;

  // The time the nigori was migrated to support keystore encryption.
  base::Time keystore_migration_time_;

  // The time the custom passphrase was set for this account. Not valid
  // if there is no custom passphrase or the custom passphrase was set
  // before support for this field was added.
  base::Time custom_passphrase_time_;

  // The key derivation params we are using for the custom passphrase. This can
  // end up not being set e.g. in cases when we reach a CUSTOM_PASSPHRASE state
  // through a legacy code path.
  base::Optional<KeyDerivationParams> custom_passphrase_key_derivation_params_;

  base::RepeatingCallback<std::string()> random_salt_generator_;

  // Determines whether Nigori migration was triggered. Used for UMA metric
  // only.
  bool migration_attempted_;

  base::WeakPtrFactory<SyncEncryptionHandlerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncEncryptionHandlerImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_SYNC_ENCRYPTION_HANDLER_IMPL_H_

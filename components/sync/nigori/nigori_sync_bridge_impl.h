// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_error.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori_local_change_processor.h"
#include "components/sync/nigori/nigori_state.h"
#include "components/sync/nigori/nigori_sync_bridge.h"

namespace sync_pb {
class NigoriLocalData;
class NigoriSpecifics;
}  // namespace sync_pb

namespace syncer {

class KeyDerivationParams;
class NigoriStorage;
class PendingLocalNigoriCommit;

// USS implementation of SyncEncryptionHandler.
// This class holds the current Nigori state and processes incoming changes and
// queries:
// 1. Serves observers of SyncEncryptionHandler interface.
// 2. Allows the passphrase manipulations (via SyncEncryptionHandler).
// 3. Communicates local and remote changes with a processor (via
// NigoriSyncBridge).
// 4. Handles keystore keys from a sync server (via KeystoreKeysHandler).
class NigoriSyncBridgeImpl : public KeystoreKeysHandler,
                             public NigoriSyncBridge,
                             public SyncEncryptionHandler {
 public:
  NigoriSyncBridgeImpl(std::unique_ptr<NigoriLocalChangeProcessor> processor,
                       std::unique_ptr<NigoriStorage> storage);

  NigoriSyncBridgeImpl(const NigoriSyncBridgeImpl&) = delete;
  NigoriSyncBridgeImpl& operator=(const NigoriSyncBridgeImpl&) = delete;

  ~NigoriSyncBridgeImpl() override;

  // SyncEncryptionHandler implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyInitialStateToObservers() override;
  DataTypeSet GetEncryptedTypes() override;
  Cryptographer* GetCryptographer() override;
  PassphraseType GetPassphraseType() override;
  void SetEncryptionPassphrase(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params) override;
  void SetExplicitPassphraseDecryptionKey(std::unique_ptr<Nigori> key) override;
  void AddTrustedVaultDecryptionKeys(
      const std::vector<std::vector<uint8_t>>& keys) override;
  base::Time GetKeystoreMigrationTime() override;
  KeystoreKeysHandler* GetKeystoreKeysHandler() override;
  const sync_pb::NigoriSpecifics_TrustedVaultDebugInfo&
  GetTrustedVaultDebugInfo() override;

  // KeystoreKeysHandler implementation.
  bool NeedKeystoreKey() const override;
  bool SetKeystoreKeys(const std::vector<std::vector<uint8_t>>& keys) override;

  // NigoriSyncBridge implementation.
  std::optional<ModelError> MergeFullSyncData(
      std::optional<EntityData> data) override;
  std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::optional<EntityData> data) override;
  std::unique_ptr<EntityData> GetDataForCommit() override;
  std::unique_ptr<EntityData> GetDataForDebugging() override;
  void ApplyDisableSyncChanges() override;

  const CryptographerImpl& GetCryptographerImplForTesting() const;
  bool HasPendingKeysForTesting() const;
  KeyDerivationParams GetCustomPassphraseKeyDerivationParamsForTesting() const;

 private:
  std::optional<ModelError> UpdateLocalState(
      const sync_pb::NigoriSpecifics& specifics);

  // Builds NigoriKeyBag, which contains keys acceptable for decryption of
  // |encryption_keybag| from remote NigoriSpecifics. Its content depends on
  // current passphrase type and available keys: it contains current default
  // encryption key, for KEYSTORE_PASSPHRASE it additionally contains key
  // obtained from |keystore_decryptor_token| and all keystore keys.
  NigoriKeyBag BuildDecryptionKeyBagForRemoteKeybag() const;

  // Uses |key_bag| to try to decrypt pending keys as represented in
  // |state_.pending_keys| (which must be set).
  //
  // If decryption is possible, the newly decrypted keys are put in the
  // |state_.cryptographer|'s keybag and the default key is updated. In that
  // case pending keys are cleared.
  //
  // If |key_bag| is not capable of decrypting pending keys,
  // |state_.pending_keys| stays set. Such outcome is not itself considered
  // and error and returns std::nullopt.
  //
  // Errors may be returned, in rare cases, for fatal protocol violations.
  std::optional<ModelError> TryDecryptPendingKeysWith(
      const NigoriKeyBag& key_bag);

  base::Time GetExplicitPassphraseTime() const;

  // Returns key derivation params based on |passphrase_type_| and
  // |custom_passphrase_key_derivation_params_|. Should be called only if
  // |passphrase_type_| is an explicit passphrase.
  KeyDerivationParams GetKeyDerivationParamsForPendingKeys() const;

  // If there are pending keys and depending on the passphrase type, it invokes
  // the appropriate observer methods (if any).
  void MaybeNotifyOfPendingKeys() const;

  // Queues keystore rotation or full keystore migration if current state
  // assumes it should happen.
  void MaybeTriggerKeystoreReencryption();

  // Serializes state of the bridge and sync metadata into the proto.
  sync_pb::NigoriLocalData SerializeAsNigoriLocalData() const;

  // Appends |local_commit| to |pending_local_commit_queue_| and if appropriate
  // calls Put() to trigger the commit.
  void QueuePendingLocalCommit(
      std::unique_ptr<PendingLocalNigoriCommit> local_commit);

  // Processes |pending_local_commit_queue_| FIFO such that all non-applicable
  // pending commits issue a failure, until the first one that is applicable is
  // found (if any). If such applicable commit is found, the corresponding Put()
  // call is issued.
  void PutNextApplicablePendingLocalCommit();

  // Populates keystore keys into |cryptographer| in case it doesn't contain
  // them already and |passphrase_type| isn't KEYSTORE_PASSPHRASE. This
  // function only updates local state and doesn't trigger a commit.
  void MaybePopulateKeystoreKeysIntoCryptographer();

  std::unique_ptr<EntityData> GetDataImpl();

  const std::unique_ptr<NigoriLocalChangeProcessor> processor_;
  const std::unique_ptr<NigoriStorage> storage_;

  syncer::NigoriState state_;

  std::list<std::unique_ptr<PendingLocalNigoriCommit>>
      pending_local_commit_queue_;

  // Observer that owns the list of actual observers, and broadcasts
  // notifications to all observers in the list.
  class BroadcastingObserver;
  const std::unique_ptr<BroadcastingObserver> broadcasting_observer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_

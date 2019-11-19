// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_REGISTRY_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/model_safe_worker.h"
#include "components/sync/engine/model_type_connector.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine_impl/nudge_handler.h"
#include "components/sync/engine_impl/uss_migrator.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

class CancelationSignal;
class CommitContributor;
class DataTypeDebugInfoEmitter;
class DirectoryCommitContributor;
class DirectoryUpdateHandler;
class KeystoreKeysHandler;
class ModelTypeWorker;
class UpdateHandler;

using UpdateHandlerMap = std::map<ModelType, UpdateHandler*>;
using CommitContributorMap = std::map<ModelType, CommitContributor*>;

// Keeps track of the sets of active update handlers and commit contributors.
class ModelTypeRegistry : public ModelTypeConnector,
                          public SyncEncryptionHandler::Observer {
 public:
  ModelTypeRegistry(const std::vector<scoped_refptr<ModelSafeWorker>>& workers,
                    UserShare* user_share,
                    NudgeHandler* nudge_handler,
                    const UssMigrator& uss_migrator,
                    CancelationSignal* cancelation_signal,
                    KeystoreKeysHandler* keystore_keys_handler);
  ~ModelTypeRegistry() override;

  // Enables an off-thread type for syncing.  Connects the given proxy
  // and its task_runner to the newly created worker.
  //
  // Expects that the proxy's ModelType is not currently enabled.
  void ConnectNonBlockingType(
      ModelType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) override;

  // Disables the syncing of an off-thread type.
  //
  // Expects that the type is currently enabled.
  // Deletes the worker associated with the type.
  void DisconnectNonBlockingType(ModelType type) override;

  // Creates update handler and commit contributor objects for directory type.
  // Expects that the type is not yet registered.
  void RegisterDirectoryType(ModelType type, ModelSafeGroup group) override;

  // Deletes objects related to directory type. Expects that the type is
  // registered.
  void UnregisterDirectoryType(ModelType type) override;

  // Implementation of SyncEncryptionHandler::Observer.
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override;

  // Gets the set of enabled types.
  ModelTypeSet GetEnabledTypes() const;

  // Returns set of types for which initial set of updates was downloaded and
  // applied.
  ModelTypeSet GetInitialSyncEndedTypes() const;

  // Returns the update handler for |type|.
  const UpdateHandler* GetUpdateHandler(ModelType type) const;

  // Simple getters.
  UpdateHandlerMap* update_handler_map();
  CommitContributorMap* commit_contributor_map();
  KeystoreKeysHandler* keystore_keys_handler();

  void RegisterDirectoryTypeDebugInfoObserver(TypeDebugInfoObserver* observer);
  void UnregisterDirectoryTypeDebugInfoObserver(
      TypeDebugInfoObserver* observer);
  bool HasDirectoryTypeDebugInfoObserver(
      const TypeDebugInfoObserver* observer) const;
  void RequestEmitDebugInfo();

  bool HasUnsyncedItems() const;

  base::WeakPtr<ModelTypeConnector> AsWeakPtr();

 private:
  using DataTypeDebugInfoEmitterMap =
      std::map<ModelType, std::unique_ptr<DataTypeDebugInfoEmitter>>;

  void OnEncryptionStateChanged();

  // DebugInfoEmitters are never deleted. Returns an existing one if we have it.
  DataTypeDebugInfoEmitter* GetEmitter(ModelType type);

  ModelTypeSet GetEnabledNonBlockingTypes() const;
  ModelTypeSet GetEnabledDirectoryTypes() const;

  syncable::Directory* directory() const {
    return user_share_->directory.get();
  }

  // Sets of handlers and contributors.
  std::map<ModelType, std::unique_ptr<DirectoryCommitContributor>>
      directory_commit_contributors_;
  std::map<ModelType, std::unique_ptr<DirectoryUpdateHandler>>
      directory_update_handlers_;

  std::vector<std::unique_ptr<ModelTypeWorker>> model_type_workers_;

  // Maps of UpdateHandlers and CommitContributors.
  // They do not own any of the objects they point to.
  UpdateHandlerMap update_handler_map_;
  CommitContributorMap commit_contributor_map_;

  // Map of DebugInfoEmitters for directory types and Non-blocking types.
  // Does not own its contents.
  DataTypeDebugInfoEmitterMap data_type_debug_info_emitter_map_;

  // The known ModelSafeWorkers.
  std::map<ModelSafeGroup, scoped_refptr<ModelSafeWorker>> workers_map_;

  UserShare* const user_share_;

  // A copy of the directory's most recent cryptographer.
  std::unique_ptr<Cryptographer> cryptographer_;

  // A copy of the directory's most recent passphrase type.
  PassphraseType passphrase_type_ =
      SyncEncryptionHandler::kInitialPassphraseType;

  // The set of encrypted types.
  ModelTypeSet encrypted_types_;

  NudgeHandler* const nudge_handler_;

  // Function to call to migrate data from the directory to USS.
  UssMigrator uss_migrator_;

  // CancelationSignal is signalled on engine shutdown. It is passed to
  // ModelTypeWorker to cancel blocking operation.
  CancelationSignal* const cancelation_signal_;

  KeystoreKeysHandler* const keystore_keys_handler_;

  // The set of observers of per-type debug info.
  //
  // Each of the DirectoryTypeDebugInfoEmitters needs such a list.  There's
  // a lot of them, and their lifetimes are unpredictable, so it makes the
  // book-keeping easier if we just store the list here.  That way it's
  // guaranteed to live as long as this sync backend.
  base::ObserverList<TypeDebugInfoObserver>::Unchecked
      type_debug_info_observers_;

  base::WeakPtrFactory<ModelTypeRegistry> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ModelTypeRegistry);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_MODEL_TYPE_REGISTRY_H_

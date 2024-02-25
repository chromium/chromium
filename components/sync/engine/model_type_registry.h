// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/model_type_connector.h"
#include "components/sync/engine/nudge_handler.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/engine/update_handler.h"

namespace syncer {

class CancelationSignal;
class CommitContributor;
class SyncEncryptionHandler;
class ModelTypeWorker;
class UpdateHandler;

using UpdateHandlerMap = std::map<ModelType, UpdateHandler*>;
using CommitContributorMap = std::map<ModelType, CommitContributor*>;

// Keeps track of the sets of active update handlers and commit contributors.
class ModelTypeRegistry : public ModelTypeConnector,
                          public SyncEncryptionHandler::Observer {
 public:
  // |nudge_handler|, |cancelation_signal| and |sync_encryption_handler| must
  // outlive this object.
  ModelTypeRegistry(NudgeHandler* nudge_handler,
                    CancelationSignal* cancelation_signal,
                    SyncEncryptionHandler* sync_encryption_handler);

  ModelTypeRegistry(const ModelTypeRegistry&) = delete;
  ModelTypeRegistry& operator=(const ModelTypeRegistry&) = delete;

  ~ModelTypeRegistry() override;

  // Implementation of ModelTypeConnector.
  void ConnectDataType(
      ModelType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) override;
  void DisconnectDataType(ModelType type) override;

  // Implementation of SyncEncryptionHandler::Observer.
  void OnPassphraseRequired(
      const KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnTrustedVaultKeyRequired() override;
  void OnTrustedVaultKeyAccepted() override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnCryptographerStateChanged(Cryptographer* cryptographer,
                                   bool has_pending_keys) override;
  void OnPassphraseTypeChanged(PassphraseType type,
                               base::Time passphrase_time) override;

  // Gets the set of connected types, which is essentially the set of types that
  // the sync engine cares about. For each of these, a worker exists to
  // propagate changes between the server and the local model's processor.
  ModelTypeSet GetConnectedTypes() const;

  // Returns set of types for which initial set of updates was downloaded and
  // applied.
  ModelTypeSet GetInitialSyncEndedTypes() const;

  // Returns the update handler for |type|. If UpdateHandler of |type| doesn't
  // exist, returns nullptr.
  const UpdateHandler* GetUpdateHandler(ModelType type) const;
  UpdateHandler* GetMutableUpdateHandler(ModelType type);

  // Simple getters.
  UpdateHandlerMap* update_handler_map();
  CommitContributorMap* commit_contributor_map();
  KeystoreKeysHandler* keystore_keys_handler();

  // Returns types that have local changes yet to be synced to the server.
  ModelTypeSet GetTypesWithUnsyncedData() const;

  bool HasUnsyncedItems() const;

  const std::vector<std::unique_ptr<ModelTypeWorker>>&
  GetConnectedModelTypeWorkersForTest() const;

  base::WeakPtr<ModelTypeConnector> AsWeakPtr();

 private:
  std::vector<std::unique_ptr<ModelTypeWorker>> connected_model_type_workers_;

  // Maps of UpdateHandlers and CommitContributors.
  // They do not own any of the objects they point to.
  UpdateHandlerMap update_handler_map_;
  CommitContributorMap commit_contributor_map_;

  const raw_ptr<NudgeHandler> nudge_handler_;

  // CancelationSignal is signalled on engine shutdown. It is passed to
  // ModelTypeWorker to cancel blocking operation.
  const raw_ptr<CancelationSignal> cancelation_signal_;

  const raw_ptr<SyncEncryptionHandler> sync_encryption_handler_;

  base::WeakPtrFactory<ModelTypeRegistry> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_REGISTRY_H_

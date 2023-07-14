// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_registry.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine/model_type_worker.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"

namespace syncer {

ModelTypeRegistry::ModelTypeRegistry(
    NudgeHandler* nudge_handler,
    CancelationSignal* cancelation_signal,
    SyncEncryptionHandler* sync_encryption_handler)
    : nudge_handler_(nudge_handler),
      cancelation_signal_(cancelation_signal),
      sync_encryption_handler_(sync_encryption_handler) {
  sync_encryption_handler_->AddObserver(this);
}

ModelTypeRegistry::~ModelTypeRegistry() {
  sync_encryption_handler_->RemoveObserver(this);
}

void ModelTypeRegistry::ConnectDataType(
    ModelType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(ProtocolTypes().Has(type));
  DCHECK(update_handler_map_.find(type) == update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) == commit_contributor_map_.end());
  DCHECK(activation_response);
  DCHECK(!activation_response->skip_engine_connection);
  DCHECK(activation_response->type_processor);

  DVLOG(1) << "Enabling an off-thread sync type: "
           << ModelTypeToDebugString(type);

  auto worker = std::make_unique<ModelTypeWorker>(
      type, activation_response->model_type_state,
      sync_encryption_handler_->GetCryptographer(),
      sync_encryption_handler_->GetEncryptedTypes().Has(type),
      sync_encryption_handler_->GetPassphraseType(), nudge_handler_,
      cancelation_signal_);

  // Save a raw pointer and add the worker to our structures.
  ModelTypeWorker* worker_ptr = worker.get();
  connected_model_type_workers_.push_back(std::move(worker));
  update_handler_map_.insert(std::make_pair(type, worker_ptr));
  commit_contributor_map_.insert(std::make_pair(type, worker_ptr));

  worker_ptr->ConnectSync(std::move(activation_response->type_processor));
}

void ModelTypeRegistry::DisconnectDataType(ModelType type) {
  if (update_handler_map_.count(type) == 0) {
    // Type not connected. Simply ignore.
    return;
  }

  DVLOG(1) << "Disabling an off-thread sync type: "
           << ModelTypeToDebugString(type);

  DCHECK(ProtocolTypes().Has(type));
  DCHECK(update_handler_map_.find(type) != update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) != commit_contributor_map_.end());

  size_t updaters_erased = update_handler_map_.erase(type);
  size_t committers_erased = commit_contributor_map_.erase(type);

  DCHECK_EQ(1U, updaters_erased);
  DCHECK_EQ(1U, committers_erased);

  auto iter = connected_model_type_workers_.begin();
  while (iter != connected_model_type_workers_.end()) {
    if ((*iter)->GetModelType() == type) {
      iter = connected_model_type_workers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void ModelTypeRegistry::SetProxyTabsDatatypeEnabled(bool enabled) {
  proxy_tabs_datatype_enabled_ = enabled;
}

ModelTypeSet ModelTypeRegistry::GetConnectedTypes() const {
  ModelTypeSet types;
  for (const std::unique_ptr<ModelTypeWorker>& worker :
       connected_model_type_workers_) {
    types.Put(worker->GetModelType());
  }
  return types;
}

bool ModelTypeRegistry::proxy_tabs_datatype_enabled() const {
  return proxy_tabs_datatype_enabled_;
}

ModelTypeSet ModelTypeRegistry::GetInitialSyncEndedTypes() const {
  ModelTypeSet result;
  for (const auto& [type, update_handler] : update_handler_map_) {
    if (update_handler->IsInitialSyncEnded())
      result.Put(type);
  }
  return result;
}

const UpdateHandler* ModelTypeRegistry::GetUpdateHandler(ModelType type) const {
  auto it = update_handler_map_.find(type);
  return it == update_handler_map_.end() ? nullptr : it->second;
}

UpdateHandler* ModelTypeRegistry::GetMutableUpdateHandler(ModelType type) {
  auto it = update_handler_map_.find(type);
  return it == update_handler_map_.end() ? nullptr : it->second;
}

UpdateHandlerMap* ModelTypeRegistry::update_handler_map() {
  return &update_handler_map_;
}

CommitContributorMap* ModelTypeRegistry::commit_contributor_map() {
  return &commit_contributor_map_;
}

KeystoreKeysHandler* ModelTypeRegistry::keystore_keys_handler() {
  return sync_encryption_handler_->GetKeystoreKeysHandler();
}

ModelTypeSet ModelTypeRegistry::GetTypesWithUnsyncedData() const {
  ModelTypeSet types;
  for (const std::unique_ptr<ModelTypeWorker>& worker :
       connected_model_type_workers_) {
    if (worker->HasLocalChanges()) {
      types.Put(worker->GetModelType());
    }
  }
  return types;
}

bool ModelTypeRegistry::HasUnsyncedItems() const {
  // For model type workers, we ask them individually.
  for (const std::unique_ptr<ModelTypeWorker>& worker :
       connected_model_type_workers_) {
    if (worker->HasLocalChanges()) {
      return true;
    }
  }

  return false;
}

const std::vector<std::unique_ptr<ModelTypeWorker>>&
ModelTypeRegistry::GetConnectedModelTypeWorkersForTest() const {
  return connected_model_type_workers_;
}

base::WeakPtr<ModelTypeConnector> ModelTypeRegistry::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ModelTypeRegistry::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {}

void ModelTypeRegistry::OnPassphraseAccepted() {}

void ModelTypeRegistry::OnTrustedVaultKeyRequired() {}

void ModelTypeRegistry::OnTrustedVaultKeyAccepted() {}

void ModelTypeRegistry::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                                bool encrypt_everything) {
  // This does NOT support disabling encryption without reconnecting the
  // type, i.e. recreating its ModelTypeWorker.
  for (const std::unique_ptr<ModelTypeWorker>& worker :
       connected_model_type_workers_) {
    if (encrypted_types.Has(worker->GetModelType())) {
      // No-op if the type was already encrypted.
      worker->EnableEncryption();
    }
  }
}

void ModelTypeRegistry::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  for (const std::unique_ptr<ModelTypeWorker>& worker :
       connected_model_type_workers_) {
    worker->OnCryptographerChange();
  }
}

void ModelTypeRegistry::OnPassphraseTypeChanged(PassphraseType type,
                                                base::Time passphrase_time) {
  for (const std::unique_ptr<ModelTypeWorker>& worker :
       connected_model_type_workers_) {
    worker->UpdatePassphraseType(type);
  }
}

}  // namespace syncer

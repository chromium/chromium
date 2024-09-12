// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_registry.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/data_type_processor.h"
#include "components/sync/engine/data_type_worker.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"

namespace syncer {

DataTypeRegistry::DataTypeRegistry(
    NudgeHandler* nudge_handler,
    CancelationSignal* cancelation_signal,
    SyncEncryptionHandler* sync_encryption_handler)
    : nudge_handler_(nudge_handler),
      cancelation_signal_(cancelation_signal),
      sync_encryption_handler_(sync_encryption_handler) {
  sync_encryption_handler_->AddObserver(this);
}

DataTypeRegistry::~DataTypeRegistry() {
  sync_encryption_handler_->RemoveObserver(this);
}

void DataTypeRegistry::ConnectDataType(
    DataType type,
    std::unique_ptr<DataTypeActivationResponse> activation_response) {
  DCHECK(ProtocolTypes().Has(type));
  DCHECK(update_handler_map_.find(type) == update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) == commit_contributor_map_.end());
  DCHECK(activation_response);
  DCHECK(!activation_response->skip_engine_connection);
  DCHECK(activation_response->type_processor);

  DVLOG(1) << "Enabling an off-thread sync type: "
           << DataTypeToDebugString(type);

  auto worker = std::make_unique<DataTypeWorker>(
      type, activation_response->data_type_state,
      sync_encryption_handler_->GetCryptographer(),
      sync_encryption_handler_->GetEncryptedTypes().Has(type),
      sync_encryption_handler_->GetPassphraseType(), nudge_handler_,
      cancelation_signal_);

  // Save a raw pointer and add the worker to our structures.
  DataTypeWorker* worker_ptr = worker.get();
  connected_data_type_workers_.push_back(std::move(worker));
  update_handler_map_.insert(std::make_pair(type, worker_ptr));
  commit_contributor_map_.insert(std::make_pair(type, worker_ptr));

  worker_ptr->ConnectSync(std::move(activation_response->type_processor));
}

void DataTypeRegistry::DisconnectDataType(DataType type) {
  if (update_handler_map_.count(type) == 0) {
    // Type not connected. Simply ignore.
    return;
  }

  DVLOG(1) << "Disabling an off-thread sync type: "
           << DataTypeToDebugString(type);

  DCHECK(ProtocolTypes().Has(type));
  DCHECK(update_handler_map_.find(type) != update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) != commit_contributor_map_.end());

  size_t updaters_erased = update_handler_map_.erase(type);
  size_t committers_erased = commit_contributor_map_.erase(type);

  DCHECK_EQ(1U, updaters_erased);
  DCHECK_EQ(1U, committers_erased);

  auto iter = connected_data_type_workers_.begin();
  while (iter != connected_data_type_workers_.end()) {
    if ((*iter)->GetDataType() == type) {
      iter = connected_data_type_workers_.erase(iter);
    } else {
      ++iter;
    }
  }
}

DataTypeSet DataTypeRegistry::GetConnectedTypes() const {
  DataTypeSet types;
  for (const std::unique_ptr<DataTypeWorker>& worker :
       connected_data_type_workers_) {
    types.Put(worker->GetDataType());
  }
  return types;
}

DataTypeSet DataTypeRegistry::GetInitialSyncEndedTypes() const {
  DataTypeSet result;
  for (const auto& [type, update_handler] : update_handler_map_) {
    if (update_handler->IsInitialSyncEnded()) {
      result.Put(type);
    }
  }
  return result;
}

const UpdateHandler* DataTypeRegistry::GetUpdateHandler(DataType type) const {
  auto it = update_handler_map_.find(type);
  return it == update_handler_map_.end() ? nullptr : it->second;
}

UpdateHandler* DataTypeRegistry::GetMutableUpdateHandler(DataType type) {
  auto it = update_handler_map_.find(type);
  return it == update_handler_map_.end() ? nullptr : it->second;
}

UpdateHandlerMap* DataTypeRegistry::update_handler_map() {
  return &update_handler_map_;
}

CommitContributorMap* DataTypeRegistry::commit_contributor_map() {
  return &commit_contributor_map_;
}

KeystoreKeysHandler* DataTypeRegistry::keystore_keys_handler() {
  return sync_encryption_handler_->GetKeystoreKeysHandler();
}

bool DataTypeRegistry::HasUnsyncedItems() const {
  // For data type workers, we ask them individually.
  for (const std::unique_ptr<DataTypeWorker>& worker :
       connected_data_type_workers_) {
    if (worker->HasLocalChanges()) {
      return true;
    }
  }

  return false;
}

const std::vector<std::unique_ptr<DataTypeWorker>>&
DataTypeRegistry::GetConnectedDataTypeWorkersForTest() const {
  return connected_data_type_workers_;
}

base::WeakPtr<DataTypeConnector> DataTypeRegistry::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DataTypeRegistry::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {}

void DataTypeRegistry::OnPassphraseAccepted() {}

void DataTypeRegistry::OnTrustedVaultKeyRequired() {}

void DataTypeRegistry::OnTrustedVaultKeyAccepted() {}

void DataTypeRegistry::OnEncryptedTypesChanged(DataTypeSet encrypted_types,
                                               bool encrypt_everything) {
  // This does NOT support disabling encryption without reconnecting the
  // type, i.e. recreating its DataTypeWorker.
  for (const std::unique_ptr<DataTypeWorker>& worker :
       connected_data_type_workers_) {
    if (encrypted_types.Has(worker->GetDataType())) {
      // No-op if the type was already encrypted.
      worker->EnableEncryption();
    }
  }
}

void DataTypeRegistry::OnCryptographerStateChanged(Cryptographer* cryptographer,
                                                   bool has_pending_keys) {
  for (const std::unique_ptr<DataTypeWorker>& worker :
       connected_data_type_workers_) {
    worker->OnCryptographerChange();
  }
}

void DataTypeRegistry::OnPassphraseTypeChanged(PassphraseType type,
                                               base::Time passphrase_time) {
  for (const std::unique_ptr<DataTypeWorker>& worker :
       connected_data_type_workers_) {
    worker->UpdatePassphraseType(type);
  }
}

}  // namespace syncer

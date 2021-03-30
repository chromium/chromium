// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/model_type_registry.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine/model_type_worker.h"
#include "components/sync/engine/nigori/cryptographer.h"
#include "components/sync/engine/nigori/keystore_keys_handler.h"

namespace syncer {

namespace {

class CommitQueueProxy : public CommitQueue {
 public:
  CommitQueueProxy(const base::WeakPtr<CommitQueue>& worker,
                   const scoped_refptr<base::SequencedTaskRunner>& sync_thread);
  ~CommitQueueProxy() override;

  void NudgeForCommit() override;

 private:
  base::WeakPtr<CommitQueue> worker_;
  scoped_refptr<base::SequencedTaskRunner> sync_thread_;
};

CommitQueueProxy::CommitQueueProxy(
    const base::WeakPtr<CommitQueue>& worker,
    const scoped_refptr<base::SequencedTaskRunner>& sync_thread)
    : worker_(worker), sync_thread_(sync_thread) {}

CommitQueueProxy::~CommitQueueProxy() {}

void CommitQueueProxy::NudgeForCommit() {
  sync_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&CommitQueue::NudgeForCommit, worker_));
}

}  // namespace

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
  DCHECK(!IsProxyType(type));
  DCHECK(update_handler_map_.find(type) == update_handler_map_.end());
  DCHECK(commit_contributor_map_.find(type) == commit_contributor_map_.end());
  DVLOG(1) << "Enabling an off-thread sync type: " << ModelTypeToString(type);

  // Save a raw pointer to the processor for connecting later.
  ModelTypeProcessor* type_processor =
      activation_response->type_processor.get();

  bool initial_sync_done =
      activation_response->model_type_state.initial_sync_done();

  auto worker = std::make_unique<ModelTypeWorker>(
      type, activation_response->model_type_state,
      /*trigger_initial_sync=*/!initial_sync_done,
      encrypted_types_.Has(type) ? sync_encryption_handler_->GetCryptographer()
                                 : nullptr,
      passphrase_type_, nudge_handler_,
      std::move(activation_response->type_processor), cancelation_signal_);

  worker->SetFallbackCryptographerForUma(
      sync_encryption_handler_->GetCryptographer());

  // Save a raw pointer and add the worker to our structures.
  ModelTypeWorker* worker_ptr = worker.get();
  connected_model_type_workers_.push_back(std::move(worker));
  update_handler_map_.insert(std::make_pair(type, worker_ptr));
  commit_contributor_map_.insert(std::make_pair(type, worker_ptr));

  // Initialize Processor -> Worker communication channel.
  type_processor->ConnectSync(std::make_unique<CommitQueueProxy>(
      worker_ptr->AsWeakPtr(), base::SequencedTaskRunnerHandle::Get()));
}

void ModelTypeRegistry::DisconnectDataType(ModelType type) {
  DVLOG(1) << "Disabling an off-thread sync type: " << ModelTypeToString(type);

  DCHECK(!IsProxyType(type));
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

void ModelTypeRegistry::ConnectProxyType(ModelType type) {
  DCHECK(IsProxyType(type));
  enabled_proxy_types_.Put(type);
}

void ModelTypeRegistry::DisconnectProxyType(ModelType type) {
  DCHECK(IsProxyType(type));
  enabled_proxy_types_.Remove(type);
}

ModelTypeSet ModelTypeRegistry::GetEnabledTypes() const {
  return Union(GetEnabledDataTypes(), enabled_proxy_types_);
}

ModelTypeSet ModelTypeRegistry::GetInitialSyncEndedTypes() const {
  ModelTypeSet result;
  for (const auto& kv : update_handler_map_) {
    if (kv.second->IsInitialSyncEnded())
      result.Put(kv.first);
  }
  return result;
}

ModelTypeSet ModelTypeRegistry::GetEnabledDataTypes() const {
  ModelTypeSet enabled_types;
  for (const auto& worker : connected_model_type_workers_) {
    enabled_types.Put(worker->GetModelType());
  }
  return enabled_types;
}

const UpdateHandler* ModelTypeRegistry::GetUpdateHandler(ModelType type) const {
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

bool ModelTypeRegistry::HasUnsyncedItems() const {
  // For model type workers, we ask them individually.
  for (const auto& worker : connected_model_type_workers_) {
    if (worker->HasLocalChangesForTest()) {
      return true;
    }
  }

  return false;
}

base::WeakPtr<ModelTypeConnector> ModelTypeRegistry::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ModelTypeRegistry::OnPassphraseRequired(
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {}

void ModelTypeRegistry::OnPassphraseAccepted() {
  for (const auto& worker : connected_model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->EncryptionAcceptedMaybeApplyUpdates();
    }
  }
}

void ModelTypeRegistry::OnTrustedVaultKeyRequired() {}

void ModelTypeRegistry::OnTrustedVaultKeyAccepted() {
  for (const auto& worker : connected_model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->EncryptionAcceptedMaybeApplyUpdates();
    }
  }
}

void ModelTypeRegistry::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {}

void ModelTypeRegistry::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                                bool encrypt_everything) {
  // TODO(skym): This does not handle reducing the number of encrypted types
  // correctly. They're removed from |encrypted_types_| but corresponding
  // workers never have their Cryptographers removed. This probably is not a use
  // case that currently needs to be supported, but it should be guarded against
  // here.
  for (const auto& worker : connected_model_type_workers_) {
    if (encrypted_types.Has(worker->GetModelType()) &&
        !encrypted_types_.Has(worker->GetModelType())) {
      worker->EnableEncryption(sync_encryption_handler_->GetCryptographer());
    }
  }
  encrypted_types_ = encrypted_types;
}

void ModelTypeRegistry::OnCryptographerStateChanged(
    Cryptographer* cryptographer,
    bool has_pending_keys) {
  for (const auto& worker : connected_model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->OnCryptographerChange();
    }
  }
}

void ModelTypeRegistry::OnPassphraseTypeChanged(PassphraseType type,
                                                base::Time passphrase_time) {
  passphrase_type_ = type;
  for (const auto& worker : connected_model_type_workers_) {
    if (encrypted_types_.Has(worker->GetModelType())) {
      worker->UpdatePassphraseType(type);
    }
  }
}

}  // namespace syncer

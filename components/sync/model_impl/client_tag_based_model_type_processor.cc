// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "components/sync/model_impl/processor_entity.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

namespace {

int CountNonTombstoneEntries(
    const std::map<ClientTagHash, std::unique_ptr<ProcessorEntity>>& entities) {
  int count = 0;
  for (const auto& kv : entities) {
    if (!kv.second->metadata().is_deleted()) {
      ++count;
    }
  }
  return count;
}

void LogNonReflectionUpdateFreshnessToUma(ModelType type,
                                          base::Time remote_modification_time) {
  const base::TimeDelta latency = base::Time::Now() - remote_modification_time;

  UMA_HISTOGRAM_CUSTOM_TIMES("Sync.NonReflectionUpdateFreshnessPossiblySkewed2",
                             latency,
                             /*min=*/base::TimeDelta::FromMilliseconds(100),
                             /*max=*/base::TimeDelta::FromDays(7),
                             /*bucket_count=*/50);

  base::UmaHistogramCustomTimes(
      std::string("Sync.NonReflectionUpdateFreshnessPossiblySkewed2.") +
          ModelTypeToHistogramSuffix(type),
      latency,
      /*min=*/base::TimeDelta::FromMilliseconds(100),
      /*max=*/base::TimeDelta::FromDays(7),
      /*bucket_count=*/50);
}

}  // namespace

ClientTagBasedModelTypeProcessor::ClientTagBasedModelTypeProcessor(
    ModelType type,
    const base::RepeatingClosure& dump_stack)
    : ClientTagBasedModelTypeProcessor(type,
                                       dump_stack,
                                       CommitOnlyTypes().Has(type)) {}

ClientTagBasedModelTypeProcessor::ClientTagBasedModelTypeProcessor(
    ModelType type,
    const base::RepeatingClosure& dump_stack,
    bool commit_only)
    : type_(type),
      bridge_(nullptr),
      dump_stack_(dump_stack),
      commit_only_(commit_only) {
  ResetState(CLEAR_METADATA);
}

ClientTagBasedModelTypeProcessor::~ClientTagBasedModelTypeProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ClientTagBasedModelTypeProcessor::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback start_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Sync is starting for " << ModelTypeToString(type_);
  DCHECK(request.error_handler) << ModelTypeToString(type_);
  DCHECK(start_callback) << ModelTypeToString(type_);
  DCHECK(!start_callback_) << ModelTypeToString(type_);
  DCHECK(!IsConnected()) << ModelTypeToString(type_);

  start_callback_ = std::move(start_callback);
  activation_request_ = request;

  // Notify the bridge sync is starting before calling the |start_callback_|
  // which in turn creates the worker.
  bridge_->OnSyncStarting(request);

  ConnectIfReady();
}

void ClientTagBasedModelTypeProcessor::OnModelStarting(
    ModelTypeSyncBridge* bridge) {
  DCHECK(bridge);
  bridge_ = bridge;
}

void ClientTagBasedModelTypeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entities_.empty());
  DCHECK(!model_ready_to_sync_);

  model_ready_to_sync_ = true;

  // The model already experienced an error; abort;
  if (model_error_)
    return;

  if (batch->GetModelTypeState().initial_sync_done()) {
    EntityMetadataMap metadata_map(batch->TakeAllMetadata());

    for (auto it = metadata_map.begin(); it != metadata_map.end(); it++) {
      std::unique_ptr<sync_pb::EntityMetadata> metadata(std::move(it->second));
      // As CreateFromMetadata() takes sync_pb::EntityMetadata by value, move it
      // to avoid copying.
      std::unique_ptr<ProcessorEntity> entity =
          ProcessorEntity::CreateFromMetadata(it->first, std::move(*metadata));
      ClientTagHash client_tag_hash =
          ClientTagHash::FromHashed(entity->metadata().client_tag_hash());
      storage_key_to_tag_hash_[entity->storage_key()] = client_tag_hash;
      entities_[client_tag_hash] = std::move(entity);
    }
    model_type_state_ = batch->GetModelTypeState();
  } else {
    // In older versions of the binary, commit-only types did not persist
    // initial_sync_done(). So this branch can be exercised for commit-only
    // types exactly once as an upgrade flow.
    // TODO(crbug.com/872360): This DCHECK can currently trigger if the user's
    // persisted Sync metadata is in an inconsistent state.
    DCHECK(commit_only_ || batch->TakeAllMetadata().empty())
        << ModelTypeToString(type_);
    // First time syncing; initialize metadata.
    model_type_state_.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type_));
  }

  ConnectIfReady();
}

bool ClientTagBasedModelTypeProcessor::IsAllowingChanges() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Changes can be handled correctly even before pending data is loaded.
  return model_ready_to_sync_;
}

void ClientTagBasedModelTypeProcessor::ConnectIfReady() {
  if (!start_callback_) {
    return;
  }
  if (model_error_) {
    activation_request_.error_handler.Run(model_error_.value());
    start_callback_.Reset();
    return;
  }
  if (!model_ready_to_sync_) {
    return;
  }

  if (!model_type_state_.has_cache_guid()) {
    // Initialize the cache_guid for old clients that didn't persist it.
    model_type_state_.set_cache_guid(activation_request_.cache_guid);
  }
  // Check for invalid persisted metadata.
  if (model_type_state_.cache_guid() != activation_request_.cache_guid ||
      model_type_state_.progress_marker().data_type_id() !=
          GetSpecificsFieldNumberFromModelType(type_)) {
    // There is a mismatch between the cache guid or the data type id stored in
    // |model_type_state_| and the one received from sync. This indicates that
    // the stored metadata are invalid (e.g. has been manipulated) and don't
    // belong to the current syncing client.
    if (model_type_state_.progress_marker().data_type_id() !=
        GetSpecificsFieldNumberFromModelType(type_)) {
      UMA_HISTOGRAM_ENUMERATION("Sync.PersistedModelTypeIdMismatch",
                                ModelTypeHistogramValue(type_));
    }
    ClearMetadataAndResetState();

    // The model is still ready to sync (with the same |bridge_|) - replay
    // the initialization.
    model_ready_to_sync_ = true;
    // Notify the bridge sync is starting to simulate an enable event.
    bridge_->OnSyncStarting(activation_request_);
  }

  // Cache GUID verification earlier above guarantees the user is the same.
  // TODO(https://crbug.com/959157): Use CoreAccountId instead of std::string.
  model_type_state_.set_authenticated_account_id(
      activation_request_.authenticated_account_id.id);

  // For commit-only types, no updates are expected and hence we can consider
  // initial_sync_done(), reflecting that sync is enabled.
  if (commit_only_ && !model_type_state_.initial_sync_done()) {
    sync_pb::ModelTypeState model_type_state = model_type_state_;
    model_type_state.set_initial_sync_done(true);
    OnFullUpdateReceived(model_type_state, UpdateResponseDataList());
    DCHECK(IsTrackingMetadata());
  }

  auto activation_response = std::make_unique<DataTypeActivationResponse>();
  activation_response->model_type_state = model_type_state_;
  activation_response->type_processor =
      std::make_unique<ModelTypeProcessorProxy>(
          weak_ptr_factory_for_worker_.GetWeakPtr(),
          base::SequencedTaskRunnerHandle::Get());
  std::move(start_callback_).Run(std::move(activation_response));
}

bool ClientTagBasedModelTypeProcessor::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!worker_;
}

void ClientTagBasedModelTypeProcessor::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disabling sync for a type never happens before the model is ready to sync.
  DCHECK(model_ready_to_sync_);
  DCHECK(!start_callback_);

  switch (metadata_fate) {
    case KEEP_METADATA: {
      bridge_->ApplyStopSyncChanges(
          /*delete_metadata_change_list=*/nullptr);
      // The model is still ready to sync (with the same |bridge_|) and same
      // sync metadata.
      ResetState(KEEP_METADATA);
      DCHECK(model_ready_to_sync_);
      break;
    }

    case CLEAR_METADATA: {
      ClearMetadataAndResetState();
      // The model is still ready to sync (with the same |bridge_|) - replay
      // the initialization.
      ModelReadyToSync(std::make_unique<MetadataBatch>());
      DCHECK(model_ready_to_sync_);
      break;
    }
  }

  DCHECK(!IsConnected());
}

void ClientTagBasedModelTypeProcessor::ClearMetadataAndResetState() {
  std::unique_ptr<MetadataChangeList> change_list;

  // Clear metadata if MergeSyncData() was called before.
  if (model_type_state_.initial_sync_done()) {
    change_list = bridge_->CreateMetadataChangeList();
    for (const auto& kv : entities_) {
      change_list->ClearMetadata(kv.second->storage_key());
    }
    change_list->ClearModelTypeState();
  } else {
    // All changes before the initial sync is done are ignored and in fact they
    // were never persisted by the bridge (prior to MergeSyncData), so we should
    // be tracking no entities.
    DCHECK(entities_.empty());
  }

  bridge_->ApplyStopSyncChanges(std::move(change_list));

  // Reset all the internal state of the processor.
  ResetState(CLEAR_METADATA);
}

bool ClientTagBasedModelTypeProcessor::IsTrackingMetadata() {
  return model_type_state_.initial_sync_done();
}

std::string ClientTagBasedModelTypeProcessor::TrackedAccountId() {
  // Returning non-empty here despite !IsTrackingMetadata() has weird semantics,
  // e.g. initial updates are being fetched but we haven't received the response
  // (i.e. prior to exercising MergeSyncData()). Let's be cautious and hide the
  // account ID.
  if (!IsTrackingMetadata()) {
    return "";
  }
  return model_type_state_.authenticated_account_id();
}

std::string ClientTagBasedModelTypeProcessor::TrackedCacheGuid() {
  // Returning non-empty here despite !IsTrackingMetadata() has weird semantics,
  // e.g. initial updates are being fetched but we haven't received the response
  // (i.e. prior to exercising MergeSyncData()). Let's be cautious and hide the
  // cache GUID.
  if (!IsTrackingMetadata()) {
    return "";
  }
  return model_type_state_.cache_guid();
}

void ClientTagBasedModelTypeProcessor::ReportError(const ModelError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore all errors after the first.
  if (model_error_) {
    return;
  }

  model_error_ = error;

  if (dump_stack_) {
    // Upload a stack trace if possible.
    dump_stack_.Run();
  }

  if (IsConnected()) {
    DisconnectSync();
  }

  // Shouldn't connect anymore.
  start_callback_.Reset();
  if (activation_request_.error_handler) {
    // Tell sync about the error.
    activation_request_.error_handler.Run(error);
  }
  // If the error handler isn't ready yet, we defer reporting the error until it
  // becomes available which happens in ConnectIfReady() upon OnSyncStarting().
}

base::Optional<ModelError> ClientTagBasedModelTypeProcessor::GetError() const {
  return model_error_;
}

base::WeakPtr<ModelTypeControllerDelegate>
ClientTagBasedModelTypeProcessor::GetControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

void ClientTagBasedModelTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Successfully connected " << ModelTypeToString(type_);

  worker_ = std::move(worker);

  NudgeForCommitIfNeeded();
}

void ClientTagBasedModelTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsConnected());

  DVLOG(1) << "Disconnecting sync for " << ModelTypeToString(type_);
  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  worker_.reset();

  for (const auto& kv : entities_) {
    kv.second->ClearTransientSyncState();
  }
}

void ClientTagBasedModelTypeProcessor::Put(
    const std::string& storage_key,
    std::unique_ptr<EntityData> data,
    MetadataChangeList* metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAllowingChanges());
  DCHECK(data);
  DCHECK(!data->is_deleted());
  DCHECK(!data->name.empty());
  DCHECK(!data->specifics.has_encrypted());
  DCHECK_EQ(type_, GetModelTypeFromSpecifics(data->specifics));

  if (!model_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // The bridge is creating a new entity. The bridge may or may not populate
    // |data->client_tag_hash|, so let's ask for the client tag if needed.
    if (data->client_tag_hash.value().empty()) {
      data->client_tag_hash = GetClientTagHash(storage_key, *data);
    } else if (bridge_->SupportsGetClientTag()) {
      // If the Put() call already included the client tag, let's verify that
      // it's consistent with the bridge's regular GetClientTag() function (if
      // supported by the bridge).
      DCHECK_EQ(
          data->client_tag_hash,
          ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(*data)));
    }
    // If another entity exists for the same client_tag_hash, it could be the
    // case that the bridge has deleted this entity but the tombstone hasn't
    // been sent to the server yet, and the bridge is trying to re-create this
    // entity with a new storage key. In such case, we should reuse the existing
    // entity.
    entity = GetEntityForTagHash(data->client_tag_hash);
    if (entity != nullptr) {
      DCHECK(storage_key != entity->storage_key());
      DCHECK(entity->metadata().is_deleted());
      // Remove the old storage key from the processor, the entity, and the
      // corresponding metadata record.
      storage_key_to_tag_hash_.erase(entity->storage_key());
      metadata_change_list->ClearMetadata(entity->storage_key());
      entity->ClearStorageKey();
      // Populate the new storage key in the existing entity.
      entity->SetStorageKey(storage_key);
      storage_key_to_tag_hash_[storage_key] = data->client_tag_hash;
    } else {
      if (data->creation_time.is_null())
        data->creation_time = base::Time::Now();
      if (data->modification_time.is_null())
        data->modification_time = data->creation_time;
      entity = CreateEntity(storage_key, *data);
    }
  } else if (entity->MatchesData(*data)) {
    // Ignore changes that don't actually change anything.
    UMA_HISTOGRAM_ENUMERATION("Sync.ModelTypeRedundantPut",
                              ModelTypeHistogramValue(type_));
    return;
  }

  entity->MakeLocalChange(std::move(data));
  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());

  NudgeForCommitIfNeeded();
}

void ClientTagBasedModelTypeProcessor::Delete(
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAllowingChanges());

  if (!model_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // Missing is as good as deleted as far as the model is concerned.
    return;
  }

  if (entity->Delete())
    metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
  else
    RemoveEntity(entity, metadata_change_list);

  NudgeForCommitIfNeeded();
}

void ClientTagBasedModelTypeProcessor::UpdateStorageKey(
    const EntityData& entity_data,
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  const ClientTagHash& client_tag_hash = entity_data.client_tag_hash;
  DCHECK(!client_tag_hash.value().empty());
  DCHECK(!storage_key.empty());
  DCHECK(!bridge_->SupportsGetStorageKey());
  DCHECK(model_type_state_.initial_sync_done());

  ProcessorEntity* entity = GetEntityForTagHash(client_tag_hash);
  DCHECK(entity);

  DCHECK(entity->storage_key().empty());
  DCHECK(storage_key_to_tag_hash_.find(storage_key) ==
         storage_key_to_tag_hash_.end());

  storage_key_to_tag_hash_[storage_key] = client_tag_hash;
  entity->SetStorageKey(storage_key);
  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
}

void ClientTagBasedModelTypeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {
  DCHECK(model_type_state_.initial_sync_done());

  // Look-up the client tag hash.
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  if (iter == storage_key_to_tag_hash_.end()) {
    // Missing is as good as untracked as far as the model is concerned.
    return;
  }

  entities_.erase(iter->second);
  storage_key_to_tag_hash_.erase(iter);
}

void ClientTagBasedModelTypeProcessor::UntrackEntityForClientTagHash(
    const ClientTagHash& client_tag_hash) {
  DCHECK(model_type_state_.initial_sync_done());
  DCHECK(!client_tag_hash.value().empty());
  // Is a no-op if no entity for |client_tag_hash| is tracked.
  DCHECK(GetEntityForTagHash(client_tag_hash) == nullptr ||
         GetEntityForTagHash(client_tag_hash)->storage_key().empty());
  entities_.erase(client_tag_hash);
}

bool ClientTagBasedModelTypeProcessor::IsEntityUnsynced(
    const std::string& storage_key) {
  ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    return false;
  }

  return entity->IsUnsynced();
}

base::Time ClientTagBasedModelTypeProcessor::GetEntityCreationTime(
    const std::string& storage_key) const {
  const ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    return base::Time();
  }
  return ProtoTimeToTime(entity->metadata().creation_time());
}

base::Time ClientTagBasedModelTypeProcessor::GetEntityModificationTime(
    const std::string& storage_key) const {
  const ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    return base::Time();
  }
  return ProtoTimeToTime(entity->metadata().modification_time());
}

void ClientTagBasedModelTypeProcessor::NudgeForCommitIfNeeded() {
  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected())
    return;

  // Don't send anything if the type is not ready to handle commits.
  if (!model_type_state_.initial_sync_done())
    return;

  // Nudge worker if there are any entities with local changes.0
  if (HasLocalChanges())
    worker_->NudgeForCommit();
}

bool ClientTagBasedModelTypeProcessor::HasLocalChanges() const {
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->RequiresCommitRequest()) {
      return true;
    }
  }
  return false;
}

void ClientTagBasedModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(max_entries, 0U);
  // If there is a model error, it must have been reported already but hasn't
  // reached the sync engine yet. In this case return directly to avoid
  // interactions with the bridge.
  if (model_error_) {
    std::move(callback).Run(CommitRequestDataList());
    return;
  }

  std::vector<std::string> entities_requiring_data;
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->RequiresCommitData()) {
      entities_requiring_data.push_back(entity->storage_key());
    }
  }
  if (!entities_requiring_data.empty()) {
    // Make a copy for the callback so that we can check if everything was
    // loaded successfully.
    std::unordered_set<std::string> storage_keys_to_load(
        entities_requiring_data.begin(), entities_requiring_data.end());
    bridge_->GetData(
        std::move(entities_requiring_data),
        base::BindOnce(&ClientTagBasedModelTypeProcessor::OnPendingDataLoaded,
                       weak_ptr_factory_for_worker_.GetWeakPtr(), max_entries,
                       std::move(callback), std::move(storage_keys_to_load)));
  } else {
    // All commit data can be available in memory for those entries passed in
    // the .put() method.
    CommitLocalChanges(max_entries, std::move(callback));
  }
}

void ClientTagBasedModelTypeProcessor::OnCommitCompleted(
    const sync_pb::ModelTypeState& model_type_state,
    const CommitResponseDataList& response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!model_error_);

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      bridge_->CreateMetadataChangeList();
  EntityChangeList entity_change_list;

  model_type_state_ = model_type_state;
  metadata_change_list->UpdateModelTypeState(model_type_state_);

  for (const CommitResponseData& data : response_list) {
    ProcessorEntity* entity = GetEntityForTagHash(data.client_tag_hash);
    if (entity == nullptr) {
      NOTREACHED() << "Received commit response for missing item."
                   << " type: " << ModelTypeToString(type_)
                   << " client_tag_hash: " << data.client_tag_hash;
      continue;
    }

    entity->ReceiveCommitResponse(data, commit_only_, type_);

    if (commit_only_) {
      if (!entity->IsUnsynced()) {
        entity_change_list.push_back(
            EntityChange::CreateDelete(entity->storage_key()));
        RemoveEntity(entity, metadata_change_list.get());
      }
      // If unsynced, we could theoretically update persisted metadata to have
      // more accurate bookkeeping. However, this wouldn't actually do anything
      // useful, we still need to commit again, and we're not going to include
      // any of the changing metadata in the commit message. So skip updating
      // metadata.
    } else if (entity->CanClearMetadata()) {
      RemoveEntity(entity, metadata_change_list.get());
    } else {
      metadata_change_list->UpdateMetadata(entity->storage_key(),
                                           entity->metadata());
    }
  }

  // Entities not mentioned in response_list weren't committed. We should reset
  // their commit_requested_sequence_number so they are committed again on next
  // sync cycle.
  // TODO(crbug.com/740757): Iterating over all entities is inefficient. It is
  // better to remember in GetLocalChanges which entities are being committed
  // and adjust only them. Alternatively we can make worker return commit status
  // for all entities, not just successful ones and use that to lookup entities
  // to clear.
  for (auto& entity_kv : entities_) {
    entity_kv.second->ClearTransientSyncState();
  }

  base::Optional<ModelError> error = bridge_->ApplySyncChanges(
      std::move(metadata_change_list), std::move(entity_change_list));
  if (error) {
    ReportError(*error);
  }
}

// Populates the client tag hashes for every update entity in |updates|.
void PopulateClientTagsForWalletData(const ModelType& type,
                                     ModelTypeSyncBridge* bridge,
                                     UpdateResponseDataList* updates) {
  DCHECK(bridge->SupportsGetClientTag());
  UpdateResponseDataList updates_with_client_tags;
  for (std::unique_ptr<UpdateResponseData>& update : *updates) {
    DCHECK(update);
    if (update->entity->parent_id == "0") {
      // Ignore the permanent root node. Other places in this file detect them
      // by having empty client tags; this cannot be used for wallet_data as no
      // wallet_data entity has a client tag.
      continue;
    }
    update->entity->client_tag_hash = ClientTagHash::FromUnhashed(
        type, bridge->GetClientTag(*update->entity));
  }
}

// Returns whether the state has a version_watermark based GC directive, which
// tells us to clear all sync data that's stored locally.
bool HasClearAllDirective(const sync_pb::ModelTypeState& model_type_state) {
  return model_type_state.progress_marker()
      .gc_directive()
      .has_version_watermark();
}

void ClientTagBasedModelTypeProcessor::OnUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    UpdateResponseDataList updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_ready_to_sync_);
  DCHECK(!model_error_);

  if (!ValidateUpdate(model_type_state, updates)) {
    return;
  }

  if (type_ == AUTOFILL_WALLET_DATA) {
    // The client tag based processor requires client tags to function properly.
    // However, the wallet data type does not have any client tags. This hacky
    // code manually asks the bridge to create the client tags for each update,
    // so that we can still use this processor. A proper fix would be to either
    // fully use client tags, or to use a different processor.
    // TODO(crbug.com/874001): Remove this feature-specific logic when the right
    // solution for Wallet data has been decided.
    PopulateClientTagsForWalletData(type_, bridge_, &updates);
  }

  base::Optional<ModelError> error;

  // We call OnFullUpdateReceived when it's the first sync cycle, or when
  // we get a garbage collection directive from the server telling us to clear
  // all data by version watermark.
  // This means that if we receive a version watermark based GC directive, we
  // always clear all data. We do this to allow the server to replace all data
  // on the client, without having to know exactly which entities the client
  // has.
  bool is_initial_sync = !model_type_state_.initial_sync_done();
  if (is_initial_sync || HasClearAllDirective(model_type_state)) {
    error = OnFullUpdateReceived(model_type_state, std::move(updates));
  } else {
    error = OnIncrementalUpdateReceived(model_type_state, std::move(updates));
  }

  if (error) {
    ReportError(*error);
    return;
  }

  if (is_initial_sync) {
    base::TimeDelta configuration_duration =
        base::Time::Now() - activation_request_.configuration_start_time;
    base::UmaHistogramCustomTimes(
        base::StringPrintf(
            "Sync.ModelTypeConfigurationTime.%s.%s",
            (activation_request_.sync_mode == SyncMode::kTransportOnly)
                ? "Ephemeral"
                : "Persistent",
            ModelTypeToHistogramSuffix(type_)),
        configuration_duration,
        /*min=*/base::TimeDelta::FromMilliseconds(1),
        /*min=*/base::TimeDelta::FromSeconds(60),
        /*buckets=*/50);
  }

  // If there were entities with empty storage keys, they should have been
  // updated by bridge as part of ApplySyncChanges.
  DCHECK(AllStorageKeysPopulated());
  // There may be new reasons to commit by the time this function is done.
  NudgeForCommitIfNeeded();
}

ProcessorEntity* ClientTagBasedModelTypeProcessor::ProcessUpdate(
    std::unique_ptr<UpdateResponseData> update,
    EntityChangeList* entity_changes,
    std::string* storage_key_to_clear) {
  const EntityData& data = *update->entity;
  const ClientTagHash& client_tag_hash = data.client_tag_hash;

  // Filter out updates without a client tag hash (including permanent nodes,
  // which have server tags instead).
  if (client_tag_hash.value().empty()) {
    return nullptr;
  }

  // Filter out unexpected client tag hashes.
  if (!data.is_deleted() && bridge_->SupportsGetClientTag() &&
      client_tag_hash !=
          ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(data))) {
    DLOG(WARNING) << "Received unexpected client tag hash: " << client_tag_hash
                  << " for " << ModelTypeToString(type_);
    return nullptr;
  }

  ProcessorEntity* entity = GetEntityForTagHash(client_tag_hash);

  // Handle corner cases first.
  if (entity == nullptr && data.is_deleted()) {
    // Local entity doesn't exist and update is tombstone.
    DLOG(WARNING) << "Received remote delete for a non-existing item."
                  << " client_tag_hash: " << client_tag_hash << " for "
                  << ModelTypeToString(type_);
    return nullptr;
  }

  if (entity) {
    entity->RecordEntityUpdateLatency(update->response_version, type_);
  }

  if (entity && entity->UpdateIsReflection(update->response_version)) {
    // Seen this update before; just ignore it.
    return nullptr;
  }

  // Cache update encryption key name in case |update| will be moved away into
  // ResolveConflict().
  const std::string update_encryption_key_name = update->encryption_key_name;
  ConflictResolution resolution_type = ConflictResolution::kTypeSize;
  if (entity && entity->IsUnsynced()) {
    // Handle conflict resolution.
    resolution_type = ResolveConflict(std::move(update), entity, entity_changes,
                                      storage_key_to_clear);
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict", resolution_type,
                              ConflictResolution::kTypeSize);
  } else {
    // Handle simple create/delete/update.
    base::Optional<EntityChange::ChangeType> change_type;

    if (entity == nullptr) {
      entity = CreateEntity(data);
      change_type = EntityChange::ACTION_ADD;
    } else if (data.is_deleted()) {
      DCHECK(!entity->metadata().is_deleted());
      change_type = EntityChange::ACTION_DELETE;
    } else if (!entity->MatchesData(data)) {
      change_type = EntityChange::ACTION_UPDATE;
    }
    entity->RecordAcceptedUpdate(*update);
    // Inform the bridge about the changes if needed.
    if (change_type) {
      switch (change_type.value()) {
        case EntityChange::ACTION_ADD:
          entity_changes->push_back(EntityChange::CreateAdd(
              entity->storage_key(), std::move(update->entity)));
          break;
        case EntityChange::ACTION_DELETE:
          // The entity was deleted; inform the bridge. Note that the local data
          // can never be deleted at this point because it would have either
          // been acked (the add case) or pending (the conflict case).
          entity_changes->push_back(
              EntityChange::CreateDelete(entity->storage_key()));
          break;
        case EntityChange::ACTION_UPDATE:
          // Specifics have changed, so update the bridge.
          entity_changes->push_back(EntityChange::CreateUpdate(
              entity->storage_key(), std::move(update->entity)));
          break;
      }
    }
  }

  // If the received entity has out of date encryption, we schedule another
  // commit to fix it.
  if (model_type_state_.encryption_key_name() != update_encryption_key_name) {
    DVLOG(2) << ModelTypeToString(type_) << ": Requesting re-encrypt commit "
             << update_encryption_key_name << " -> "
             << model_type_state_.encryption_key_name();

    entity->IncrementSequenceNumber(base::Time::Now());
  }
  return entity;
}

ConflictResolution ClientTagBasedModelTypeProcessor::ResolveConflict(
    std::unique_ptr<UpdateResponseData> update,
    ProcessorEntity* entity,
    EntityChangeList* changes,
    std::string* storage_key_to_clear) {
  const EntityData& remote_data = *update->entity;

  ConflictResolution resolution_type = ConflictResolution::kTypeSize;

  // Determine the type of resolution.
  if (entity->MatchesData(remote_data)) {
    // The changes are identical so there isn't a real conflict.
    resolution_type = ConflictResolution::kChangesMatch;
  } else if (entity->metadata().is_deleted()) {
    // Local tombstone vs remote update (non-deletion). Should be undeleted.
    resolution_type = ConflictResolution::kUseRemote;
  } else if (entity->MatchesOwnBaseData()) {
    // If there is no real local change, then the entity must be unsynced due to
    // a pending local re-encryption request. In this case, the remote data
    // should win.
    resolution_type = ConflictResolution::kIgnoreLocalEncryption;
  } else if (entity->MatchesBaseData(remote_data)) {
    // The remote data isn't actually changing from the last remote data that
    // was seen, so it must have been a re-encryption and can be ignored.
    resolution_type = ConflictResolution::kIgnoreRemoteEncryption;
  } else {
    // There's a real data conflict here; let the bridge resolve it.
    resolution_type =
        bridge_->ResolveConflict(entity->storage_key(), remote_data);
  }

  // Apply the resolution.
  switch (resolution_type) {
    case ConflictResolution::kChangesMatch:
      // Record the update and squash the pending commit.
      entity->RecordForcedUpdate(*update);
      break;
    case ConflictResolution::kUseLocal:
    case ConflictResolution::kIgnoreRemoteEncryption:
      // Record that we received the update from the server but leave the
      // pending commit intact.
      entity->RecordIgnoredUpdate(*update);
      break;
    case ConflictResolution::kUseRemote:
    case ConflictResolution::kIgnoreLocalEncryption:
      // Update client data to match server.
      if (update->entity->is_deleted()) {
        DCHECK(!entity->metadata().is_deleted());
        // Squash the pending commit.
        entity->RecordForcedUpdate(*update);
        changes->push_back(EntityChange::CreateDelete(entity->storage_key()));
      } else if (!entity->metadata().is_deleted()) {
        // Squash the pending commit.
        entity->RecordForcedUpdate(*update);
        changes->push_back(EntityChange::CreateUpdate(
            entity->storage_key(), std::move(update->entity)));
      } else {
        // Remote undeletion. This could imply a new storage key for some
        // bridges, so we may need to wait until UpdateStorageKey() is called.
        if (!bridge_->SupportsGetStorageKey()) {
          *storage_key_to_clear = entity->storage_key();
          entity->ClearStorageKey();
        }
        // Squash the pending commit.
        entity->RecordForcedUpdate(*update);
        changes->push_back(EntityChange::CreateAdd(entity->storage_key(),
                                                   std::move(update->entity)));
      }
      break;
    case ConflictResolution::kUseNewDEPRECATED:
    case ConflictResolution::kTypeSize:
      NOTREACHED();
      break;
  }

  return resolution_type;
}

void ClientTagBasedModelTypeProcessor::RecommitAllForEncryption(
    const std::unordered_set<std::string>& already_updated,
    MetadataChangeList* metadata_changes) {
  ModelTypeSyncBridge::StorageKeyList entities_needing_data;

  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->storage_key().empty() ||
        (already_updated.find(entity->storage_key()) !=
         already_updated.end())) {
      // Entities with empty storage key were already processed. ProcessUpdate()
      // incremented their sequence numbers and cached commit data. Their
      // metadata will be persisted in UpdateStorageKey().
      continue;
    }
    entity->IncrementSequenceNumber(base::Time::Now());
    if (entity->RequiresCommitData()) {
      entities_needing_data.push_back(entity->storage_key());
    }
    metadata_changes->UpdateMetadata(entity->storage_key(), entity->metadata());
  }
}

bool ClientTagBasedModelTypeProcessor::ValidateUpdate(
    const sync_pb::ModelTypeState& model_type_state,
    const UpdateResponseDataList& updates) {
  if (!model_type_state_.initial_sync_done()) {
    // Due to uss_migrator, initial sync (when migrating from non-USS) does not
    // contain any gc directives. Thus, we cannot expect the conditions below to
    // be satisfied. It is okay to skip the check as for an initial sync, the gc
    // directive does not make any semantical difference.
    return true;
  }

  if (HasClearAllDirective(model_type_state) &&
      bridge_->SupportsIncrementalUpdates()) {
    ReportError(ModelError(FROM_HERE,
                           "Received an update with version watermark for "
                           "bridge that supports incremental updates"));

    return false;
  } else if (!HasClearAllDirective(model_type_state) &&
             !bridge_->SupportsIncrementalUpdates() && !updates.empty()) {
    // We receive an update without clear all directive from the server to
    // indicate no data has changed. This contradicts with the list of updates
    // being non-empty, the bridge cannot handle it and we need to fail here.
    // (If the last condition does not hold true and the list of updates is
    // empty, we still need to pass the empty update to the bridge because the
    // progress marker might have changed.)
    ReportError(ModelError(FROM_HERE,
                           "Received a non-empty update without version "
                           "watermark for bridge that does not support "
                           "incremental updates"));
    return false;
  }
  return true;
}

base::Optional<ModelError>
ClientTagBasedModelTypeProcessor::OnFullUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    UpdateResponseDataList updates) {
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  DCHECK(model_ready_to_sync_);

  // Check that the worker correctly marked initial sync as done
  // for this update.
  DCHECK(model_type_state.initial_sync_done());

  if (HasClearAllDirective(model_type_state)) {
    ExpireAllEntries(metadata_changes.get());
  } else {
    // Ensure that this is the initial sync, and it was not already marked done.
    DCHECK(!model_type_state_.initial_sync_done());
  }

  // Given that we either just removed all existing sync entities (in the full
  // update case), or we just started sync for the first time, we should not
  // have any entities here.
  DCHECK(entities_.empty());

  EntityChangeList entity_data;

  model_type_state_ = model_type_state;
  metadata_changes->UpdateModelTypeState(model_type_state_);

  for (const std::unique_ptr<syncer::UpdateResponseData>& update : updates) {
    DCHECK(update);
    const ClientTagHash& client_tag_hash = update->entity->client_tag_hash;
    if (client_tag_hash.value().empty()) {
      // Ignore updates missing a client tag hash (e.g. permanent nodes).
      continue;
    }
    if (update->entity->is_deleted()) {
      DLOG(WARNING) << "Ignoring tombstone found during initial update: "
                    << "client_tag_hash = " << client_tag_hash << " for "
                    << ModelTypeToString(type_);
      continue;
    }
    if (bridge_->SupportsGetClientTag() &&
        client_tag_hash != ClientTagHash::FromUnhashed(
                               type_, bridge_->GetClientTag(*update->entity))) {
      DLOG(WARNING) << "Received unexpected client tag hash: "
                    << client_tag_hash << " for " << ModelTypeToString(type_);
      continue;
    }

#if DCHECK_IS_ON()
    // TODO(crbug.com/872360): The CreateEntity() call below assumes that no
    // entity with this client_tag_hash exists already, but in some cases it
    // does.
    if (entities_.find(client_tag_hash) != entities_.end()) {
      DLOG(ERROR) << "Received duplicate client_tag_hash " << client_tag_hash
                  << " for " << ModelTypeToString(type_);
    }
#endif  // DCHECK_IS_ON()
    ProcessorEntity* entity = CreateEntity(*update->entity);
    entity->RecordAcceptedUpdate(*update);
    const std::string& storage_key = entity->storage_key();
    entity_data.push_back(
        EntityChange::CreateAdd(storage_key, std::move(update->entity)));
    if (!storage_key.empty())
      metadata_changes->UpdateMetadata(storage_key, entity->metadata());
  }

  // Let the bridge handle associating and merging the data.
  base::Optional<ModelError> error = bridge_->MergeSyncData(
      std::move(metadata_changes), std::move(entity_data));
  return error;
}

base::Optional<ModelError>
ClientTagBasedModelTypeProcessor::OnIncrementalUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    UpdateResponseDataList updates) {
  DCHECK(model_ready_to_sync_);
  DCHECK(model_type_state.initial_sync_done());

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  EntityChangeList entity_changes;

  metadata_changes->UpdateModelTypeState(model_type_state);
  bool got_new_encryption_requirements =
      model_type_state_.encryption_key_name() !=
      model_type_state.encryption_key_name();
  model_type_state_ = model_type_state;

  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> already_updated;

  for (std::unique_ptr<syncer::UpdateResponseData>& update : updates) {
    DCHECK(update);
    std::string storage_key_to_clear;
    ProcessorEntity* entity = ProcessUpdate(std::move(update), &entity_changes,
                                            &storage_key_to_clear);

    if (!entity) {
      // The update is either of the following:
      // 1. Tombstone of entity that didn't exist locally.
      // 2. Reflection, thus should be ignored.
      // 3. Update without a client tag hash (including permanent nodes, which
      // have server tags instead).
      continue;
    }

    LogNonReflectionUpdateFreshnessToUma(
        type_,
        /*remote_modification_time=*/
        ProtoTimeToTime(entity->metadata().modification_time()));

    if (entity->storage_key().empty()) {
      // Storage key of this entity is not known yet. Don't update metadata, it
      // will be done from UpdateStorageKey.

      // If this is the result of a conflict resolution (where a remote
      // undeletion was preferred), then need to clear a metadata entry from
      // the database.
      if (!storage_key_to_clear.empty()) {
        metadata_changes->ClearMetadata(storage_key_to_clear);
        storage_key_to_tag_hash_.erase(storage_key_to_clear);
      }
      continue;
    }

    DCHECK(storage_key_to_clear.empty());

    if (entity->CanClearMetadata()) {
      metadata_changes->ClearMetadata(entity->storage_key());
      storage_key_to_tag_hash_.erase(entity->storage_key());
      entities_.erase(
          ClientTagHash::FromHashed(entity->metadata().client_tag_hash()));
    } else {
      metadata_changes->UpdateMetadata(entity->storage_key(),
                                       entity->metadata());
    }

    if (got_new_encryption_requirements) {
      already_updated.insert(entity->storage_key());
    }
  }

  if (got_new_encryption_requirements) {
    // TODO(pavely): Currently we recommit all entities. We should instead
    // recommit only the ones whose encryption key doesn't match the one in
    // DataTypeState. Work is tracked in http://crbug.com/727874.
    RecommitAllForEncryption(already_updated, metadata_changes.get());
  }
  // Inform the bridge of the new or updated data.
  return bridge_->ApplySyncChanges(std::move(metadata_changes),
                                   std::move(entity_changes));
}

void ClientTagBasedModelTypeProcessor::OnPendingDataLoaded(
    size_t max_entries,
    GetLocalChangesCallback callback,
    std::unordered_set<std::string> storage_keys_to_load,
    std::unique_ptr<DataBatch> data_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The model already experienced an error; abort;
  if (model_error_)
    return;

  ConsumeDataBatch(std::move(storage_keys_to_load), std::move(data_batch));

  ConnectIfReady();
  CommitLocalChanges(max_entries, std::move(callback));
}

void ClientTagBasedModelTypeProcessor::ConsumeDataBatch(
    std::unordered_set<std::string> storage_keys_to_load,
    std::unique_ptr<DataBatch> data_batch) {
  while (data_batch->HasNext()) {
    KeyAndData data = data_batch->Next();
    const std::string& storage_key = data.first;

    storage_keys_to_load.erase(storage_key);
    ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
    // If the entity wasn't deleted or updated with new commit.
    if (entity != nullptr && entity->RequiresCommitData()) {
      // SetCommitData will update EntityData's fields with values from
      // metadata.
      entity->SetCommitData(std::move(data.second));
    }
  }

  // Detect failed loads that shouldn't have failed.
  std::vector<std::string> storage_keys_to_untrack;
  for (const std::string& storage_key : storage_keys_to_load) {
    ProcessorEntity* entity = GetEntityForStorageKey(storage_key);
    if (entity == nullptr || entity->metadata().is_deleted()) {
      // Skip entities that are not tracked any more or already marked for
      // deletion.
      continue;
    }
    // This scenario indicates a bug in the bridge, which didn't properly
    // propagate a local deletion to the processor, either in the form of
    // Delete() or UntrackEntity(). As a workaround to avoid negative side
    // effects of this inconsistent state, we treat it as if UntrackEntity()
    // had been called.
    storage_keys_to_untrack.push_back(storage_key);
    UMA_HISTOGRAM_ENUMERATION("Sync.ModelTypeOrphanMetadata",
                              ModelTypeHistogramValue(type_));
  }

  if (storage_keys_to_untrack.empty()) {
    return;
  }

  DCHECK(model_ready_to_sync_);
  DCHECK(IsTrackingMetadata());

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();

  for (const std::string& storage_key : storage_keys_to_untrack) {
    UntrackEntityForStorageKey(storage_key);
    metadata_changes->ClearMetadata(storage_key);
  }

  bridge_->ApplySyncChanges(std::move(metadata_changes), EntityChangeList());
}

void ClientTagBasedModelTypeProcessor::CommitLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK(!model_error_);
  CommitRequestDataList commit_requests;
  // TODO(rlarocque): Do something smarter than iterate here.
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->RequiresCommitRequest() && !entity->RequiresCommitData()) {
      auto request = std::make_unique<CommitRequestData>();
      entity->InitializeCommitRequestData(request.get());
      commit_requests.push_back(std::move(request));
      if (commit_requests.size() >= max_entries) {
        break;
      }
    }
  }
  std::move(callback).Run(std::move(commit_requests));
}

ClientTagHash ClientTagBasedModelTypeProcessor::GetClientTagHash(
    const std::string& storage_key,
    const EntityData& data) const {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  DCHECK(bridge_->SupportsGetClientTag());
  return iter == storage_key_to_tag_hash_.end()
             ? ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(data))
             : iter->second;
}

ProcessorEntity* ClientTagBasedModelTypeProcessor::GetEntityForStorageKey(
    const std::string& storage_key) {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  return iter == storage_key_to_tag_hash_.end()
             ? nullptr
             : GetEntityForTagHash(iter->second);
}

const ProcessorEntity* ClientTagBasedModelTypeProcessor::GetEntityForStorageKey(
    const std::string& storage_key) const {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  return iter == storage_key_to_tag_hash_.end()
             ? nullptr
             : GetEntityForTagHash(iter->second);
}

ProcessorEntity* ClientTagBasedModelTypeProcessor::GetEntityForTagHash(
    const ClientTagHash& tag_hash) {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

const ProcessorEntity* ClientTagBasedModelTypeProcessor::GetEntityForTagHash(
    const ClientTagHash& tag_hash) const {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

ProcessorEntity* ClientTagBasedModelTypeProcessor::CreateEntity(
    const std::string& storage_key,
    const EntityData& data) {
  DCHECK(!data.client_tag_hash.value().empty());
  DCHECK(entities_.find(data.client_tag_hash) == entities_.end());
  DCHECK(!bridge_->SupportsGetStorageKey() || !storage_key.empty());
  DCHECK(storage_key.empty() || storage_key_to_tag_hash_.find(storage_key) ==
                                    storage_key_to_tag_hash_.end());
  std::unique_ptr<ProcessorEntity> entity = ProcessorEntity::CreateNew(
      storage_key, data.client_tag_hash, data.id, data.creation_time);
  ProcessorEntity* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  if (!storage_key.empty())
    storage_key_to_tag_hash_[storage_key] = data.client_tag_hash;
  return entity_ptr;
}

ProcessorEntity* ClientTagBasedModelTypeProcessor::CreateEntity(
    const EntityData& data) {
  if (bridge_->SupportsGetClientTag()) {
    DCHECK_EQ(data.client_tag_hash,
              ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(data)));
  }
  std::string storage_key;
  if (bridge_->SupportsGetStorageKey())
    storage_key = bridge_->GetStorageKey(data);
  return CreateEntity(storage_key, data);
}

bool ClientTagBasedModelTypeProcessor::AllStorageKeysPopulated() const {
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (entity->storage_key().empty())
      return false;
  }
  return true;
}

size_t ClientTagBasedModelTypeProcessor::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(model_type_state_);
  memory_usage += EstimateMemoryUsage(entities_);
  memory_usage += EstimateMemoryUsage(storage_key_to_tag_hash_);
  if (bridge_) {
    memory_usage += bridge_->EstimateSyncOverheadMemoryUsage();
  }
  return memory_usage;
}

bool ClientTagBasedModelTypeProcessor::HasLocalChangesForTest() const {
  return HasLocalChanges();
}

bool ClientTagBasedModelTypeProcessor::IsTrackingEntityForTest(
    const std::string& storage_key) const {
  return storage_key_to_tag_hash_.count(storage_key) != 0;
}

bool ClientTagBasedModelTypeProcessor::IsModelReadyToSyncForTest() const {
  return model_ready_to_sync_;
}

void ClientTagBasedModelTypeProcessor::ExpireAllEntries(
    MetadataChangeList* metadata_changes) {
  DCHECK(metadata_changes);

  std::vector<std::string> storage_key_to_be_deleted;
  for (const auto& kv : entities_) {
    ProcessorEntity* entity = kv.second.get();
    if (!entity->IsUnsynced()) {
      storage_key_to_be_deleted.push_back(entity->storage_key());
    }
  }

  // Delete selected keys while not iterating over |entities_|.
  for (const std::string& key : storage_key_to_be_deleted) {
    metadata_changes->ClearMetadata(key);
    auto iter = storage_key_to_tag_hash_.find(key);
    DCHECK(iter != storage_key_to_tag_hash_.end());
    entities_.erase(iter->second);
    storage_key_to_tag_hash_.erase(iter);
  }
}

void ClientTagBasedModelTypeProcessor::RemoveEntity(
    ProcessorEntity* entity,
    MetadataChangeList* metadata_change_list) {
  metadata_change_list->ClearMetadata(entity->storage_key());
  storage_key_to_tag_hash_.erase(entity->storage_key());
  entities_.erase(
      ClientTagHash::FromHashed(entity->metadata().client_tag_hash()));
}

void ClientTagBasedModelTypeProcessor::ResetState(
    SyncStopMetadataFate metadata_fate) {
  // This should reset all mutable fields (except for |bridge_|).
  worker_.reset();

  switch (metadata_fate) {
    case KEEP_METADATA:
      break;
    case CLEAR_METADATA:
      model_ready_to_sync_ = false;
      entities_.clear();
      storage_key_to_tag_hash_.clear();
      model_type_state_ = sync_pb::ModelTypeState();
      model_type_state_.mutable_progress_marker()->set_data_type_id(
          GetSpecificsFieldNumberFromModelType(type_));
      break;
  }

  // Do not let any delayed callbacks to be called.
  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
}

void ClientTagBasedModelTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  if (!bridge_)
    return;
  bridge_->GetAllDataForDebugging(base::BindOnce(
      &ClientTagBasedModelTypeProcessor::MergeDataWithMetadataForDebugging,
      weak_ptr_factory_for_worker_.GetWeakPtr(), std::move(callback)));
}

void ClientTagBasedModelTypeProcessor::MergeDataWithMetadataForDebugging(
    AllNodesCallback callback,
    std::unique_ptr<DataBatch> batch) {
  std::unique_ptr<base::ListValue> all_nodes =
      std::make_unique<base::ListValue>();
  std::string type_string = ModelTypeToString(type_);

  while (batch->HasNext()) {
    KeyAndData key_and_data = batch->Next();
    std::unique_ptr<EntityData> data = std::move(key_and_data.second);

    // There is an overlap between EntityData fields from the bridge and
    // EntityMetadata fields from the processor's entity, metadata is
    // the authoritative source of truth.
    ProcessorEntity* entity = GetEntityForStorageKey(key_and_data.first);
    // |entity| could be null if there are some unapplied changes.
    if (entity != nullptr) {
      const sync_pb::EntityMetadata& metadata = entity->metadata();
      // Set id value as directory, "s" means server.
      data->id = "s" + metadata.server_id();
      data->creation_time = ProtoTimeToTime(metadata.creation_time());
      data->modification_time = ProtoTimeToTime(metadata.modification_time());
      data->client_tag_hash =
          ClientTagHash::FromHashed(metadata.client_tag_hash());
    }

    std::unique_ptr<base::DictionaryValue> node = data->ToDictionaryValue();
    node->SetString("modelType", type_string);
    // Copy the whole metadata message into the dictionary (if existing).
    if (entity != nullptr) {
      node->Set("metadata", EntityMetadataToValue(entity->metadata()));
    }
    all_nodes->Append(std::move(node));
  }

  // Create a permanent folder for this data type. Since sync server no longer
  // create root folders, and USS won't migrate root folders from directory, we
  // create root folders for each data type here.
  std::unique_ptr<base::DictionaryValue> rootnode =
      std::make_unique<base::DictionaryValue>();
  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses modelType to check if root node is parent of real
  // data node. NON_UNIQUE_NAME will be the name of node to display.
  rootnode->SetString("PARENT_ID", "r");
  rootnode->SetString("UNIQUE_SERVER_TAG", type_string);
  rootnode->SetBoolean("IS_DIR", true);
  rootnode->SetString("modelType", type_string);
  rootnode->SetString("NON_UNIQUE_NAME", type_string);
  all_nodes->Append(std::move(rootnode));

  std::move(callback).Run(type_, std::move(all_nodes));
}

void ClientTagBasedModelTypeProcessor::GetStatusCountersForDebugging(
    StatusCountersCallback callback) {
  StatusCounters counters;
  counters.num_entries_and_tombstones = entities_.size();
  counters.num_entries = CountNonTombstoneEntries(entities_);
  std::move(callback).Run(type_, counters);
}

void ClientTagBasedModelTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  SyncRecordModelTypeMemoryHistogram(type_, EstimateMemoryUsage());
  SyncRecordModelTypeCountHistogram(type_, CountNonTombstoneEntries(entities_));
}

}  // namespace syncer

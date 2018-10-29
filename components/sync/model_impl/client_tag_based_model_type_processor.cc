// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "components/sync/model_impl/processor_entity_tracker.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

namespace {

bool CompareProtoTimeStamp(const int64_t left, const int64_t right) {
  return left > right;
}

// This function use quick select algorithm (std::nth_element) to find the |n|th
// bigest number in the vector |time_stamps|.
int64_t FindTheNthBigestProtoTimeStamp(std::vector<int64_t> time_stamps,
                                       size_t n) {
  DCHECK(n);

  if (n > time_stamps.size())
    return 0;

  std::nth_element(time_stamps.begin(), time_stamps.begin() + n - 1,
                   time_stamps.end(), &CompareProtoTimeStamp);

  return time_stamps[n - 1];
}

int CountNonTombstoneEntries(
    const std::map<std::string, std::unique_ptr<ProcessorEntityTracker>>&
        entities) {
  int count = 0;
  for (const auto& kv : entities) {
    if (!kv.second->metadata().is_deleted()) {
      ++count;
    }
  }
  return count;
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
      commit_only_(commit_only),
      weak_ptr_factory_for_controller_(this),
      weak_ptr_factory_for_worker_(this) {
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
      std::unique_ptr<ProcessorEntityTracker> entity =
          ProcessorEntityTracker::CreateFromMetadata(it->first, &it->second);
      storage_key_to_tag_hash_[entity->storage_key()] =
          entity->metadata().client_tag_hash();
      entities_[entity->metadata().client_tag_hash()] = std::move(entity);
    }
    model_type_state_ = batch->GetModelTypeState();
  } else {
    // TODO(crbug.com/872360): This DCHECK can currently trigger if the user's
    // persisted Sync metadata is in an inconsistent state.
    DCHECK(commit_only_ || batch->TakeAllMetadata().empty())
        << ModelTypeToString(type_);
    // First time syncing; initialize metadata.
    model_type_state_.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type_));
    // For commit-only types, no updates are expected and hence we can consider
    // initial_sync_done().
    model_type_state_.set_initial_sync_done(commit_only_);
  }

  ConnectIfReady();
}

bool ClientTagBasedModelTypeProcessor::IsModelReadyOrError() const {
  return model_error_ || model_ready_to_sync_;
}

bool ClientTagBasedModelTypeProcessor::IsAllowingChanges() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Changes can be handled correctly even before pending data is loaded.
  return model_ready_to_sync_;
}

void ClientTagBasedModelTypeProcessor::ConnectIfReady() {
  if (!IsModelReadyOrError() || !start_callback_)
    return;

  if (model_error_) {
    activation_request_.error_handler.Run(model_error_.value());
    start_callback_.Reset();
    return;
  }

  DCHECK(model_ready_to_sync_);

  if (!model_type_state_.has_cache_guid()) {
    model_type_state_.set_cache_guid(activation_request_.cache_guid);
  } else if (model_type_state_.cache_guid() != activation_request_.cache_guid) {
    // There is a mismatch between the cache guid stored in |model_type_state_|
    // and the one received from sync and stored it |activation_request_|. This
    // indicates that the stored metadata are invalid (e.g. has been
    // manipulated) and don't belong to the current syncing client.
    const ModelTypeSyncBridge::StopSyncResponse response =
        ClearMetadataAndResetState();

    switch (response) {
      case ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync:
        // The model is still ready to sync (with the same |bridge_|) - replay
        // the initialization.
        model_ready_to_sync_ = true;
        // For commit-only types, no updates are expected and hence we can
        // consider initial_sync_done().
        model_type_state_.set_initial_sync_done(commit_only_);
        // Notify the bridge sync is starting to simulate an enable event.
        bridge_->OnSyncStarting(activation_request_);
        break;
      case ModelTypeSyncBridge::StopSyncResponse::kModelNoLongerReadyToSync:
        // Model not ready to sync, so wait until the bridge calls
        // ModelReadyToSync().
        DCHECK(!model_ready_to_sync_);
        // Notify the bridge sync is starting to simulate an enable event.
        bridge_->OnSyncStarting(activation_request_);
        // Return early to avoid replying to OnSyncStarting() immediately. This
        // will be handled in ModelReadyToSync().
        return;
    }
  }

  // Cache GUID verification guarantees the user is the same.
  model_type_state_.set_authenticated_account_id(
      activation_request_.authenticated_account_id);

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
      switch (bridge_->ApplyStopSyncChanges(
          /*delete_metadata_change_list=*/nullptr)) {
        case ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync:
          // The model is still ready to sync (with the same |bridge_|) and same
          // sync metadata.
          ResetState(KEEP_METADATA);
          DCHECK(model_ready_to_sync_);
          break;
        case ModelTypeSyncBridge::StopSyncResponse::kModelNoLongerReadyToSync:
          // Model not ready to sync, so wait until the bridge calls
          // ModelReadyToSync(), and meanwhile throw away all metadata.
          ResetState(CLEAR_METADATA);
          DCHECK(!model_ready_to_sync_);
          break;
      }
      break;
    }

    case CLEAR_METADATA: {
      switch (ClearMetadataAndResetState()) {
        case ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync:
          // The model is still ready to sync (with the same |bridge_|) - replay
          // the initialization.
          ModelReadyToSync(std::make_unique<MetadataBatch>());
          DCHECK(model_ready_to_sync_);
          break;
        case ModelTypeSyncBridge::StopSyncResponse::kModelNoLongerReadyToSync:
          // Model not ready to sync, so wait until the bridge calls
          // ModelReadyToSync().
          DCHECK(!model_ready_to_sync_);
          break;
      }
      break;
    }
  }

  DCHECK(!IsConnected());
}

ModelTypeSyncBridge::StopSyncResponse
ClientTagBasedModelTypeProcessor::ClearMetadataAndResetState() {
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

  const ModelTypeSyncBridge::StopSyncResponse response =
      bridge_->ApplyStopSyncChanges(std::move(change_list));

  // Reset all the internal state of the processor.
  ResetState(CLEAR_METADATA);

  return response;
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

void ClientTagBasedModelTypeProcessor::ReportError(const ModelError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore all errors after the first.
  if (model_error_)
    return;

  model_error_ = error;

  if (dump_stack_) {
    // Upload a stack trace if possible.
    dump_stack_.Run();
  }

  if (IsConnected()) {
    DisconnectSync();
  }

  if (start_callback_) {
    // Tell sync about the error instead of connecting.
    ConnectIfReady();
  } else if (activation_request_.error_handler) {
    // Connecting was already initiated; just tell sync about the error instead
    // of going through ConnectIfReady().
    activation_request_.error_handler.Run(error);
  }
}

base::Optional<ModelError> ClientTagBasedModelTypeProcessor::GetError() const {
  return model_error_;
}

base::WeakPtr<ModelTypeControllerDelegate>
ClientTagBasedModelTypeProcessor::GetControllerDelegate() {
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
  DCHECK(!data->non_unique_name.empty());

  // Only the pseudo-USS bridge for PASSWORDS populates encrypted data.
  // TODO(crbug.com/856941): Remove when PASSWORDS are migrated to USS and
  // replace instead with a DCHECK that verifies the input is not encrypted.
  if (!data->specifics.has_encrypted()) {
    DCHECK_EQ(type_, GetModelTypeFromSpecifics(data->specifics));
  }

  if (!model_type_state_.initial_sync_done()) {
    // Ignore changes before the initial sync is done.
    return;
  }

  ProcessorEntityTracker* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // The bridge is creating a new entity. The bridge may or may not populate
    // |data->client_tag_hash|, so let's ask for the client tag if needed.
    if (data->client_tag_hash.empty()) {
      data->client_tag_hash = GetClientTagHash(storage_key, *data);
    } else if (bridge_->SupportsGetClientTag()) {
      // If the Put() call already included the client tag, let's verify that
      // it's consistent with the bridge's regular GetClientTag() function (if
      // supported by the bridge).
      DCHECK_EQ(data->client_tag_hash,
                GenerateSyncableHash(type_, bridge_->GetClientTag(*data)));
    }

    if (data->creation_time.is_null())
      data->creation_time = base::Time::Now();
    if (data->modification_time.is_null())
      data->modification_time = data->creation_time;
    entity = CreateEntity(storage_key, *data);
  } else if (entity->MatchesData(*data)) {
    // Ignore changes that don't actually change anything.
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

  ProcessorEntityTracker* entity = GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // That's unusual, but not necessarily a bad thing.
    // Missing is as good as deleted as far as the model is concerned.
    DLOG(WARNING) << "Attempted to delete missing item."
                  << " storage key: " << storage_key;
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
  const std::string& client_tag_hash = entity_data.client_tag_hash;
  DCHECK(!client_tag_hash.empty());
  DCHECK(!storage_key.empty());
  DCHECK(!bridge_->SupportsGetStorageKey());
  DCHECK(model_type_state_.initial_sync_done());

  ProcessorEntityTracker* entity = GetEntityForTagHash(client_tag_hash);
  DCHECK(entity);

  DCHECK(entity->storage_key().empty());
  DCHECK(storage_key_to_tag_hash_.find(storage_key) ==
         storage_key_to_tag_hash_.end());

  storage_key_to_tag_hash_[storage_key] = client_tag_hash;
  entity->SetStorageKey(storage_key);
  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
}

void ClientTagBasedModelTypeProcessor::UntrackEntity(
    const EntityData& entity_data) {
  const std::string& client_tag_hash = entity_data.client_tag_hash;
  DCHECK(model_type_state_.initial_sync_done());
  DCHECK(!client_tag_hash.empty());
  DCHECK(GetEntityForTagHash(client_tag_hash)->storage_key().empty());
  entities_.erase(client_tag_hash);
}

void ClientTagBasedModelTypeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {
  DCHECK(model_type_state_.initial_sync_done());

  // Look-up the client tag hash.
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  if (iter == storage_key_to_tag_hash_.end()) {
    // TODO(jkrcal): Add metrics keyed by the model_type to see which model_type
    // violates the proper behavior expected here (https://crbug.com/847822).
    // That's unusual, but not necessarily a bad thing.
    // Missing is as good as untracked as far as the model is concerned.
    DLOG(WARNING) << "Attempted to untrack missing item with storage_key: "
                  << storage_key;
    return;
  }

  entities_.erase(iter->second);
  storage_key_to_tag_hash_.erase(iter);
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
    ProcessorEntityTracker* entity = kv.second.get();
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
  DCHECK(!model_error_);

  std::vector<std::string> entities_requiring_data;
  for (const auto& kv : entities_) {
    ProcessorEntityTracker* entity = kv.second.get();
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
    // All commit data can be availbale in memory for those entries passed in
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
    ProcessorEntityTracker* entity = GetEntityForTagHash(data.client_tag_hash);
    if (entity == nullptr) {
      NOTREACHED() << "Received commit response for missing item."
                   << " type: " << ModelTypeToString(type_)
                   << " client_tag_hash: " << data.client_tag_hash;
      continue;
    }

    entity->ReceiveCommitResponse(data, commit_only_);

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
      std::move(metadata_change_list), entity_change_list);
  if (error) {
    ReportError(*error);
  }
}

// Returns an updates list that has client tag hashes populated for every
// update entity.
UpdateResponseDataList PopulateClientTagsForWalletData(
    const ModelType& type,
    ModelTypeSyncBridge* bridge,
    const UpdateResponseDataList& updates) {
  DCHECK(bridge->SupportsGetClientTag());
  UpdateResponseDataList updates_with_client_tags;
  for (const UpdateResponseData& update : updates) {
    if (update.entity->parent_id == "0") {
      // Ignore the permanent root node. Other places in this file detect them
      // by having empty client tags; this cannot be used for wallet_data as no
      // wallet_data entity has a client tag.
      continue;
    }
    updates_with_client_tags.push_back(update);
    updates_with_client_tags.back().entity =
        update.entity->UpdateClientTagHash(GenerateSyncableHash(
            type, bridge->GetClientTag(update.entity.value())));
  }
  return updates_with_client_tags;
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
    const UpdateResponseDataList& updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_ready_to_sync_);
  DCHECK(!model_error_);

  if (!ValidateUpdate(model_type_state, updates)) {
    return;
  }

  const UpdateResponseDataList* updates_to_process = &updates;
  UpdateResponseDataList pre_processed_updates;

  if (type_ == AUTOFILL_WALLET_DATA) {
    // The client tag based processor requires client tags to function properly.
    // However, the wallet data type does not have any client tags. This hacky
    // code manually asks the bridge to create the client tags for each update,
    // so that we can still use this processor. A proper fix would be to either
    // fully use client tags, or to use a different processor.
    // TODO(crbug.com/874001): Remove this feature-specific logic when the right
    // solution for Wallet data has been decided.
    pre_processed_updates =
        PopulateClientTagsForWalletData(type_, bridge_, updates);
    updates_to_process = &pre_processed_updates;
  }

  base::Optional<ModelError> error;

  // We call OnFullUpdateReceived when it's the first sync cycle, or when
  // we get a garbage collection directive from the server telling us to clear
  // all data by version watermark.
  // This means that if we receive a version watermark based GC directive, we
  // always clear all data. We do this to allow the server to replace all data
  // on the client, without having to know exactly which entities the client
  // has.
  if (!model_type_state_.initial_sync_done() ||
      HasClearAllDirective(model_type_state)) {
    error = OnFullUpdateReceived(model_type_state, *updates_to_process);
  } else {
    error = OnIncrementalUpdateReceived(model_type_state, *updates_to_process);
    ExpireEntriesIfNeeded(model_type_state.progress_marker());
  }

  if (error) {
    ReportError(*error);
    return;
  }

  // If there were trackers with empty storage keys, they should have been
  // updated by bridge as part of ApplySyncChanges.
  DCHECK(AllStorageKeysPopulated());
  // There may be new reasons to commit by the time this function is done.
  NudgeForCommitIfNeeded();
}

ProcessorEntityTracker* ClientTagBasedModelTypeProcessor::ProcessUpdate(
    const UpdateResponseData& update,
    EntityChangeList* entity_changes) {
  const EntityData& data = update.entity.value();
  const std::string& client_tag_hash = data.client_tag_hash;

  // Filter out updates without a client tag hash (including permanent nodes,
  // which have server tags instead).
  if (client_tag_hash.empty()) {
    return nullptr;
  }

  // Filter out unexpected client tag hashes.
  if (!data.is_deleted() && bridge_->SupportsGetClientTag() &&
      client_tag_hash !=
          GenerateSyncableHash(type_, bridge_->GetClientTag(data))) {
    DLOG(WARNING) << "Received unexpected client tag hash: " << client_tag_hash;
    return nullptr;
  }

  ProcessorEntityTracker* entity = GetEntityForTagHash(client_tag_hash);

  // Handle corner cases first.
  if (entity == nullptr && data.is_deleted()) {
    // Local entity doesn't exist and update is tombstone.
    DLOG(WARNING) << "Received remote delete for a non-existing item."
                  << " client_tag_hash: " << client_tag_hash;
    return nullptr;
  }
  if (entity && entity->UpdateIsReflection(update.response_version)) {
    // Seen this update before; just ignore it.
    return nullptr;
  }

  if (entity && entity->IsUnsynced()) {
    // Handle conflict resolution.
    ConflictResolution::Type resolution_type =
        ResolveConflict(update, entity, entity_changes);
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveConflict", resolution_type,
                              ConflictResolution::TYPE_SIZE);
  } else {
    // Handle simple create/delete/update.
    if (entity == nullptr) {
      entity = CreateEntity(data);
      entity_changes->push_back(
          EntityChange::CreateAdd(entity->storage_key(), update.entity));
    } else if (data.is_deleted()) {
      // The entity was deleted; inform the bridge. Note that the local data
      // can never be deleted at this point because it would have either been
      // acked (the add case) or pending (the conflict case).
      DCHECK(!entity->metadata().is_deleted());
      entity_changes->push_back(
          EntityChange::CreateDelete(entity->storage_key()));
    } else if (!entity->MatchesData(data)) {
      // Specifics have changed, so update the bridge.
      entity_changes->push_back(
          EntityChange::CreateUpdate(entity->storage_key(), update.entity));
    }
    entity->RecordAcceptedUpdate(update);
  }

  // If the received entity has out of date encryption, we schedule another
  // commit to fix it.
  if (model_type_state_.encryption_key_name() != update.encryption_key_name) {
    DVLOG(2) << ModelTypeToString(type_) << ": Requesting re-encrypt commit "
             << update.encryption_key_name << " -> "
             << model_type_state_.encryption_key_name();

    entity->IncrementSequenceNumber();
    if (entity->RequiresCommitData()) {
      // If there is no pending commit data, then either this update wasn't
      // in conflict or the remote data won; either way the remote data is
      // the right data to re-queue for commit.
      entity->CacheCommitData(update.entity);
    }
  }

  return entity;
}

ConflictResolution::Type ClientTagBasedModelTypeProcessor::ResolveConflict(
    const UpdateResponseData& update,
    ProcessorEntityTracker* entity,
    EntityChangeList* changes) {
  const EntityData& remote_data = update.entity.value();

  ConflictResolution::Type resolution_type = ConflictResolution::TYPE_SIZE;
  std::unique_ptr<EntityData> new_data;

  // Determine the type of resolution.
  if (entity->MatchesData(remote_data)) {
    // The changes are identical so there isn't a real conflict.
    resolution_type = ConflictResolution::CHANGES_MATCH;
  } else if (entity->RequiresCommitData() ||
             entity->MatchesBaseData(entity->commit_data().value())) {
    // If commit data needs to be loaded at this point, it can only be due to a
    // re-encryption request. If the commit data matches the base data, it also
    // must be a re-encryption request. Either way there's no real local change
    // and the remote data should win.
    resolution_type = ConflictResolution::IGNORE_LOCAL_ENCRYPTION;
  } else if (entity->MatchesBaseData(remote_data)) {
    // The remote data isn't actually changing from the last remote data that
    // was seen, so it must have been a re-encryption and can be ignored.
    resolution_type = ConflictResolution::IGNORE_REMOTE_ENCRYPTION;
  } else {
    // There's a real data conflict here; let the bridge resolve it.
    ConflictResolution resolution =
        bridge_->ResolveConflict(entity->commit_data().value(), remote_data);
    resolution_type = resolution.type();
    new_data = resolution.ExtractData();
  }

  // Apply the resolution.
  switch (resolution_type) {
    case ConflictResolution::CHANGES_MATCH:
      // Record the update and squash the pending commit.
      entity->RecordForcedUpdate(update);
      break;
    case ConflictResolution::USE_LOCAL:
    case ConflictResolution::IGNORE_REMOTE_ENCRYPTION:
      // Record that we received the update from the server but leave the
      // pending commit intact.
      entity->RecordIgnoredUpdate(update);
      break;
    case ConflictResolution::USE_REMOTE:
    case ConflictResolution::IGNORE_LOCAL_ENCRYPTION:
      // Squash the pending commit.
      entity->RecordForcedUpdate(update);
      // Update client data to match server.
      if (update.entity->is_deleted()) {
        changes->push_back(EntityChange::CreateDelete(entity->storage_key()));
      } else {
        changes->push_back(
            EntityChange::CreateUpdate(entity->storage_key(), update.entity));
      }
      break;
    case ConflictResolution::USE_NEW:
      // Record that we received the update.
      entity->RecordIgnoredUpdate(update);
      // Make a new pending commit to update the server.
      entity->MakeLocalChange(std::move(new_data));
      // Update the client with the new entity.
      changes->push_back(EntityChange::CreateUpdate(entity->storage_key(),
                                                    entity->commit_data()));
      break;
    case ConflictResolution::TYPE_SIZE:
      NOTREACHED();
      break;
  }
  DCHECK(!new_data);

  return resolution_type;
}

void ClientTagBasedModelTypeProcessor::RecommitAllForEncryption(
    const std::unordered_set<std::string>& already_updated,
    MetadataChangeList* metadata_changes) {
  ModelTypeSyncBridge::StorageKeyList entities_needing_data;

  for (const auto& kv : entities_) {
    ProcessorEntityTracker* entity = kv.second.get();
    if (entity->storage_key().empty() ||
        (already_updated.find(entity->storage_key()) !=
         already_updated.end())) {
      // Entities with empty storage key were already processed. ProcessUpdate()
      // incremented their sequence numbers and cached commit data. Their
      // metadata will be persisted in UpdateStorageKey().
      continue;
    }
    entity->IncrementSequenceNumber();
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
             !bridge_->SupportsIncrementalUpdates()) {
    // We receive empty updates (without clear all directive) from the server to
    // indicate nothing changed. We can just ignore these updates for bridges
    // that don't support incremental updates.
    if (!updates.empty()) {
      ReportError(ModelError(FROM_HERE,
                             "Received an update without version watermark for "
                             "bridge that does not support incremental "
                             "updates"));
    }
    return false;
  }
  return true;
}

base::Optional<ModelError>
ClientTagBasedModelTypeProcessor::OnFullUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    const UpdateResponseDataList& updates) {
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

  for (const UpdateResponseData& update : updates) {
    if (update.entity->client_tag_hash.empty()) {
      // Ignore updates missing a client tag hash (e.g. permanent nodes).
      continue;
    }
    if (update.entity->is_deleted()) {
      DLOG(WARNING) << "Ignoring tombstone found during initial update: "
                    << "client_tag_hash = " << update.entity->client_tag_hash;
      continue;
    }

#if DCHECK_IS_ON()
    // TODO(crbug.com/872360): The CreateEntity() call below assumes that no
    // entity with this client_tag_hash exists already, but in some cases it
    // does.
    if (entities_.find(update.entity->client_tag_hash) != entities_.end()) {
      DLOG(ERROR) << "Received duplicate client_tag_hash "
                  << update.entity->client_tag_hash << " for "
                  << ModelTypeToString(type_);
    }
#endif  // DCHECK_IS_ON()
    ProcessorEntityTracker* entity = CreateEntity(update.entity.value());
    entity->RecordAcceptedUpdate(update);
    const std::string& storage_key = entity->storage_key();
    entity_data.push_back(EntityChange::CreateAdd(storage_key, update.entity));
    if (!storage_key.empty())
      metadata_changes->UpdateMetadata(storage_key, entity->metadata());
  }

  // Let the bridge handle associating and merging the data.
  base::Optional<ModelError> error =
      bridge_->MergeSyncData(std::move(metadata_changes), entity_data);
  return error;
}

base::Optional<ModelError>
ClientTagBasedModelTypeProcessor::OnIncrementalUpdateReceived(
    const sync_pb::ModelTypeState& model_type_state,
    const UpdateResponseDataList& updates) {
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

  for (const UpdateResponseData& update : updates) {
    ProcessorEntityTracker* entity = ProcessUpdate(update, &entity_changes);

    if (!entity) {
      // The update is either of the following:
      // 1. Tombstone of entity that didn't exist locally.
      // 2. Reflection, thus should be ignored.
      // 3. Update without a client tag hash (including permanent nodes, which
      // have server tags instead).
      continue;
    }
    if (entity->storage_key().empty()) {
      // Storage key of this entity is not known yet. Don't update metadata, it
      // will be done from UpdateStorageKey.
      continue;
    }
    if (entity->CanClearMetadata()) {
      metadata_changes->ClearMetadata(entity->storage_key());
      storage_key_to_tag_hash_.erase(entity->storage_key());
      entities_.erase(entity->metadata().client_tag_hash());
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
    return bridge_->ApplySyncChangesWithNewEncryptionRequirements(
        std::move(metadata_changes), std::move(entity_changes));
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
    ProcessorEntityTracker* entity = GetEntityForStorageKey(storage_key);
    // If the entity wasn't deleted or updated with new commit.
    if (entity != nullptr && entity->RequiresCommitData()) {
      // SetCommitData will update EntityData's fields with values from
      // metadata.
      entity->SetCommitData(data.second.get());
    }
  }

  // Detect failed loads that shouldn't have failed.
  std::vector<std::string> storage_keys_to_untrack;
  for (const std::string& storage_key : storage_keys_to_load) {
    ProcessorEntityTracker* entity = GetEntityForStorageKey(storage_key);
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
                              ModelTypeToHistogramInt(type_),
                              static_cast<int>(MODEL_TYPE_COUNT));
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
    ProcessorEntityTracker* entity = kv.second.get();
    if (entity->RequiresCommitRequest() && !entity->RequiresCommitData()) {
      CommitRequestData request;
      entity->InitializeCommitRequestData(&request);
      commit_requests.push_back(request);
      if (commit_requests.size() >= max_entries) {
        break;
      }
    }
  }
  std::move(callback).Run(std::move(commit_requests));
}

std::string ClientTagBasedModelTypeProcessor::GetClientTagHash(
    const std::string& storage_key,
    const EntityData& data) const {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  DCHECK(bridge_->SupportsGetClientTag());
  return iter == storage_key_to_tag_hash_.end()
             ? GenerateSyncableHash(type_, bridge_->GetClientTag(data))
             : iter->second;
}

ProcessorEntityTracker*
ClientTagBasedModelTypeProcessor::GetEntityForStorageKey(
    const std::string& storage_key) {
  auto iter = storage_key_to_tag_hash_.find(storage_key);
  return iter == storage_key_to_tag_hash_.end()
             ? nullptr
             : GetEntityForTagHash(iter->second);
}

ProcessorEntityTracker* ClientTagBasedModelTypeProcessor::GetEntityForTagHash(
    const std::string& tag_hash) {
  auto it = entities_.find(tag_hash);
  return it != entities_.end() ? it->second.get() : nullptr;
}

ProcessorEntityTracker* ClientTagBasedModelTypeProcessor::CreateEntity(
    const std::string& storage_key,
    const EntityData& data) {
  DCHECK(!data.client_tag_hash.empty());
  DCHECK(entities_.find(data.client_tag_hash) == entities_.end());
  DCHECK(!bridge_->SupportsGetStorageKey() || !storage_key.empty());
  DCHECK(storage_key.empty() || storage_key_to_tag_hash_.find(storage_key) ==
                                    storage_key_to_tag_hash_.end());
  std::unique_ptr<ProcessorEntityTracker> entity =
      ProcessorEntityTracker::CreateNew(storage_key, data.client_tag_hash,
                                        data.id, data.creation_time);
  ProcessorEntityTracker* entity_ptr = entity.get();
  entities_[data.client_tag_hash] = std::move(entity);
  if (!storage_key.empty())
    storage_key_to_tag_hash_[storage_key] = data.client_tag_hash;
  return entity_ptr;
}

ProcessorEntityTracker* ClientTagBasedModelTypeProcessor::CreateEntity(
    const EntityData& data) {
  if (bridge_->SupportsGetClientTag()) {
    DCHECK_EQ(data.client_tag_hash,
              GenerateSyncableHash(type_, bridge_->GetClientTag(data)));
  }
  std::string storage_key;
  if (bridge_->SupportsGetStorageKey())
    storage_key = bridge_->GetStorageKey(data);
  return CreateEntity(storage_key, data);
}

bool ClientTagBasedModelTypeProcessor::AllStorageKeysPopulated() const {
  for (const auto& kv : entities_) {
    ProcessorEntityTracker* entity = kv.second.get();
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

bool ClientTagBasedModelTypeProcessor::IsModelReadyToSyncForTest() const {
  return model_ready_to_sync_;
}

void ClientTagBasedModelTypeProcessor::ExpireEntriesIfNeeded(
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  if (!progress_marker.has_gc_directive())
    return;

  const sync_pb::GarbageCollectionDirective& new_gc_directive =
      progress_marker.gc_directive();
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  bool has_expired_changes = false;

  if (new_gc_directive.has_age_watermark_in_days()) {
    DCHECK(new_gc_directive.age_watermark_in_days());
    // For saving resource purpose(ex. cpu, battery), We round up garbage
    // collection age to day, so we only run GC once a day if server did not
    // change the |age_watermark_in_days|.
    base::Time to_be_expired =
        base::Time::Now().LocalMidnight() -
        base::TimeDelta::FromDays(new_gc_directive.age_watermark_in_days());
    if (cached_gc_directive_aged_out_day_ != to_be_expired) {
      ExpireEntriesByAge(new_gc_directive.age_watermark_in_days(),
                         metadata_changes.get());
      cached_gc_directive_aged_out_day_ = to_be_expired;
      has_expired_changes = true;
    }
  }

  if (new_gc_directive.has_max_number_of_items()) {
    DCHECK(new_gc_directive.max_number_of_items());
    ExpireEntriesByItemLimit(new_gc_directive.max_number_of_items(),
                             metadata_changes.get());
    has_expired_changes = true;
  }

  if (has_expired_changes)
    bridge_->ApplySyncChanges(std::move(metadata_changes), EntityChangeList());
}

void ClientTagBasedModelTypeProcessor::ClearMetadataForEntries(
    const std::vector<std::string>& storage_key_to_be_deleted,
    MetadataChangeList* metadata_changes) {
  for (const std::string& key : storage_key_to_be_deleted) {
    metadata_changes->ClearMetadata(key);
    auto iter = storage_key_to_tag_hash_.find(key);
    DCHECK(iter != storage_key_to_tag_hash_.end());
    entities_.erase(iter->second);
    storage_key_to_tag_hash_.erase(key);
  }
}

void ClientTagBasedModelTypeProcessor::ExpireAllEntries(
    MetadataChangeList* metadata_changes) {
  DCHECK(metadata_changes);

  std::vector<std::string> storage_key_to_be_deleted;
  for (const auto& kv : entities_) {
    ProcessorEntityTracker* entity = kv.second.get();
    if (!entity->IsUnsynced()) {
      storage_key_to_be_deleted.push_back(entity->storage_key());
    }
  }

  ClearMetadataForEntries(storage_key_to_be_deleted, metadata_changes);
}

void ClientTagBasedModelTypeProcessor::ExpireEntriesByAge(
    int32_t age_watermark_in_days,
    MetadataChangeList* metadata_changes) {
  DCHECK(metadata_changes);

  base::Time to_be_expired =
      base::Time::Now() - base::TimeDelta::FromDays(age_watermark_in_days);
  std::vector<std::string> storage_key_to_be_deleted;
  for (const auto& kv : entities_) {
    ProcessorEntityTracker* entity = kv.second.get();
    if (!entity->IsUnsynced() &&
        ProtoTimeToTime(entity->metadata().modification_time()) <=
            to_be_expired) {
      storage_key_to_be_deleted.push_back(entity->storage_key());
    }
  }

  ClearMetadataForEntries(storage_key_to_be_deleted, metadata_changes);
}

void ClientTagBasedModelTypeProcessor::ExpireEntriesByItemLimit(
    int32_t max_number_of_items,
    MetadataChangeList* metadata_changes) {
  DCHECK(metadata_changes);

  size_t limited_number = max_number_of_items;
  if (limited_number >= entities_.size())
    return;

  std::vector<int64_t> all_proto_times;
  for (const auto& kv : entities_) {
    ProcessorEntityTracker* entity = kv.second.get();
    all_proto_times.push_back(entity->metadata().modification_time());
  }
  int64_t expired_proto_time = FindTheNthBigestProtoTimeStamp(
      std::move(all_proto_times), limited_number);

  std::vector<std::string> storage_key_to_be_deleted;
  for (const auto& kv : entities_) {
    ProcessorEntityTracker* entity = kv.second.get();
    if (!entity->IsUnsynced() &&
        entity->metadata().modification_time() < expired_proto_time) {
      storage_key_to_be_deleted.push_back(entity->storage_key());
    }
  }

  ClearMetadataForEntries(storage_key_to_be_deleted, metadata_changes);
}

void ClientTagBasedModelTypeProcessor::RemoveEntity(
    ProcessorEntityTracker* entity,
    MetadataChangeList* metadata_change_list) {
  metadata_change_list->ClearMetadata(entity->storage_key());
  storage_key_to_tag_hash_.erase(entity->storage_key());
  entities_.erase(entity->metadata().client_tag_hash());
}

void ClientTagBasedModelTypeProcessor::ResetState(
    SyncStopMetadataFate metadata_fate) {
  // This should reset all mutable fields (except for |bridge_|).
  worker_.reset();
  cached_gc_directive_aged_out_day_ = base::Time::FromDoubleT(0);

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
    KeyAndData data = batch->Next();
    std::unique_ptr<base::DictionaryValue> node =
        data.second->ToDictionaryValue();
    ProcessorEntityTracker* entity = GetEntityForStorageKey(data.first);
    // Entity could be null if there are some unapplied changes.
    if (entity != nullptr) {
      std::unique_ptr<base::DictionaryValue> metadata =
          EntityMetadataToValue(entity->metadata());
      base::Value* server_id = metadata->FindKey("server_id");
      if (server_id) {
        // Set ID value as directory, "s" means server.
        node->SetString("ID", "s" + server_id->GetString());
      }
      node->Set("metadata", std::move(metadata));
    }
    node->SetString("modelType", type_string);
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

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
#include "components/sync/model_impl/client_tag_based_remote_update_handler.h"
#include "components/sync/model_impl/processor_entity.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {
namespace {

const char kErrorSiteHistogramPrefix[] = "Sync.ModelTypeErrorSite.";

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
  DCHECK(!entity_tracker_);
  DCHECK(!model_ready_to_sync_);

  model_ready_to_sync_ = true;

  // The model already experienced an error; abort;
  if (model_error_)
    return;

  if (batch->GetModelTypeState().initial_sync_done()) {
    EntityMetadataMap metadata_map(batch->TakeAllMetadata());
    entity_tracker_ = std::make_unique<ProcessorEntityTracker>(
        batch->GetModelTypeState(), std::move(metadata_map));
  } else {
    // In older versions of the binary, commit-only types did not persist
    // initial_sync_done(). So this branch can be exercised for commit-only
    // types exactly once as an upgrade flow.
    // TODO(crbug.com/872360): This DCHECK can currently trigger if the user's
    // persisted Sync metadata is in an inconsistent state.
    DCHECK(commit_only_ || batch->TakeAllMetadata().empty())
        << ModelTypeToString(type_);
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

  CheckForInvalidPersistedMetadata();

  auto activation_response = std::make_unique<DataTypeActivationResponse>();
  if (!entity_tracker_) {
    sync_pb::ModelTypeState model_type_state;
    model_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type_));
    model_type_state.set_cache_guid(activation_request_.cache_guid);
    model_type_state.set_authenticated_account_id(
        activation_request_.authenticated_account_id.ToString());
    if (commit_only_) {
      // For commit-only types, no updates are expected and hence we can
      // consider initial_sync_done(), reflecting that sync is enabled.
      model_type_state.set_initial_sync_done(true);
      OnFullUpdateReceived(model_type_state, UpdateResponseDataList());
      DCHECK(entity_tracker_);
    } else {
      activation_response->model_type_state = model_type_state;
    }
  }

  if (entity_tracker_) {
    activation_response->model_type_state = entity_tracker_->model_type_state();
  }

  DCHECK_EQ(activation_response->model_type_state.cache_guid(),
            activation_request_.cache_guid);

  activation_response->type_processor =
      std::make_unique<ModelTypeProcessorProxy>(
          weak_ptr_factory_for_worker_.GetWeakPtr(),
          base::SequencedTaskRunnerHandle::Get());

  // Defer invoking of |start_callback_| to avoid synchronous call from the
  // |bridge_|. It might cause a situation when inside the ModelReadyToSync()
  // another methods of the bridge eventually were called. This behavior would
  // be complicated and be unexpected in some bridges.
  // See crbug.com/1055584 for more details.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(start_callback_),
                                std::move(activation_response)));
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

  // All changes before the initial sync is done are ignored and in fact they
  // were never persisted by the bridge (prior to MergeSyncData), so no
  // entities should be tracking.
  //
  // Clear metadata if MergeSyncData() was called before.
  if (entity_tracker_) {
    change_list = bridge_->CreateMetadataChangeList();
    std::vector<const ProcessorEntity*> entities =
        entity_tracker_->GetAllEntitiesIncludingTombstones();
    for (const ProcessorEntity* entity : entities) {
      change_list->ClearMetadata(entity->storage_key());
    }
    change_list->ClearModelTypeState();
  }

  bridge_->ApplyStopSyncChanges(std::move(change_list));

  // Reset all the internal state of the processor.
  ResetState(CLEAR_METADATA);
}

bool ClientTagBasedModelTypeProcessor::IsTrackingMetadata() {
  return entity_tracker_ != nullptr;
}

std::string ClientTagBasedModelTypeProcessor::TrackedAccountId() {
  // Returning non-empty here despite !IsTrackingMetadata() has weird semantics,
  // e.g. initial updates are being fetched but we haven't received the response
  // (i.e. prior to exercising MergeSyncData()). Let's be cautious and hide the
  // account ID.
  if (!IsTrackingMetadata()) {
    return "";
  }
  return entity_tracker_->model_type_state().authenticated_account_id();
}

std::string ClientTagBasedModelTypeProcessor::TrackedCacheGuid() {
  // Returning non-empty here despite !IsTrackingMetadata() has weird semantics,
  // e.g. initial updates are being fetched but we haven't received the response
  // (i.e. prior to exercising MergeSyncData()). Let's be cautious and hide the
  // cache GUID.
  if (!IsTrackingMetadata()) {
    return "";
  }
  return entity_tracker_->model_type_state().cache_guid();
}

void ClientTagBasedModelTypeProcessor::ReportError(const ModelError& error) {
  ReportErrorImpl(error, ErrorSite::kBridgeInitiated);
}

void ClientTagBasedModelTypeProcessor::ReportErrorImpl(const ModelError& error,
                                                       ErrorSite site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore all errors after the first.
  if (model_error_) {
    return;
  }

  model_error_ = error;

  const std::string type_suffix = ModelTypeToHistogramSuffix(type_);
  base::UmaHistogramEnumeration(kErrorSiteHistogramPrefix + type_suffix, site);

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

  if (entity_tracker_) {
    entity_tracker_->ClearTransientSyncState();
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
  DCHECK(!storage_key.empty());
  DCHECK_EQ(type_, GetModelTypeFromSpecifics(data->specifics));

  if (!entity_tracker_) {
    // Ignore changes before the initial sync is done.
    return;
  }

  ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // The bridge is creating a new entity. The bridge may or may not populate
    // |data->client_tag_hash|, so let's ask for the client tag if needed.
    if (data->client_tag_hash.value().empty()) {
      DCHECK(bridge_->SupportsGetClientTag());
      data->client_tag_hash =
          ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(*data));
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
    entity = entity_tracker_->GetEntityForTagHash(data->client_tag_hash);
    if (entity != nullptr) {
      DCHECK(storage_key != entity->storage_key());
      if (!entity->metadata().is_deleted()) {
        // The bridge overrides an entity that is not deleted. This is
        // unexpected but the processor tolerates it. It is very likely a
        // metadata orphan; report it to metrics.
        UMA_HISTOGRAM_ENUMERATION("Sync.ModelTypeOrphanMetadata.Put",
                                  ModelTypeHistogramValue(type_));
      }
      // Remove the old storage key from the tracker and the corresponding
      // metadata record.
      metadata_change_list->ClearMetadata(entity->storage_key());
      entity_tracker_->UpdateOrOverrideStorageKey(data->client_tag_hash,
                                                  storage_key);
    } else {
      if (data->creation_time.is_null())
        data->creation_time = base::Time::Now();
      if (data->modification_time.is_null())
        data->modification_time = data->creation_time;
      entity = CreateEntity(storage_key, *data);
    }
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

  if (!entity_tracker_) {
    // Ignore changes before the initial sync is done.
    return;
  }

  ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // Missing is as good as deleted as far as the model is concerned.
    return;
  }

  if (entity->Delete())
    metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
  else
    RemoveEntity(entity->storage_key(), metadata_change_list);

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
  DCHECK(entity_tracker_);

  const ProcessorEntity* entity =
      entity_tracker_->GetEntityForTagHash(client_tag_hash);
  DCHECK(entity);

  DCHECK(entity->storage_key().empty());
  entity_tracker_->UpdateOrOverrideStorageKey(client_tag_hash, storage_key);

  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
}

void ClientTagBasedModelTypeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {
  if (!entity_tracker_) {
    // Ignore changes before the initial sync is done.
    return;
  }
  entity_tracker_->RemoveEntityForStorageKey(storage_key);
}

void ClientTagBasedModelTypeProcessor::UntrackEntityForClientTagHash(
    const ClientTagHash& client_tag_hash) {
  DCHECK(!client_tag_hash.value().empty());
  if (!entity_tracker_) {
    // Ignore changes before the initial sync is done.
    return;
  }
  entity_tracker_->RemoveEntityForClientTagHash(client_tag_hash);
}

bool ClientTagBasedModelTypeProcessor::IsEntityUnsynced(
    const std::string& storage_key) {
  if (!entity_tracker_) {
    return false;
  }

  const ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    return false;
  }

  return entity->IsUnsynced();
}

base::Time ClientTagBasedModelTypeProcessor::GetEntityCreationTime(
    const std::string& storage_key) const {
  if (!entity_tracker_) {
    return base::Time();
  }

  const ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    return base::Time();
  }
  return ProtoTimeToTime(entity->metadata().creation_time());
}

base::Time ClientTagBasedModelTypeProcessor::GetEntityModificationTime(
    const std::string& storage_key) const {
  if (!entity_tracker_) {
    return base::Time();
  }

  const ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
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
  if (!entity_tracker_)
    return;

  // Nudge worker if there are any entities with local changes.0
  if (entity_tracker_->HasLocalChanges())
    worker_->NudgeForCommit();
}

void ClientTagBasedModelTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(max_entries, 0U);

  // If there is a model error, it must have been reported already but hasn't
  // reached the sync engine yet. In this case return directly to avoid
  // interactions with the bridge.
  // In some cases local changes may be requested before entity tracker is
  // loaded. Just invoke the callback with empty list.
  if (model_error_ || !entity_tracker_) {
    std::move(callback).Run(CommitRequestDataList());
    return;
  }

  std::vector<const ProcessorEntity*> entities =
      entity_tracker_->GetAllEntitiesIncludingTombstones();
  std::vector<std::string> entities_requiring_data;
  for (const ProcessorEntity* entity : entities) {
    if (entity->RequiresCommitData())
      entities_requiring_data.push_back(entity->storage_key());
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
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!model_error_);

  DCHECK(entity_tracker_)
      << "Received commit response when entity tracker is null. Type: "
      << ModelTypeToString(type_);

  // |error_response_list| is ignored, because all errors are treated as
  // transientand the processor with eventually retry.

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      bridge_->CreateMetadataChangeList();
  EntityChangeList entity_change_list;

  entity_tracker_->set_model_type_state(model_type_state);
  metadata_change_list->UpdateModelTypeState(
      entity_tracker_->model_type_state());

  for (const CommitResponseData& data : committed_response_list) {
    ProcessorEntity* entity =
        entity_tracker_->GetEntityForTagHash(data.client_tag_hash);
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
        RemoveEntity(entity->storage_key(), metadata_change_list.get());
      }
      // If unsynced, we could theoretically update persisted metadata to have
      // more accurate bookkeeping. However, this wouldn't actually do anything
      // useful, we still need to commit again, and we're not going to include
      // any of the changing metadata in the commit message. So skip updating
      // metadata.
    } else if (entity->CanClearMetadata()) {
      RemoveEntity(entity->storage_key(), metadata_change_list.get());
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
  entity_tracker_->ClearTransientSyncState();

  base::Optional<ModelError> error = bridge_->ApplySyncChanges(
      std::move(metadata_change_list), std::move(entity_change_list));

  if (!error_response_list.empty()) {
    bridge_->OnCommitAttemptErrors(error_response_list);
  }

  if (error) {
    ReportErrorImpl(*error, ErrorSite::kApplyUpdatesOnCommitResponse);
  }
}

// Returns whether the state has a version_watermark based GC directive, which
// tells us to clear all sync data that's stored locally.
bool HasClearAllDirective(const sync_pb::ModelTypeState& model_type_state) {
  return model_type_state.progress_marker()
      .gc_directive()
      .has_version_watermark();
}

void ClientTagBasedModelTypeProcessor::OnCommitFailed(
    SyncCommitError commit_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bridge_->OnCommitAttemptFailed(commit_error);
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

  base::Optional<ModelError> error;

  // We call OnFullUpdateReceived when it's the first sync cycle, or when
  // we get a garbage collection directive from the server telling us to clear
  // all data by version watermark.
  // This means that if we receive a version watermark based GC directive, we
  // always clear all data. We do this to allow the server to replace all data
  // on the client, without having to know exactly which entities the client
  // has.
  const bool is_initial_sync = !IsTrackingMetadata();
  const bool treating_as_full_update =
      is_initial_sync || HasClearAllDirective(model_type_state);
  if (treating_as_full_update) {
    error = OnFullUpdateReceived(model_type_state, std::move(updates));
  } else {
    error = OnIncrementalUpdateReceived(model_type_state, std::move(updates));
  }

  if (error) {
    ReportErrorImpl(*error, treating_as_full_update
                                ? ErrorSite::kApplyFullUpdates
                                : ErrorSite::kApplyIncrementalUpdates);
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

  DCHECK(entity_tracker_);
  // If there were entities with empty storage keys, they should have been
  // updated by bridge as part of ApplySyncChanges.
  DCHECK(entity_tracker_->AllStorageKeysPopulated());
  // There may be new reasons to commit by the time this function is done.
  NudgeForCommitIfNeeded();
}

bool ClientTagBasedModelTypeProcessor::ValidateUpdate(
    const sync_pb::ModelTypeState& model_type_state,
    const UpdateResponseDataList& updates) {
  if (!entity_tracker_) {
    // Due to uss_migrator, initial sync (when migrating from non-USS) does not
    // contain any gc directives. Thus, we cannot expect the conditions below to
    // be satisfied. It is okay to skip the check as for an initial sync, the gc
    // directive does not make any semantical difference.
    return true;
  }

  if (HasClearAllDirective(model_type_state) &&
      bridge_->SupportsIncrementalUpdates()) {
    ReportErrorImpl(ModelError(FROM_HERE,
                               "Received an update with version watermark for "
                               "bridge that supports incremental updates"),
                    ErrorSite::kSupportsIncrementalUpdatesMismatch);

    return false;
  } else if (!HasClearAllDirective(model_type_state) &&
             !bridge_->SupportsIncrementalUpdates() && !updates.empty()) {
    // We receive an update without clear all directive from the server to
    // indicate no data has changed. This contradicts with the list of updates
    // being non-empty, the bridge cannot handle it and we need to fail here.
    // (If the last condition does not hold true and the list of updates is
    // empty, we still need to pass the empty update to the bridge because the
    // progress marker might have changed.)
    ReportErrorImpl(ModelError(FROM_HERE,
                               "Received a non-empty update without version "
                               "watermark for bridge that does not support "
                               "incremental updates"),
                    ErrorSite::kSupportsIncrementalUpdatesMismatch);
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

  // Check that the worker correctly marked initial sync as done for this
  // update.
  DCHECK(model_type_state.initial_sync_done());

  // Ensure that this is the initial sync, and it was not already marked done.
  DCHECK(HasClearAllDirective(model_type_state) || !entity_tracker_);

  if (entity_tracker_ && HasClearAllDirective(model_type_state)) {
    ExpireAllEntries(metadata_changes.get());
    entity_tracker_->set_model_type_state(model_type_state);
  }

  if (!entity_tracker_) {
    entity_tracker_ = std::make_unique<ProcessorEntityTracker>(
        model_type_state, EntityMetadataMap());
  }

  // TODO(crbug.com/1041888): the comment below may be wrong in case where a
  // datatype supports non-incremental updates and local updates are
  // acceptable.
  // Given that we either just removed all existing sync entities (in the full
  // update case).
  DCHECK(!entity_tracker_->size());

  metadata_changes->UpdateModelTypeState(entity_tracker_->model_type_state());

  EntityChangeList entity_data;
  for (syncer::UpdateResponseData& update : updates) {
    const ClientTagHash& client_tag_hash = update.entity.client_tag_hash;
    if (client_tag_hash.value().empty()) {
      // Ignore updates missing a client tag hash (e.g. permanent nodes).
      continue;
    }
    if (update.entity.is_deleted()) {
      SyncRecordModelTypeUpdateDropReason(
          UpdateDropReason::kTombstoneInFullUpdate, type_);
      DLOG(WARNING) << "Ignoring tombstone found during initial update: "
                    << "client_tag_hash = " << client_tag_hash << " for "
                    << ModelTypeToString(type_);
      continue;
    }
    if (bridge_->SupportsGetClientTag() &&
        client_tag_hash != ClientTagHash::FromUnhashed(
                               type_, bridge_->GetClientTag(update.entity))) {
      SyncRecordModelTypeUpdateDropReason(
          UpdateDropReason::kInconsistentClientTag, type_);
      DLOG(WARNING) << "Received unexpected client tag hash: "
                    << client_tag_hash << " for " << ModelTypeToString(type_);
      continue;
    }

    std::string storage_key;
    if (bridge_->SupportsGetStorageKey()) {
      storage_key = bridge_->GetStorageKey(update.entity);
      if (storage_key.empty()) {
        SyncRecordModelTypeUpdateDropReason(
            UpdateDropReason::kCannotGenerateStorageKey, type_);
        DLOG(WARNING) << "Received entity with invalid update for "
                      << ModelTypeToString(type_);
        continue;
      }
    }
#if DCHECK_IS_ON()
    // TODO(crbug.com/872360): The CreateEntity() call below assumes that no
    // entity with this client_tag_hash exists already, but in some cases it
    // does.
    if (entity_tracker_->GetEntityForTagHash(client_tag_hash)) {
      DLOG(ERROR) << "Received duplicate client_tag_hash " << client_tag_hash
                  << " for " << ModelTypeToString(type_);
    }
#endif  // DCHECK_IS_ON()
    ProcessorEntity* entity = CreateEntity(storage_key, update.entity);
    entity->RecordAcceptedUpdate(update);
    entity_data.push_back(
        EntityChange::CreateAdd(storage_key, std::move(update.entity)));
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
  DCHECK(entity_tracker_);

  ClientTagBasedRemoteUpdateHandler updates_handler(type_, bridge_,
                                                    entity_tracker_.get());
  return updates_handler.ProcessIncrementalUpdate(model_type_state,
                                                  std::move(updates));
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
  CommitLocalChanges(max_entries, std::move(callback));
}

void ClientTagBasedModelTypeProcessor::ConsumeDataBatch(
    std::unordered_set<std::string> storage_keys_to_load,
    std::unique_ptr<DataBatch> data_batch) {
  DCHECK(entity_tracker_);
  while (data_batch->HasNext()) {
    KeyAndData data = data_batch->Next();
    const std::string& storage_key = data.first;

    storage_keys_to_load.erase(storage_key);
    ProcessorEntity* entity =
        entity_tracker_->GetEntityForStorageKey(storage_key);
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
    const ProcessorEntity* entity =
        entity_tracker_->GetEntityForStorageKey(storage_key);
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
    UMA_HISTOGRAM_ENUMERATION("Sync.ModelTypeOrphanMetadata.GetData",
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
  DCHECK(entity_tracker_);
  // Prepares entities commit request data for entities which are
  // out of sync with the sync thread.
  CommitRequestDataList commit_requests;
  // TODO(rlarocque): Do something smarter than iterate here.
  std::vector<ProcessorEntity*> entities =
      entity_tracker_->GetEntitiesWithLocalChanges(max_entries);
  for (ProcessorEntity* entity : entities) {
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

ProcessorEntity* ClientTagBasedModelTypeProcessor::CreateEntity(
    const std::string& storage_key,
    const EntityData& data) {
  DCHECK(!bridge_->SupportsGetStorageKey() || !storage_key.empty());
  DCHECK(entity_tracker_);
  ProcessorEntity* entity_ptr = entity_tracker_->Add(storage_key, data);
  return entity_ptr;
}

size_t ClientTagBasedModelTypeProcessor::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  if (entity_tracker_) {
    memory_usage += entity_tracker_->EstimateMemoryUsage();
  }
  if (bridge_) {
    memory_usage += bridge_->EstimateSyncOverheadMemoryUsage();
  }
  return memory_usage;
}

bool ClientTagBasedModelTypeProcessor::HasLocalChangesForTest() const {
  return entity_tracker_ && entity_tracker_->HasLocalChanges();
}

bool ClientTagBasedModelTypeProcessor::IsTrackingEntityForTest(
    const std::string& storage_key) const {
  return entity_tracker_ &&
         entity_tracker_->GetEntityForStorageKey(storage_key) != nullptr;
}

bool ClientTagBasedModelTypeProcessor::IsModelReadyToSyncForTest() const {
  return model_ready_to_sync_;
}

void ClientTagBasedModelTypeProcessor::ExpireAllEntries(
    MetadataChangeList* metadata_changes) {
  DCHECK(metadata_changes);
  DCHECK(entity_tracker_);

  std::vector<std::string> storage_key_to_be_deleted;
  for (const ProcessorEntity* entity :
       entity_tracker_->GetAllEntitiesIncludingTombstones()) {
    if (!entity->IsUnsynced()) {
      storage_key_to_be_deleted.push_back(entity->storage_key());
    }
  }

  for (const std::string& key : storage_key_to_be_deleted) {
    RemoveEntity(key, metadata_changes);
  }
}

void ClientTagBasedModelTypeProcessor::RemoveEntity(
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  DCHECK(!storage_key.empty());
  DCHECK(entity_tracker_);
  DCHECK(entity_tracker_->GetEntityForStorageKey(storage_key));
  metadata_change_list->ClearMetadata(storage_key);
  entity_tracker_->RemoveEntityForStorageKey(storage_key);
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
      entity_tracker_.reset();
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
    const ProcessorEntity* entity =
        entity_tracker_->GetEntityForStorageKey(key_and_data.first);
    // |entity| could be null if there are some unapplied changes.
    if (entity != nullptr) {
      const sync_pb::EntityMetadata& metadata = entity->metadata();
      // Set id value as the legacy Directory implementation, "s" means server.
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
  // creates root folders, and USS won't migrate root folders from the
  // Directory, we create root folders for each data type here.
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

void ClientTagBasedModelTypeProcessor::CheckForInvalidPersistedMetadata() {
  if (!entity_tracker_) {
    return;
  }

  const sync_pb::ModelTypeState& model_type_state =
      entity_tracker_->model_type_state();
  const bool invalid_cache_guid =
      model_type_state.cache_guid() != activation_request_.cache_guid;
  const bool invalid_data_type_id =
      model_type_state.progress_marker().data_type_id() !=
      GetSpecificsFieldNumberFromModelType(type_);
  const bool invalid_account_id =
      model_type_state.authenticated_account_id() !=
      activation_request_.authenticated_account_id.ToString();
  // Do not check for the authenticated_account_id since the cache GUID equality
  // implies account ID equality (verified in ProfileSyncService).
  //
  // Check for invalid persisted metadata.
  // TODO(crbug.com/1079314): add UMA for each case of inconsistent data.
  if (!invalid_cache_guid && !invalid_data_type_id) {
    if (invalid_account_id) {
      sync_pb::ModelTypeState update_model_type_state = model_type_state;
      update_model_type_state.set_authenticated_account_id(
          activation_request_.authenticated_account_id.ToString());
      entity_tracker_->set_model_type_state(update_model_type_state);
    }
    return;
  }
  // There is a mismatch between the cache guid or the data type id stored
  // in |model_type_state_| and the one received from sync. This indicates
  // that the stored metadata are invalid (e.g. has been manipulated) and
  // don't belong to the current syncing client.
  if (model_type_state.progress_marker().data_type_id() !=
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
  DCHECK(!entity_tracker_);
}

void ClientTagBasedModelTypeProcessor::GetStatusCountersForDebugging(
    StatusCountersCallback callback) {
  StatusCounters counters;
  if (entity_tracker_) {
    counters.num_entries_and_tombstones = entity_tracker_->size();
    counters.num_entries = entity_tracker_->CountNonTombstoneEntries();
  }
  std::move(callback).Run(type_, counters);
}

void ClientTagBasedModelTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  SyncRecordModelTypeMemoryHistogram(type_, EstimateMemoryUsage());
  const size_t non_tombstone_entries_count =
      entity_tracker_ == nullptr ? 0
                                 : entity_tracker_->CountNonTombstoneEntries();
  SyncRecordModelTypeCountHistogram(type_, non_tombstone_entries_count);
}

}  // namespace syncer

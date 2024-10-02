// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/client_tag_based_data_type_processor.h"

#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/data_type_processor_metrics.h"
#include "components/sync/engine/data_type_processor_proxy.h"
#include "components/sync/model/client_tag_based_remote_update_handler.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/processor_entity.h"
#include "components/sync/model/type_entities_count.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace syncer {
namespace {

const char kErrorSiteHistogramPrefix[] = "Sync.DataTypeErrorSite.";

size_t CountDuplicateClientTags(const EntityMetadataMap& metadata_map) {
  size_t count = 0u;
  std::set<std::string> client_tag_hashes;
  for (const auto& [storage_key, metadata] : metadata_map) {
    const std::string& client_tag_hash = metadata->client_tag_hash();
    if (client_tag_hashes.find(client_tag_hash) != client_tag_hashes.end()) {
      count++;
    }
    client_tag_hashes.insert(client_tag_hash);
  }
  return count;
}

void RecordDataTypeNumUnsyncedEntitiesOnModelReady(
    DataType data_type,
    const ProcessorEntityTracker& entity_tracker) {
  size_t num_unsynced_entities = 0;
  for (const auto* entity :
       entity_tracker.GetAllEntitiesIncludingTombstones()) {
    if (entity->IsUnsynced()) {
      num_unsynced_entities++;
    }
  }
  SyncRecordDataTypeNumUnsyncedEntitiesOnModelReady(data_type,
                                                    num_unsynced_entities);
}

// Returns true if the unique position for the `target_entity` should be reused.
// The entity's position is compared with `position_before` and `position_after`
// (any of them can be invalid) to verify its order.
bool ShouldReuseTrackedUniquePositionFor(const ProcessorEntity* target_entity,
                                         const UniquePosition& position_before,
                                         const UniquePosition& position_after) {
  if (!target_entity || target_entity->metadata().is_deleted()) {
    return false;
  }

  UniquePosition target_entity_position =
      UniquePosition::FromProto(target_entity->metadata().unique_position());
  if (!target_entity_position.IsValid()) {
    // Do not CHECK if the unique position is invalid, and generate a new
    // position instead. This could be the case when some data type did not use
    // unique positions before and hence they don't have it for already-existing
    // entities.
    return false;
  }

  // Check that the existing unique position is in correct order to neighbours.
  if (position_after.IsValid() &&
      position_after.LessThan(target_entity_position)) {
    return false;
  }

  if (position_before.IsValid() &&
      target_entity_position.LessThan(position_before)) {
    return false;
  }

  return true;
}

}  // namespace

ClientTagBasedDataTypeProcessor::ClientTagBasedDataTypeProcessor(
    DataType type,
    const base::RepeatingClosure& dump_stack)
    : type_(type), dump_stack_(dump_stack) {}

ClientTagBasedDataTypeProcessor::~ClientTagBasedDataTypeProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ClientTagBasedDataTypeProcessor::OnSyncStarting(
    const DataTypeActivationRequest& request,
    StartCallback start_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Sync is starting for " << DataTypeToDebugString(type_);
  DUMP_WILL_BE_CHECK(request.IsValid()) << DataTypeToDebugString(type_);
  DUMP_WILL_BE_CHECK(start_callback) << DataTypeToDebugString(type_);
  DUMP_WILL_BE_CHECK(!start_callback_) << DataTypeToDebugString(type_);
  DUMP_WILL_BE_CHECK(!IsConnected()) << DataTypeToDebugString(type_);

  start_callback_ = std::move(start_callback);
  activation_request_ = request;

  // Notify the bridge sync is starting before calling the |start_callback_|
  // which in turn creates the worker.
  bridge_->OnSyncStarting(request);

  ConnectIfReady();
}

void ClientTagBasedDataTypeProcessor::OnModelStarting(
    DataTypeSyncBridge* bridge) {
  DUMP_WILL_BE_CHECK(bridge);
  bridge_ = bridge;
}

void ClientTagBasedDataTypeProcessor::ModelReadyToSync(
    std::unique_ptr<MetadataBatch> batch) {
  TRACE_EVENT0("sync", "ClientTagBasedDataTypeProcessor::ModelReadyToSync");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(!entity_tracker_);
  DUMP_WILL_BE_CHECK(!model_ready_to_sync_);

  model_ready_to_sync_ = true;

  // The model already experienced an error; abort.
  if (model_error_) {
    return;
  }

  if (ClearPersistedMetadataIfInvalid(*batch)) {
    DLOG(ERROR) << "The persisted metadata was invalid and was cleared for "
                << DataTypeToDebugString(type_) << ". Start over fresh.";
  } else {
    sync_pb::DataTypeState data_type_state = batch->GetDataTypeState();
    if (IsInitialSyncAtLeastPartiallyDone(
            data_type_state.initial_sync_state())) {
      entity_tracker_ = std::make_unique<ProcessorEntityTracker>(
          data_type_state, batch->TakeAllMetadata());
      RecordDataTypeNumUnsyncedEntitiesOnModelReady(type_, *entity_tracker_);
    } else {
      // If initial sync isn't done, there must be no entity metadata (if there
      // was, ClearPersistedMetadataIfInvalid() would've detected the
      // inconsistency).
      DUMP_WILL_BE_CHECK(batch->GetAllMetadata().empty());
    }
  }

  DUMP_WILL_BE_CHECK(model_ready_to_sync_);
  ConnectIfReady();
}

bool ClientTagBasedDataTypeProcessor::IsAllowingChanges() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Changes can be handled correctly even before pending data is loaded.
  return model_ready_to_sync_;
}

void ClientTagBasedDataTypeProcessor::ConnectIfReady() {
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
  DUMP_WILL_BE_CHECK(!pending_clear_metadata_);

  ClearPersistedMetadataIfInconsistentWithActivationRequest();

  auto activation_response = std::make_unique<DataTypeActivationResponse>();
  if (!entity_tracker_) {
    sync_pb::DataTypeState data_type_state;
    data_type_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(type_));
    data_type_state.set_cache_guid(activation_request_.cache_guid);
    data_type_state.set_authenticated_account_id(
        activation_request_.authenticated_account_id.ToString());
    // For passwords, the bridge re-downloads all passwords to obtain any
    // potential notes from the sync server that were ignored by earlier
    // versions of the browser that didn't support notes. This should be done
    // first when the browser is upgraded to a version that support passwords
    // notes. Store in the data type store that this redownload has happened
    // already to ensure it happens only once.
    if (type_ == PASSWORDS) {
      data_type_state.set_notes_enabled_before_initial_sync_for_passwords(true);
    }

    if (CommitOnlyTypes().Has(type_)) {
      // For commit-only types, no updates are expected.
      data_type_state.set_initial_sync_state(
          sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_UNNECESSARY);
      OnFullUpdateReceived(data_type_state, UpdateResponseDataList(),
                           /*gc_directive=*/std::nullopt);
      DUMP_WILL_BE_CHECK(entity_tracker_);
    } else {
      activation_response->data_type_state = data_type_state;
    }
  }

  if (entity_tracker_) {
    activation_response->data_type_state = entity_tracker_->data_type_state();
  }

  DUMP_WILL_BE_CHECK_EQ(activation_response->data_type_state.cache_guid(),
                        activation_request_.cache_guid);

  activation_response->type_processor =
      std::make_unique<DataTypeProcessorProxy>(
          weak_ptr_factory_for_worker_.GetWeakPtr(),
          base::SequencedTaskRunner::GetCurrentDefault());

  // Defer invoking of |start_callback_| to avoid synchronous call from the
  // |bridge_|. It might cause a situation when inside the ModelReadyToSync()
  // another methods of the bridge eventually were called. This behavior would
  // be complicated and be unexpected in some bridges.
  // See crbug.com/1055584 for more details.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(start_callback_),
                                std::move(activation_response)));
}

bool ClientTagBasedDataTypeProcessor::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !!worker_;
}

void ClientTagBasedDataTypeProcessor::OnSyncStopping(
    SyncStopMetadataFate metadata_fate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Disabling sync for a type never happens before the model is ready to sync.
  DUMP_WILL_BE_CHECK(model_ready_to_sync_);
  DUMP_WILL_BE_CHECK(!start_callback_);

  // Reset `activation_request_`. This acts as a flag that the processor has
  // been stopped or has not been started yet. Note: this avoids calling
  // bridge's OnSyncStarting() from ClearAllTrackedMetadataAndResetState().
  activation_request_ = DataTypeActivationRequest{};

  switch (metadata_fate) {
    case KEEP_METADATA: {
      bridge_->OnSyncPaused();
      // The model is still ready to sync (with the same |bridge_|) and same
      // sync metadata.
      ResetState(KEEP_METADATA);
      DUMP_WILL_BE_CHECK(model_ready_to_sync_);
      break;
    }

    case CLEAR_METADATA: {
      ClearAllTrackedMetadataAndResetState();
      DUMP_WILL_BE_CHECK(model_ready_to_sync_);
      break;
    }
  }

  DUMP_WILL_BE_CHECK(!IsConnected());
}

void ClientTagBasedDataTypeProcessor::ClearAllTrackedMetadataAndResetState() {
  std::unique_ptr<MetadataChangeList> change_list;

  // All changes before the initial sync is done are ignored and in fact they
  // were never persisted by the bridge (prior to MergeFullSyncData), so no
  // entities should be tracking.
  //
  // Clear metadata if MergeFullSyncData() was called before.
  if (entity_tracker_) {
    change_list = bridge_->CreateMetadataChangeList();
    std::vector<const ProcessorEntity*> entities =
        entity_tracker_->GetAllEntitiesIncludingTombstones();
    for (const ProcessorEntity* entity : entities) {
      change_list->ClearMetadata(entity->storage_key());
    }
    change_list->ClearDataTypeState();
  }

  ClearAllMetadataAndResetStateImpl(std::move(change_list));
}

void ClientTagBasedDataTypeProcessor::ClearAllProvidedMetadataAndResetState(
    const EntityMetadataMap& metadata_map) {
  std::unique_ptr<MetadataChangeList> change_list =
      bridge_->CreateMetadataChangeList();
  for (const auto& [storage_key, metadata] : metadata_map) {
    change_list->ClearMetadata(storage_key);
  }
  change_list->ClearDataTypeState();

  ClearAllMetadataAndResetStateImpl(std::move(change_list));
}

void ClientTagBasedDataTypeProcessor::ClearAllMetadataAndResetStateImpl(
    std::unique_ptr<MetadataChangeList> change_list) {
  if (change_list) {
    bridge_->ApplyDisableSyncChanges(std::move(change_list));
  } else {
    // TODO(crbug.com/40900767): This mimics the behavior previous to
    // https://crrev.com/c/4372288 but is quite questionable: it means sync was
    // disabled before the initial download completed, which has nothing to do
    // with sync-paused.
    bridge_->OnSyncPaused();
  }

  // Reset all the internal state of the processor.
  ResetState(CLEAR_METADATA);

  if (activation_request_.IsValid()) {
    // If OnSyncStarting() already was called, notify the bridge again.
    bridge_->OnSyncStarting(activation_request_);
  }
}

bool ClientTagBasedDataTypeProcessor::IsTrackingMetadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_tracker_ != nullptr;
}

std::string ClientTagBasedDataTypeProcessor::TrackedAccountId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Returning non-empty here despite !IsTrackingMetadata() has weird semantics,
  // e.g. initial updates are being fetched but we haven't received the response
  // (i.e. prior to exercising MergeFullSyncData()). Let's be cautious and hide
  // the account ID.
  if (!IsTrackingMetadata()) {
    return "";
  }
  return entity_tracker_->data_type_state().authenticated_account_id();
}

std::string ClientTagBasedDataTypeProcessor::TrackedCacheGuid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Returning non-empty here despite !IsTrackingMetadata() has weird semantics,
  // e.g. initial updates are being fetched but we haven't received the response
  // (i.e. prior to exercising MergeFullSyncData()). Let's be cautious and hide
  // the cache GUID.
  if (!IsTrackingMetadata()) {
    return "";
  }
  return entity_tracker_->data_type_state().cache_guid();
}

void ClientTagBasedDataTypeProcessor::ReportError(const ModelError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReportErrorImpl(error, ErrorSite::kReportedByBridge);
}

void ClientTagBasedDataTypeProcessor::ReportErrorImpl(const ModelError& error,
                                                      ErrorSite site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ignore all errors after the first.
  if (model_error_) {
    return;
  }

  const std::string type_suffix = DataTypeToHistogramSuffix(type_);
  base::UmaHistogramEnumeration(kErrorSiteHistogramPrefix + type_suffix, site);

  if (dump_stack_) {
    // Upload a stack trace if possible.
    dump_stack_.Run();
  }

  if (IsConnected()) {
    DisconnectSync();
  } else {
    // There could be in-flight connection requests that would eventually invoke
    // ConnectSync(), unless cancelled here.
    weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  }

  model_error_ = error;

  // Shouldn't connect anymore.
  start_callback_.Reset();
  if (activation_request_.IsValid()) {
    // Tell sync about the error.
    activation_request_.error_handler.Run(error);
  }
  // If the error handler isn't ready yet, we defer reporting the error until it
  // becomes available which happens in ConnectIfReady() upon OnSyncStarting().
}

std::optional<ModelError> ClientTagBasedDataTypeProcessor::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_error_;
}

base::WeakPtr<DataTypeControllerDelegate>
ClientTagBasedDataTypeProcessor::GetControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

void ClientTagBasedDataTypeProcessor::ConnectSync(
    std::unique_ptr<CommitQueue> worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(!model_error_);

  DVLOG(1) << "Successfully connected " << DataTypeToDebugString(type_);

  worker_ = std::move(worker);

  NudgeForCommitIfNeeded();
}

void ClientTagBasedDataTypeProcessor::DisconnectSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(IsConnected());

  DVLOG(1) << "Disconnecting sync for " << DataTypeToDebugString(type_);
  weak_ptr_factory_for_worker_.InvalidateWeakPtrs();
  worker_.reset();

  if (entity_tracker_) {
    entity_tracker_->ClearTransientSyncState();
  }
}

void ClientTagBasedDataTypeProcessor::Put(
    const std::string& storage_key,
    std::unique_ptr<EntityData> data,
    MetadataChangeList* metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(IsAllowingChanges());
  DUMP_WILL_BE_CHECK(data);
  DUMP_WILL_BE_CHECK(!data->is_deleted());
  DUMP_WILL_BE_CHECK(!data->specifics.has_encrypted());
  DUMP_WILL_BE_CHECK(!storage_key.empty());
  DUMP_WILL_BE_CHECK_EQ(type_, GetDataTypeFromSpecifics(data->specifics));

  if (!entity_tracker_) {
    // Ignore changes before the initial sync is done.
    return;
  }

  // Local changes based on remote update is discouraged because it may lead to
  // ping-pong issues between clients and result in uncontrolled traffic to the
  // server.
  // TODO(crbug.com/40927082): this could be a CHECK instead, add a metric to
  // find out first which data types have such behavior.
  if (processing_incremental_updates_) {
    base::UmaHistogramEnumeration("Sync.LocalChangeDuringRemoteUpdate",
                                  DataTypeHistogramValue(type_));
  }

  // |data->specifics| is about to be committed, and therefore represents the
  // imminent server-side state in most cases.
  sync_pb::EntitySpecifics trimmed_specifics =
      bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(data->specifics);

  // Extract `unique_position` before the `data` is moved.
  std::optional<sync_pb::UniquePosition> unique_position;
  if (bridge_->SupportsUniquePositions()) {
    unique_position = bridge_->GetUniquePosition(data->specifics);
  }

  ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    // The bridge is creating a new entity. The bridge may or may not populate
    // |data->client_tag_hash|, so let's ask for the client tag if needed.
    if (data->client_tag_hash.value().empty()) {
      DUMP_WILL_BE_CHECK(bridge_->SupportsGetClientTag());
      data->client_tag_hash =
          ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(*data));
    } else if (bridge_->SupportsGetClientTag()) {
      // If the Put() call already included the client tag, let's verify that
      // it's consistent with the bridge's regular GetClientTag() function (if
      // supported by the bridge).
      DUMP_WILL_BE_CHECK_EQ(
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
      DUMP_WILL_BE_CHECK(storage_key != entity->storage_key());
      if (!entity->metadata().is_deleted()) {
        // The bridge overrides an entity that is not deleted. This is
        // unexpected but the processor tolerates it. It is very likely a
        // metadata orphan; report it to metrics.
        UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeOrphanMetadata.Put",
                                  DataTypeHistogramValue(type_));
      }
      // Remove the old storage key from the tracker and the corresponding
      // metadata record.
      metadata_change_list->ClearMetadata(entity->storage_key());
      entity_tracker_->UpdateOrOverrideStorageKey(data->client_tag_hash,
                                                  storage_key);
      entity->RecordLocalUpdate(std::move(data), std::move(trimmed_specifics),
                                std::move(unique_position));
    } else {
      if (data->creation_time.is_null()) {
        data->creation_time = base::Time::Now();
      }
      if (data->modification_time.is_null()) {
        data->modification_time = data->creation_time;
      }

      entity = entity_tracker_->AddUnsyncedLocal(storage_key, std::move(data),
                                                 std::move(trimmed_specifics),
                                                 std::move(unique_position));
    }
  } else if (entity->MatchesData(*data)) {
    // Ignore changes that don't actually change anything.
    return;
  } else {
    entity->RecordLocalUpdate(std::move(data), std::move(trimmed_specifics),
                              std::move(unique_position));
  }

  DUMP_WILL_BE_CHECK(entity->IsUnsynced());

  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());

  NudgeForCommitIfNeeded();
}

void ClientTagBasedDataTypeProcessor::Delete(
    const std::string& storage_key,
    const DeletionOrigin& origin,
    MetadataChangeList* metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(IsAllowingChanges());

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

  if (entity->RecordLocalDeletion(origin)) {
    metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
  } else {
    RemoveEntity(entity->storage_key(), metadata_change_list);
  }

  NudgeForCommitIfNeeded();
}

void ClientTagBasedDataTypeProcessor::UpdateStorageKey(
    const EntityData& entity_data,
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  const ClientTagHash& client_tag_hash = entity_data.client_tag_hash;
  DUMP_WILL_BE_CHECK(!client_tag_hash.value().empty());
  DUMP_WILL_BE_CHECK(!storage_key.empty());
  DUMP_WILL_BE_CHECK(!bridge_->SupportsGetStorageKey());
  DUMP_WILL_BE_CHECK(entity_tracker_);

  const ProcessorEntity* entity =
      entity_tracker_->GetEntityForTagHash(client_tag_hash);
  DUMP_WILL_BE_CHECK(entity);

  DUMP_WILL_BE_CHECK(entity->storage_key().empty());
  entity_tracker_->UpdateOrOverrideStorageKey(client_tag_hash, storage_key);

  metadata_change_list->UpdateMetadata(storage_key, entity->metadata());
}

void ClientTagBasedDataTypeProcessor::UntrackEntityForStorageKey(
    const std::string& storage_key) {
  if (!entity_tracker_) {
    // Ignore changes before the initial sync is done.
    return;
  }
  entity_tracker_->RemoveEntityForStorageKey(storage_key);
}

void ClientTagBasedDataTypeProcessor::UntrackEntityForClientTagHash(
    const ClientTagHash& client_tag_hash) {
  DUMP_WILL_BE_CHECK(!client_tag_hash.value().empty());
  if (!entity_tracker_) {
    // Ignore changes before the initial sync is done.
    return;
  }
  entity_tracker_->RemoveEntityForClientTagHash(client_tag_hash);
}

std::vector<std::string>
ClientTagBasedDataTypeProcessor::GetAllTrackedStorageKeys() const {
  std::vector<std::string> storage_keys;
  if (entity_tracker_) {
    for (const ProcessorEntity* entity :
         entity_tracker_->GetAllEntitiesIncludingTombstones()) {
      storage_keys.push_back(entity->storage_key());
    }
  }
  return storage_keys;
}

bool ClientTagBasedDataTypeProcessor::IsEntityUnsynced(
    const std::string& storage_key) const {
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

base::Time ClientTagBasedDataTypeProcessor::GetEntityCreationTime(
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

base::Time ClientTagBasedDataTypeProcessor::GetEntityModificationTime(
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

void ClientTagBasedDataTypeProcessor::NudgeForCommitIfNeeded() {
  // Don't bother sending anything if there's no one to send to.
  if (!IsConnected()) {
    return;
  }

  // Don't send anything if the type is not ready to handle commits.
  if (!entity_tracker_) {
    return;
  }

  // Nudge worker if there are any entities with local changes.0
  if (entity_tracker_->HasLocalChanges()) {
    worker_->NudgeForCommit();
  }
}

void ClientTagBasedDataTypeProcessor::GetLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK_GT(max_entries, 0U);
  DUMP_WILL_BE_CHECK(IsConnected());
  DUMP_WILL_BE_CHECK(!model_error_);

  // Use base::debug::Alias() to ensure that crash dumps in reports include
  // DataType.
  const DataType data_type = type_;
  base::debug::Alias(&data_type);

  // In some cases local changes may be requested before entity tracker is
  // loaded. Just invoke the callback with empty list.
  if (!entity_tracker_) {
    std::move(callback).Run(CommitRequestDataList());
    return;
  }

  std::vector<const ProcessorEntity*> entities =
      entity_tracker_->GetAllEntitiesIncludingTombstones();
  std::vector<std::string> entities_requiring_data;
  for (const ProcessorEntity* entity : entities) {
    if (entity->RequiresCommitData()) {
      entities_requiring_data.push_back(entity->storage_key());
    }
  }
  if (!entities_requiring_data.empty()) {
    // Make a copy to later check if everything was loaded successfully.
    std::unordered_set<std::string> storage_keys_to_load(
        entities_requiring_data.begin(), entities_requiring_data.end());
    std::unique_ptr<DataBatch> data_batch =
        bridge_->GetDataForCommit(std::move(entities_requiring_data));
    // The `GetDataForCommit` call may have produced a `model_error_` (if the
    // bridge called `ReportError`), in which case the `data_batch` should also
    // be null.
    if (model_error_) {
      return;
    }
    CHECK(data_batch);
    ConsumeDataBatch(std::move(storage_keys_to_load), std::move(data_batch));
  }

  CommitLocalChanges(max_entries, std::move(callback));
}

void ClientTagBasedDataTypeProcessor::OnCommitCompleted(
    const sync_pb::DataTypeState& data_type_state,
    const CommitResponseDataList& committed_response_list,
    const FailedCommitResponseDataList& error_response_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(IsConnected());
  DUMP_WILL_BE_CHECK(!model_error_);

  DUMP_WILL_BE_CHECK(entity_tracker_)
      << "Received commit response when entity tracker is null. Type: "
      << DataTypeToDebugString(type_);

  // Use base::debug::Alias() to ensure that crash dumps in reports include
  // DataType.
  const DataType data_type = type_;
  base::debug::Alias(&data_type);

  // |error_response_list| is ignored, because all errors are treated as
  // transientand the processor with eventually retry.

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      bridge_->CreateMetadataChangeList();
  EntityChangeList entity_change_list;

  entity_tracker_->set_data_type_state(data_type_state);
  metadata_change_list->UpdateDataTypeState(entity_tracker_->data_type_state());

  for (const CommitResponseData& data : committed_response_list) {
    ProcessorEntity* entity =
        entity_tracker_->GetEntityForTagHash(data.client_tag_hash);
    if (entity == nullptr) {
      // This can happen (rarely) if the entity got untracked while a Commit was
      // ongoing, or if the server sent a bogus response (unlikely).
      DLOG(ERROR) << "Received commit response for missing item."
                  << " type: " << DataTypeToDebugString(type_)
                  << " client_tag_hash: " << data.client_tag_hash;
      base::UmaHistogramEnumeration("Sync.CommitResponseForUnknownEntity",
                                    DataTypeHistogramValue(type_));
      continue;
    }

    entity->ReceiveCommitResponse(data, CommitOnlyTypes().Has(type_));

    if (CommitOnlyTypes().Has(type_)) {
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
  // TODO(crbug.com/41329567): Iterating over all entities is inefficient. It is
  // better to remember in GetLocalChanges which entities are being committed
  // and adjust only them. Alternatively we can make worker return commit status
  // for all entities, not just successful ones and use that to lookup entities
  // to clear.
  entity_tracker_->ClearTransientSyncState();

  std::optional<ModelError> error = bridge_->ApplyIncrementalSyncChanges(
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
bool HasClearAllDirective(
    const std::optional<sync_pb::GarbageCollectionDirective>& gc_directive) {
  return gc_directive.has_value() && gc_directive->has_version_watermark();
}

void ClientTagBasedDataTypeProcessor::OnCommitFailed(
    SyncCommitError commit_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(IsConnected());
  DUMP_WILL_BE_CHECK(!model_error_);

  // Use base::debug::Alias() to ensure that crash dumps in reports include
  // DataType.
  const DataType data_type = type_;
  base::debug::Alias(&data_type);

  switch (bridge_->OnCommitAttemptFailed(commit_error)) {
    case DataTypeSyncBridge::CommitAttemptFailedBehavior::
        kShouldRetryOnNextCycle:
      // Entities weren't committed. Reset their
      // |commit_requested_sequence_number| to commit them again on next sync
      // cycle.
      entity_tracker_->ClearTransientSyncState();
      break;
    case DataTypeSyncBridge::CommitAttemptFailedBehavior::kDontRetryOnNextCycle:
      // Do nothing and leave all entities in a transient state.
      break;
  }
}

void ClientTagBasedDataTypeProcessor::OnUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DUMP_WILL_BE_CHECK(model_ready_to_sync_);
  DUMP_WILL_BE_CHECK(IsConnected());
  DUMP_WILL_BE_CHECK(!model_error_);
  DUMP_WILL_BE_CHECK(!data_type_state.progress_marker().has_gc_directive());

  // Use base::debug::Alias() to ensure that crash dumps in reports include
  // DataType.
  const DataType data_type = type_;
  base::debug::Alias(&data_type);

  if (!ValidateUpdate(data_type_state, updates, gc_directive)) {
    return;
  }

  std::optional<ModelError> error;

  // We call OnFullUpdateReceived when it's the first sync cycle, or when
  // we get a garbage collection directive from the server telling us to clear
  // all data by version watermark.
  // This means that if we receive a version watermark based GC directive, we
  // always clear all data. We do this to allow the server to replace all data
  // on the client, without having to know exactly which entities the client
  // has.
  const bool is_initial_sync = !IsTrackingMetadata();
  const bool treating_as_full_update =
      is_initial_sync || HasClearAllDirective(gc_directive);
  if (treating_as_full_update) {
    error = OnFullUpdateReceived(data_type_state, std::move(updates),
                                 std::move(gc_directive));
  } else {
    error = OnIncrementalUpdateReceived(data_type_state, std::move(updates),
                                        std::move(gc_directive));
  }

  if (error) {
    ReportErrorImpl(*error, treating_as_full_update
                                ? ErrorSite::kApplyFullUpdates
                                : ErrorSite::kApplyIncrementalUpdates);
    return;
  }

  if (is_initial_sync) {
    LogDataTypeConfigurationTime(type_, activation_request_.sync_mode,
                                 activation_request_.configuration_start_time);
  }

  DUMP_WILL_BE_CHECK(entity_tracker_);
  // If there were entities with empty storage keys, they should have been
  // updated by bridge as part of ApplyIncrementalSyncChanges.
  DUMP_WILL_BE_CHECK(entity_tracker_->AllStorageKeysPopulated());
  // There may be new reasons to commit by the time this function is done.
  NudgeForCommitIfNeeded();
}

void ClientTagBasedDataTypeProcessor::StorePendingInvalidations(
    std::vector<sync_pb::DataTypeState::Invalidation> invalidations_to_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsConnected());
  CHECK(bridge_);

  // Use base::debug::Alias() to ensure that crash dumps in reports include
  // DataType.
  const DataType data_type = type_;
  base::debug::Alias(&data_type);

  if (model_error_ || !entity_tracker_) {
    // It's possible to have incoming invalidations while the data type is not
    // fully initialized (e.g. before the initial sync).
    return;
  }

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  sync_pb::DataTypeState data_type_state = entity_tracker_->data_type_state();
  data_type_state.mutable_invalidations()->Assign(
      invalidations_to_store.begin(), invalidations_to_store.end());
  metadata_changes->UpdateDataTypeState(data_type_state);
  entity_tracker_->set_data_type_state(data_type_state);
  bridge_->ApplyIncrementalSyncChanges(std::move(metadata_changes),
                                       EntityChangeList());
}

bool ClientTagBasedDataTypeProcessor::ValidateUpdate(
    const sync_pb::DataTypeState& data_type_state,
    const UpdateResponseDataList& updates,
    const std::optional<sync_pb::GarbageCollectionDirective>& gc_directive) {
  if (!entity_tracker_) {
    // Due to uss_migrator, initial sync (when migrating from non-USS) does not
    // contain any gc directives. Thus, we cannot expect the conditions below to
    // be satisfied. It is okay to skip the check as for an initial sync, the gc
    // directive does not make any semantical difference.
    return true;
  }

  if (HasClearAllDirective(gc_directive) &&
      bridge_->SupportsIncrementalUpdates()) {
    ReportErrorImpl(ModelError(FROM_HERE,
                               "Received an update with version watermark for "
                               "bridge that supports incremental updates"),
                    ErrorSite::kSupportsIncrementalUpdatesMismatch);

    return false;
  } else if (!HasClearAllDirective(gc_directive) &&
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

std::optional<ModelError> ClientTagBasedDataTypeProcessor::OnFullUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  DUMP_WILL_BE_CHECK(model_ready_to_sync_);

  // Check that the worker correctly marked initial sync as (at least) partially
  // done for this update.
  DUMP_WILL_BE_CHECK(IsInitialSyncDone(data_type_state.initial_sync_state()) ||
                     (ApplyUpdatesImmediatelyTypes().Has(type_) &&
                      IsInitialSyncAtLeastPartiallyDone(
                          data_type_state.initial_sync_state())));

  // Ensure that this is the initial sync, and it was not already marked done.
  DUMP_WILL_BE_CHECK(HasClearAllDirective(gc_directive) || !entity_tracker_);

  if (entity_tracker_ && HasClearAllDirective(gc_directive)) {
    ExpireAllEntries(metadata_changes.get());
    entity_tracker_->set_data_type_state(data_type_state);
  }

  if (!entity_tracker_) {
    entity_tracker_ = std::make_unique<ProcessorEntityTracker>(
        data_type_state, EntityMetadataMap());
  }

  // TODO(crbug.com/40668179): the comment below may be wrong in case where a
  // datatype supports non-incremental updates and local updates are
  // acceptable.
  // Given that we either just removed all existing sync entities (in the full
  // update case).
  DUMP_WILL_BE_CHECK(!entity_tracker_->size());

  metadata_changes->UpdateDataTypeState(entity_tracker_->data_type_state());

  EntityChangeList entity_data;
  for (syncer::UpdateResponseData& update : updates) {
    const ClientTagHash& client_tag_hash = update.entity.client_tag_hash;
    if (client_tag_hash.value().empty()) {
      // Ignore updates missing a client tag hash (e.g. permanent nodes).
      continue;
    }
    if (update.entity.is_deleted()) {
      SyncRecordDataTypeUpdateDropReason(
          UpdateDropReason::kTombstoneInFullUpdate, type_);
      DLOG(WARNING) << "Ignoring tombstone found during initial update: "
                    << "client_tag_hash = " << client_tag_hash << " for "
                    << DataTypeToDebugString(type_);
      continue;
    }

    if (!bridge_->IsEntityDataValid(update.entity)) {
      SyncRecordDataTypeUpdateDropReason(UpdateDropReason::kDroppedByBridge,
                                         type_);
      DLOG(WARNING) << "Received entity with invalid update for "
                    << DataTypeToDebugString(type_);
      continue;
    }

    if (bridge_->SupportsGetClientTag() &&
        client_tag_hash != ClientTagHash::FromUnhashed(
                               type_, bridge_->GetClientTag(update.entity))) {
      SyncRecordDataTypeUpdateDropReason(
          UpdateDropReason::kInconsistentClientTag, type_);
      DLOG(WARNING) << "Received unexpected client tag hash: "
                    << client_tag_hash << " for "
                    << DataTypeToDebugString(type_);
      continue;
    }

    std::string storage_key;
    if (bridge_->SupportsGetStorageKey()) {
      storage_key = bridge_->GetStorageKey(update.entity);
      // TODO(crbug.com/40677711): Make this a DUMP_WILL_BE_CHECK as storage
      // keys should not be empty after IsEntityDataValid() has been implemented
      // by all bridges.
      if (storage_key.empty()) {
        SyncRecordDataTypeUpdateDropReason(
            UpdateDropReason::kCannotGenerateStorageKey, type_);
        DLOG(WARNING) << "Received entity with invalid update for "
                      << DataTypeToDebugString(type_);
        continue;
      }
    }
#if DCHECK_IS_ON()
    // TODO(crbug.com/41406929): The CreateEntity() call below assumes that no
    // entity with this client_tag_hash exists already, but in some cases it
    // does.
    if (entity_tracker_->GetEntityForTagHash(client_tag_hash)) {
      DLOG(ERROR) << "Received duplicate client_tag_hash " << client_tag_hash
                  << " for " << DataTypeToDebugString(type_);
    }
#endif  // DCHECK_IS_ON()
    std::optional<sync_pb::UniquePosition> unique_position;
    if (bridge_->SupportsUniquePositions()) {
      unique_position = bridge_->GetUniquePosition(update.entity.specifics);
    }
    ProcessorEntity* entity = entity_tracker_->AddRemote(
        storage_key, update,
        bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(
            update.entity.specifics),
        std::move(unique_position));
    entity_data.push_back(
        EntityChange::CreateAdd(storage_key, std::move(update.entity)));
    if (!storage_key.empty()) {
      metadata_changes->UpdateMetadata(storage_key, entity->metadata());
    }
  }

  // If there is already an error (this can happen if one of the metadata
  // writes failed), don't even send the data to the bridge.
  if (model_error_) {
    return model_error_;
  }

  // Let the bridge handle associating and merging the data.
  // For ApplyUpdatesImmediatelyTypes(), no merge is necessary or supported, so
  // call ApplyIncrementalSyncChanges() instead.
  if (ApplyUpdatesImmediatelyTypes().Has(type_)) {
    return bridge_->ApplyIncrementalSyncChanges(std::move(metadata_changes),
                                                std::move(entity_data));
  }

  return bridge_->MergeFullSyncData(std::move(metadata_changes),
                                    std::move(entity_data));
}

std::optional<ModelError>
ClientTagBasedDataTypeProcessor::OnIncrementalUpdateReceived(
    const sync_pb::DataTypeState& data_type_state,
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  DUMP_WILL_BE_CHECK(model_ready_to_sync_);
  DUMP_WILL_BE_CHECK(IsInitialSyncDone(data_type_state.initial_sync_state()) ||
                     (ApplyUpdatesImmediatelyTypes().Has(type_) &&
                      IsInitialSyncAtLeastPartiallyDone(
                          data_type_state.initial_sync_state())));
  DUMP_WILL_BE_CHECK(entity_tracker_);

  ClientTagBasedRemoteUpdateHandler updates_handler(type_, bridge_,
                                                    entity_tracker_.get());
  base::AutoReset<bool> auto_reset_processing_updates(
      &processing_incremental_updates_, true);
  return updates_handler.ProcessIncrementalUpdate(
      data_type_state, std::move(updates), std::move(gc_directive));
}

void ClientTagBasedDataTypeProcessor::ConsumeDataBatch(
    std::unordered_set<std::string> storage_keys_to_load,
    std::unique_ptr<DataBatch> data_batch) {
  DUMP_WILL_BE_CHECK(entity_tracker_);
  while (data_batch->HasNext()) {
    auto [storage_key, data] = data_batch->Next();

    storage_keys_to_load.erase(storage_key);
    ProcessorEntity* entity =
        entity_tracker_->GetEntityForStorageKey(storage_key);
    // If the entity wasn't deleted or updated with new commit.
    if (entity != nullptr && entity->RequiresCommitData()) {
      // SetCommitData will update EntityData's fields with values from
      // metadata.
      entity->SetCommitData(std::move(data));
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
    UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeOrphanMetadata.GetData",
                              DataTypeHistogramValue(type_));
  }

  if (storage_keys_to_untrack.empty()) {
    return;
  }

  DUMP_WILL_BE_CHECK(model_ready_to_sync_);
  DUMP_WILL_BE_CHECK(IsTrackingMetadata());

  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();

  for (const std::string& storage_key : storage_keys_to_untrack) {
    UntrackEntityForStorageKey(storage_key);
    metadata_changes->ClearMetadata(storage_key);
  }

  bridge_->ApplyIncrementalSyncChanges(std::move(metadata_changes),
                                       EntityChangeList());
}

void ClientTagBasedDataTypeProcessor::CommitLocalChanges(
    size_t max_entries,
    GetLocalChangesCallback callback) {
  DUMP_WILL_BE_CHECK(!model_error_);
  DUMP_WILL_BE_CHECK(entity_tracker_);
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

size_t ClientTagBasedDataTypeProcessor::EstimateMemoryUsage() const {
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

bool ClientTagBasedDataTypeProcessor::HasLocalChangesForTest() const {
  return entity_tracker_ && entity_tracker_->HasLocalChanges();
}

bool ClientTagBasedDataTypeProcessor::IsTrackingEntityForTest(
    const std::string& storage_key) const {
  return entity_tracker_ &&
         entity_tracker_->GetEntityForStorageKey(storage_key) != nullptr;
}

bool ClientTagBasedDataTypeProcessor::IsModelReadyToSyncForTest() const {
  return model_ready_to_sync_;
}

void ClientTagBasedDataTypeProcessor::ExpireAllEntries(
    MetadataChangeList* metadata_changes) {
  DUMP_WILL_BE_CHECK(metadata_changes);
  DUMP_WILL_BE_CHECK(entity_tracker_);

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

void ClientTagBasedDataTypeProcessor::RemoveEntity(
    const std::string& storage_key,
    MetadataChangeList* metadata_change_list) {
  DUMP_WILL_BE_CHECK(!storage_key.empty());
  DUMP_WILL_BE_CHECK(entity_tracker_);
  DUMP_WILL_BE_CHECK(entity_tracker_->GetEntityForStorageKey(storage_key));
  metadata_change_list->ClearMetadata(storage_key);
  entity_tracker_->RemoveEntityForStorageKey(storage_key);
}

void ClientTagBasedDataTypeProcessor::ResetState(
    SyncStopMetadataFate metadata_fate) {
  switch (metadata_fate) {
    case KEEP_METADATA:
      break;
    case CLEAR_METADATA:
      entity_tracker_.reset();
      break;
  }

  if (IsConnected()) {
    DisconnectSync();
  }
}

void ClientTagBasedDataTypeProcessor::HasUnsyncedData(
    base::OnceCallback<void(bool)> callback) {
  // Note that if there's a `model_error_`, there might be unsynced data that
  // remains unsynced indefinitely (at least until the next browser restart).
  std::move(callback).Run(entity_tracker_ &&
                          entity_tracker_->HasLocalChanges());
}

void ClientTagBasedDataTypeProcessor::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  if (!bridge_) {
    return;
  }

  std::unique_ptr<DataBatch> batch = bridge_->GetAllDataForDebugging();
  if (!batch) {
    std::move(callback).Run(base::Value::List());
    return;
  }

  base::Value::List all_nodes;
  std::string type_string = DataTypeToDebugString(type_);

  while (batch->HasNext()) {
    auto [storage_key, data] = batch->Next();

    // There is an overlap between EntityData fields from the bridge and
    // EntityMetadata fields from the processor's entity, metadata is
    // the authoritative source of truth.
    const ProcessorEntity* entity =
        entity_tracker_->GetEntityForStorageKey(storage_key);
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

    base::Value::Dict node = data->ToDictionaryValue();
    node.Set("dataType", type_string);
    // Copy the whole metadata message into the dictionary (if existing).
    if (entity != nullptr) {
      node.Set("metadata", EntityMetadataToValue(entity->metadata()));
    }
    all_nodes.Append(std::move(node));
  }

  // Create a permanent folder for this data type. Since sync server no longer
  // creates root folders, and USS won't migrate root folders from the
  // Directory, we create root folders for each data type here.

  // Function isTypeRootNode in sync_node_browser.js use PARENT_ID and
  // UNIQUE_SERVER_TAG to check if the node is root node. isChildOf in
  // sync_node_browser.js uses dataType to check if root node is parent of real
  // data node. NON_UNIQUE_NAME will be the name of node to display.
  auto rootnode = base::Value::Dict()
                      .Set("PARENT_ID", "r")
                      .Set("UNIQUE_SERVER_TAG", type_string)
                      .Set("IS_DIR", true)
                      .Set("dataType", type_string)
                      .Set("NON_UNIQUE_NAME", type_string);
  all_nodes.Append(std::move(rootnode));

  std::move(callback).Run(std::move(all_nodes));
}

bool ClientTagBasedDataTypeProcessor::ClearPersistedMetadataIfInvalid(
    const MetadataBatch& metadata) {
  // The entity tracker must not have been created before the metadata was
  // validated.
  CHECK(!entity_tracker_);

  const sync_pb::DataTypeState& data_type_state = metadata.GetDataTypeState();
  const EntityMetadataMap& metadata_map = metadata.GetAllMetadata();

  // Check if ClearMetadataIfStopped() was called before ModelReadyToSync().
  // If so, clear the metadata from storage (using the bridge's
  // ApplyDisableSyncChanges()).
  if (pending_clear_metadata_) {
    pending_clear_metadata_ = false;
    // Avoid calling the bridge if there's nothing to clear.
    if (data_type_state.ByteSizeLong() > 0 || !metadata_map.empty()) {
      LogClearMetadataWhileStoppedHistogram(type_, /*is_delayed_call=*/true);
      // This will incur an I/O operation by asking the bridge to clear the
      // metadata in storage.
      ClearAllProvidedMetadataAndResetState(metadata_map);
      // Not having `entity_tracker_` results in doing the initial sync again.
      CHECK(!entity_tracker_);
      return true;
    }
    // Else: There was nothing to clear.
    return false;
  }

  // Check that there's no entity metadata unless the initial sync is at least
  // started.
  if (!IsInitialSyncAtLeastPartiallyDone(
          data_type_state.initial_sync_state()) &&
      !metadata_map.empty()) {
    base::UmaHistogramEnumeration(
        "Sync.DataTypeEntityMetadataWithoutInitialSync",
        DataTypeHistogramValue(type_));

    ClearAllProvidedMetadataAndResetState(metadata_map);
    // Not having `entity_tracker_` results in doing the initial sync again.
    CHECK(!entity_tracker_);
    return true;
  }

  // Check that there are no duplicate client tags.
  size_t count_of_duplicates = CountDuplicateClientTags(metadata_map);
  if (count_of_duplicates > 0u) {
    // Metadata entities with duplicate client tag hashes most likely arise
    // from metadata orphans; report their count to metrics.
    for (size_t i = 0; i < count_of_duplicates; i++) {
      base::UmaHistogramEnumeration(
          "Sync.DataTypeOrphanMetadata.ModelReadyToSync",
          DataTypeHistogramValue(type_));
    }

    ClearAllProvidedMetadataAndResetState(metadata_map);
    // Not having `entity_tracker_` results in doing the initial sync again.
    CHECK(!entity_tracker_);
    return true;
  }

  return false;
}

void ClientTagBasedDataTypeProcessor::
    ClearPersistedMetadataIfInconsistentWithActivationRequest() {
  if (!entity_tracker_) {
    return;
  }
  const sync_pb::DataTypeState& data_type_state =
      entity_tracker_->data_type_state();

  // Check for a mismatch in authenticated account id. The id can change after
  // restart (and this does not mean the account has changed, this is checked
  // later here by cache_guid mismatch). Easy to fix in place.
  // TODO(crbug.com/40897441): This doesn't fit the method name. It's also not
  // clear if this codepath is even required
  if (data_type_state.authenticated_account_id() !=
      activation_request_.authenticated_account_id.ToString()) {
    sync_pb::DataTypeState update_data_type_state = data_type_state;
    update_data_type_state.set_authenticated_account_id(
        activation_request_.authenticated_account_id.ToString());
    entity_tracker_->set_data_type_state(update_data_type_state);
  }

  // Check for deeper issues where we need to restart sync for this type.
  const bool valid_cache_guid =
      data_type_state.cache_guid() == activation_request_.cache_guid;
  // Check for a mismatch between the cache guid or the data type id stored
  // in |data_type_state_| and the one received from sync. A mismatch indicates
  // that the stored metadata are invalid (e.g. has been manipulated) and
  // don't belong to the current syncing client.
  const bool valid_data_type_id =
      data_type_state.progress_marker().data_type_id() ==
      GetSpecificsFieldNumberFromDataType(type_);
  if (valid_cache_guid && valid_data_type_id) {
    return;
  }

  ClearAllTrackedMetadataAndResetState();
  // Not having `entity_tracker_` results in doing the initial sync again.
  DUMP_WILL_BE_CHECK(!entity_tracker_);
}

void ClientTagBasedDataTypeProcessor::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const TypeEntitiesCount&)> callback) const {
  TypeEntitiesCount count(type_);
  if (entity_tracker_) {
    count.entities = entity_tracker_->size();
    count.non_tombstone_entities = entity_tracker_->CountNonTombstoneEntries();
  }
  std::move(callback).Run(count);
}

void ClientTagBasedDataTypeProcessor::RecordMemoryUsageAndCountsHistograms() {
  SyncRecordDataTypeMemoryHistogram(type_, EstimateMemoryUsage());
  const size_t non_tombstone_entries_count =
      entity_tracker_ == nullptr ? 0
                                 : entity_tracker_->CountNonTombstoneEntries();
  SyncRecordDataTypeCountHistogram(type_, non_tombstone_entries_count);
}

const sync_pb::EntitySpecifics&
ClientTagBasedDataTypeProcessor::GetPossiblyTrimmedRemoteSpecifics(
    const std::string& storage_key) const {
  DUMP_WILL_BE_CHECK(entity_tracker_);
  DUMP_WILL_BE_CHECK(!storage_key.empty());

  ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
  if (entity == nullptr) {
    return sync_pb::EntitySpecifics::default_instance();
  }
  return entity->metadata().possibly_trimmed_base_specifics();
}

sync_pb::UniquePosition ClientTagBasedDataTypeProcessor::UniquePositionAfter(
    const std::string& storage_key_before,
    const ClientTagHash& target_client_tag_hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(entity_tracker_);

  UniquePosition position_before = UniquePosition::FromProto(
      GetUniquePositionForStorageKey(storage_key_before));
  if (!position_before.IsValid()) {
    return GenerateFallbackUniquePosition(target_client_tag_hash);
  }

  const ProcessorEntity* target_entity =
      entity_tracker_->GetEntityForTagHash(target_client_tag_hash);
  if (ShouldReuseTrackedUniquePositionFor(target_entity, position_before,
                                          UniquePosition())) {
    CHECK(target_entity);
    return target_entity->metadata().unique_position();
  }

  return UniquePosition::After(position_before, UniquePosition::GenerateSuffix(
                                                    target_client_tag_hash))
      .ToProto();
}

sync_pb::UniquePosition ClientTagBasedDataTypeProcessor::UniquePositionBefore(
    const std::string& storage_key_after,
    const ClientTagHash& target_client_tag_hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(entity_tracker_);

  UniquePosition position_after = UniquePosition::FromProto(
      GetUniquePositionForStorageKey(storage_key_after));
  if (!position_after.IsValid()) {
    return GenerateFallbackUniquePosition(target_client_tag_hash);
  }

  const ProcessorEntity* target_entity =
      entity_tracker_->GetEntityForTagHash(target_client_tag_hash);
  if (ShouldReuseTrackedUniquePositionFor(target_entity, UniquePosition(),
                                          position_after)) {
    CHECK(target_entity);
    return target_entity->metadata().unique_position();
  }

  return UniquePosition::Before(position_after, UniquePosition::GenerateSuffix(
                                                    target_client_tag_hash))
      .ToProto();
}

sync_pb::UniquePosition ClientTagBasedDataTypeProcessor::UniquePositionBetween(
    const std::string& storage_key_before,
    const std::string& storage_key_after,
    const ClientTagHash& target_client_tag_hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(entity_tracker_);

  UniquePosition position_before = UniquePosition::FromProto(
      GetUniquePositionForStorageKey(storage_key_before));
  UniquePosition position_after = UniquePosition::FromProto(
      GetUniquePositionForStorageKey(storage_key_after));
  if (!position_after.IsValid() || !position_before.IsValid()) {
    return GenerateFallbackUniquePosition(target_client_tag_hash);
  }

  if (!position_before.LessThan(position_after)) {
    // The order in sync metadata is incorrect due to inconsistency with the
    // model. This normally should not happen but generate some meaningful
    // unique position to prevent data loss (in case the bridge verifies unique
    // position validness).
    base::UmaHistogramEnumeration("Sync.DataTypeUniquePositionIncorrectOrder",
                                  DataTypeHistogramValue(type_));
    return UniquePosition::After(
               position_before,
               UniquePosition::GenerateSuffix(target_client_tag_hash))
        .ToProto();
  }

  const ProcessorEntity* target_entity =
      entity_tracker_->GetEntityForTagHash(target_client_tag_hash);
  if (ShouldReuseTrackedUniquePositionFor(target_entity, position_before,
                                          position_after)) {
    CHECK(target_entity);
    return target_entity->metadata().unique_position();
  }

  return UniquePosition::Between(
             position_before, position_after,
             UniquePosition::GenerateSuffix(target_client_tag_hash))
      .ToProto();
}

sync_pb::UniquePosition
ClientTagBasedDataTypeProcessor::UniquePositionForInitialEntity(
    const ClientTagHash& target_client_tag_hash) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(entity_tracker_);

  const ProcessorEntity* target_entity =
      entity_tracker_->GetEntityForTagHash(target_client_tag_hash);
  if (ShouldReuseTrackedUniquePositionFor(target_entity, UniquePosition(),
                                          UniquePosition())) {
    CHECK(target_entity);
    return target_entity->metadata().unique_position();
  }

  return UniquePosition::InitialPosition(
             UniquePosition::GenerateSuffix(target_client_tag_hash))
      .ToProto();
}

sync_pb::UniquePosition
ClientTagBasedDataTypeProcessor::GetUniquePositionForStorageKey(
    const std::string& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(entity_tracker_);

  ProcessorEntity* entity =
      entity_tracker_->GetEntityForStorageKey(storage_key);
  if (!entity) {
    DVLOG(1)
        << "GetUniquePositionForStorageKey is called for non-existing entity";
    return sync_pb::UniquePosition();
  }

  return entity->metadata().unique_position();
}

base::WeakPtr<DataTypeLocalChangeProcessor>
ClientTagBasedDataTypeProcessor::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_for_controller_.GetWeakPtr();
}

void ClientTagBasedDataTypeProcessor::ClearMetadataIfStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If a model error has been encountered, the local model is assumed to be
  // unusable, so no way to clear anything.
  if (model_error_.has_value()) {
    return;
  }

  // If Sync is not actually stopped, ignore this call.
  if (activation_request_.IsValid()) {
    return;
  }

  if (!model_ready_to_sync_) {
    // Defer clearing metadata until ModelReadyToSync() is invoked.
    pending_clear_metadata_ = true;
  } else if (IsTrackingMetadata()) {
    // Proceed only if there is metadata to clear and no error has been reported
    // yet.
    LogClearMetadataWhileStoppedHistogram(type_, /*is_delayed_call=*/false);
    DUMP_WILL_BE_CHECK(!activation_request_.IsValid());
    // This will incur an I/O operation by asking the bridge to clear the
    // metadata in storage.
    ClearAllTrackedMetadataAndResetState();
  }
}

void ClientTagBasedDataTypeProcessor::ReportBridgeErrorForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!model_error_.has_value());
  ReportError(ModelError(FROM_HERE, "Reported error from test"));
}

sync_pb::UniquePosition
ClientTagBasedDataTypeProcessor::GenerateFallbackUniquePosition(
    const ClientTagHash& client_tag_hash) const {
  base::UmaHistogramEnumeration("Sync.DataTypeUniquePositionError",
                                DataTypeHistogramValue(type_));
  return UniquePositionForInitialEntity(client_tag_hash);
}

}  // namespace syncer

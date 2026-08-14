// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_bridge.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/journey_specifics.pb.h"

namespace history::journeys {

JourneysSyncBridge::JourneysSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::JOURNEY, base::BindOnce(&JourneysSyncBridge::OnStoreCreated,
                                           weak_ptr_factory_.GetWeakPtr()));
}

JourneysSyncBridge::~JourneysSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<syncer::ModelError> JourneysSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is a read-only data type, meaning that no data originates locally,
  // hence there is nothing to merge.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

std::optional<syncer::ModelError>
JourneysSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/526686844): Implement me.
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> JourneysSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // JOURNEY is a read-only client data type. Committing local journeys back
  // to the server is not supported for now.
  NOTREACHED();
}

std::unique_ptr<syncer::DataBatch>
JourneysSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/526686844): Implement me.
  return std::make_unique<syncer::MutableDataBatch>();
}

// The global immutable sync identity, used to identify entities across devices.
// For Journeys, each journey generated on the server is assigned a unique GUID
// (journey_id). Returning journey_id ensures that incoming updates for the same
// Journey on different devices or sync cycles map to the exact same entity.
std::string JourneysSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.journey().journey_id();
}

// The local database primary key, representing the key under which this entity
// is stored in the local LevelDB database. When the sync processor receives
// incoming updates from the server, it uses this function to route the entity
// to the corresponding record in the DataTypestore and in-memory caches.
std::string JourneysSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTag(entity_data);
}

// Invoked before any received update from the server is processed or passed to
// ApplyIncrementalSyncChanges.
bool JourneysSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.has_journey() &&
         !entity_data.specifics.journey().journey_id().empty();
}

sync_pb::EntitySpecifics
JourneysSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // JOURNEY is currently a read-only client data type, but trimmed specifics
  // are returned in case local commits are supported in the future.
  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::JourneySpecifics trimmed_specifics = entity_specifics.journey();
  trimmed_specifics.clear_journey_id();
  trimmed_specifics.clear_title();
  trimmed_specifics.clear_emoji();
  trimmed_specifics.clear_overview();
  trimmed_specifics.clear_short_overview();
  trimmed_specifics.clear_creation_time_windows_epoch_micros();
  trimmed_specifics.clear_history_entries();
  trimmed_specifics.clear_continuation_queries();
  // LINT.ThenChange(//components/sync/protocol/journey_specifics.proto:JourneySpecifics)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_journey() = std::move(trimmed_specifics);
  }
  return trimmed_entity_specifics;
}

void JourneysSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (store_) {
    store_->DeleteAllDataAndMetadata(std::move(delete_metadata_change_list),
                                     base::DoNothing());
  }
}

void JourneysSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&JourneysSyncBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

// Receives all previously persisted journeys from disk.
// Here we parse the protobufs to populate our local in-memory dataset/model so
// that existing journeys are immediately available when Chrome starts. Then it
// initiates reading the sync metadata.
void JourneysSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  // TODO(crbug.com/526686844): Implement me. Currently skips parsing records
  // and directly reads metadata.
  store_->ReadAllMetadata(base::BindOnce(&JourneysSyncBridge::OnReadAllMetadata,
                                         weak_ptr_factory_.GetWeakPtr()));
}

// Receives the sync metadata batch, consisting of the per sync-type
// DataTypeState and the per journey entry EntityMetadata. Tells Chrome Sync
// that the local model is loaded and ready. Until this is called, Sync will not
// attempt GetUpdates or connect the bridge to the sync thread. If any disk I/O
// step fails, we disable the sync data type safely.
void JourneysSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

}  // namespace history::journeys

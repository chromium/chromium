// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/syncable_service_based_bridge.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/persisted_entity_data.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"

namespace syncer {
namespace {

std::unique_ptr<EntityData> ConvertPersistedToEntityData(
    const ClientTagHash& client_tag_hash,
    sync_pb::PersistedEntityData data) {
  DCHECK(!client_tag_hash.value().empty());
  auto entity_data = std::make_unique<EntityData>();

  entity_data->name = std::move(*data.mutable_name());
  entity_data->specifics = std::move(*data.mutable_specifics());
  entity_data->client_tag_hash = client_tag_hash;

  // Purposefully crash if we have client only data, as this could result in
  // sending password in plain text.
  CHECK(!entity_data->specifics.password().has_client_only_encrypted_data());

  return entity_data;
}

sync_pb::PersistedEntityData CreatePersistedFromRemoteData(
    const EntityData& entity_data) {
  sync_pb::PersistedEntityData persisted;
  persisted.set_name(entity_data.name);
  *persisted.mutable_specifics() = entity_data.specifics;
  return persisted;
}

sync_pb::PersistedEntityData CreatePersistedFromLocalData(
    const SyncData& sync_data) {
  DCHECK(sync_data.IsValid());
  DCHECK(!sync_data.GetTitle().empty());

  sync_pb::PersistedEntityData persisted;
  persisted.set_name(sync_data.GetTitle());
  *persisted.mutable_specifics() = sync_data.GetSpecifics();
  return persisted;
}

SyncChange::SyncChangeType ConvertToSyncChangeType(
    EntityChange::ChangeType type) {
  switch (type) {
    case EntityChange::ACTION_DELETE:
      return SyncChange::ACTION_DELETE;
    case EntityChange::ACTION_ADD:
      return SyncChange::ACTION_ADD;
    case EntityChange::ACTION_UPDATE:
      return SyncChange::ACTION_UPDATE;
  }
  NOTREACHED_IN_MIGRATION();
  return SyncChange::ACTION_UPDATE;
}

// Parses the content of |record_list| into |*in_memory_store|. The output
// parameter is first for binding purposes.
std::optional<ModelError> ParseInMemoryStoreOnBackendSequence(
    SyncableServiceBasedBridge::InMemoryStore* in_memory_store,
    std::unique_ptr<DataTypeStore::RecordList> record_list) {
  DCHECK(in_memory_store);
  DCHECK(in_memory_store->empty());
  DCHECK(record_list);

  for (const DataTypeStore::Record& record : *record_list) {
    sync_pb::PersistedEntityData persisted_entity;
    if (!persisted_entity.ParseFromString(record.value)) {
      return ModelError(FROM_HERE, "Failed deserializing data.");
    }

    in_memory_store->emplace(record.id, std::move(persisted_entity));
  }

  return std::nullopt;
}

// Object to propagate local changes to the bridge, which will ultimately
// propagate them to the server.
class LocalChangeProcessor : public SyncChangeProcessor {
 public:
  LocalChangeProcessor(
      DataType type,
      const base::RepeatingCallback<void(const std::optional<ModelError>&)>&
          error_callback,
      DataTypeStore* store,
      SyncableServiceBasedBridge::InMemoryStore* in_memory_store,
      DataTypeLocalChangeProcessor* other)
      : type_(type),
        error_callback_(error_callback),
        store_(store),
        in_memory_store_(in_memory_store),
        other_(other) {
    DCHECK(store);
    DCHECK(other);
  }

  LocalChangeProcessor(const LocalChangeProcessor&) = delete;
  LocalChangeProcessor& operator=(const LocalChangeProcessor&) = delete;

  ~LocalChangeProcessor() override = default;

  std::optional<ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const SyncChangeList& change_list) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Reject changes if the processor has already experienced errors.
    std::optional<ModelError> processor_error = other_->GetError();
    if (processor_error) {
      return processor_error;
    }

    std::unique_ptr<DataTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();

    for (const SyncChange& change : change_list) {
      switch (change.change_type()) {
        case SyncChange::ACTION_ADD:
        case SyncChange::ACTION_UPDATE: {
          DCHECK_EQ(type_, change.sync_data().GetDataType());
          DCHECK(change.sync_data().IsValid())
              << " from " << change.location().ToString();
          // Local adds and updates must have a non-unique-title.
          DCHECK(!change.sync_data().GetTitle().empty())
              << " from " << change.location().ToString();

          const ClientTagHash client_tag_hash =
              change.sync_data().GetClientTagHash();
          const std::string storage_key = client_tag_hash.value();
          DCHECK(!storage_key.empty());

          sync_pb::PersistedEntityData persisted_entity =
              CreatePersistedFromLocalData(change.sync_data());

          (*in_memory_store_)[storage_key] = persisted_entity;
          batch->WriteData(storage_key, persisted_entity.SerializeAsString());

          other_->Put(storage_key,
                      ConvertPersistedToEntityData(
                          /*client_tag_hash=*/client_tag_hash,
                          std::move(persisted_entity)),
                      batch->GetMetadataChangeList());
          break;
        }

        case SyncChange::ACTION_DELETE: {
          const std::string storage_key =
              change.sync_data().GetClientTagHash().value();
          DCHECK(!storage_key.empty())
              << " from " << change.location().ToString();

          if (IsActOnceDataType(type_)) {
            if (other_->IsEntityUnsynced(storage_key)) {
              // Ignore the local deletion if the entity hasn't been committed
              // yet, similarly to how WriteNode::Drop() does it.
              continue;
            }
            batch->GetMetadataChangeList()->ClearMetadata(storage_key);
            other_->UntrackEntityForStorageKey(storage_key);
          } else {
            other_->Delete(storage_key, DeletionOrigin::Unspecified(),
                           batch->GetMetadataChangeList());
          }

          in_memory_store_->erase(storage_key);
          batch->DeleteData(storage_key);

          break;
        }
      }
    }

    store_->CommitWriteBatch(std::move(batch), error_callback_);

    return std::nullopt;
  }

 private:
  const DataType type_;
  const base::RepeatingCallback<void(const std::optional<ModelError>&)>
      error_callback_;
  const raw_ptr<DataTypeStore> store_;
  const raw_ptr<SyncableServiceBasedBridge::InMemoryStore, DanglingUntriaged>
      in_memory_store_;
  const raw_ptr<DataTypeLocalChangeProcessor> other_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

SyncableServiceBasedBridge::SyncableServiceBasedBridge(
    DataType type,
    OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor,
    SyncableService* syncable_service)
    : DataTypeSyncBridge(std::move(change_processor)),
      type_(type),
      syncable_service_(syncable_service) {
  DCHECK(syncable_service_);

  init_start_time_ = base::Time::Now();

  std::move(store_factory)
      .Run(type_, base::BindOnce(&SyncableServiceBasedBridge::OnStoreCreated,
                                 weak_ptr_factory_.GetWeakPtr()));
}

SyncableServiceBasedBridge::~SyncableServiceBasedBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("sync",
               "SyncableServiceBasedBridge::~SyncableServiceBasedBridge");
  // Inform the syncable service to make sure instances of LocalChangeProcessor
  // are not continued to be used.
  if (syncable_service_started_) {
    syncable_service_->OnBrowserShutdown(type_);
  }
}

std::unique_ptr<MetadataChangeList>
SyncableServiceBasedBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<ModelError> SyncableServiceBasedBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(!syncable_service_started_);
  DCHECK(in_memory_store_.empty());

  StoreAndConvertRemoteChanges(std::move(metadata_change_list),
                               std::move(entity_change_list));

  // We ignore the output of previous call of StoreAndConvertRemoteChanges() at
  // this point and let StartSyncableService() read from |in_memory_store_|,
  // which has been updated above as part of StoreAndConvertRemoteChanges().
  return StartSyncableService();
}

std::optional<ModelError>
SyncableServiceBasedBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(syncable_service_started_);

  SyncChangeList sync_change_list = StoreAndConvertRemoteChanges(
      std::move(metadata_change_list), std::move(entity_change_list));

  if (sync_change_list.empty()) {
    return std::nullopt;
  }

  return syncable_service_->ProcessSyncChanges(FROM_HERE, sync_change_list);
}

std::unique_ptr<DataBatch> SyncableServiceBasedBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    auto it = in_memory_store_.find(storage_key);
    if (it != in_memory_store_.end()) {
      // Note that client tag hash is used as storage key too.
      batch->Put(storage_key,
                 ConvertPersistedToEntityData(
                     ClientTagHash::FromHashed(it->first), it->second));
    }
  }
  return batch;
}

std::unique_ptr<DataBatch>
SyncableServiceBasedBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& [storage_key, persisted_entity_data] : in_memory_store_) {
    // Note that client tag hash is used as storage key too.
    batch->Put(storage_key, ConvertPersistedToEntityData(
                                ClientTagHash::FromHashed(storage_key),
                                persisted_entity_data));
  }
  return batch;
}

std::string SyncableServiceBasedBridge::GetClientTag(
    const EntityData& entity_data) {
  // Not supported as per SupportsGetClientTag().
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string SyncableServiceBasedBridge::GetStorageKey(
    const EntityData& entity_data) {
  // Not supported as per SupportsGetStorageKey().
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

bool SyncableServiceBasedBridge::SupportsGetClientTag() const {
  return false;
}

bool SyncableServiceBasedBridge::SupportsGetStorageKey() const {
  return false;
}

ConflictResolution SyncableServiceBasedBridge::ResolveConflict(
    const std::string& storage_key,
    const EntityData& remote_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_data.is_deleted()) {
    return ConflictResolution::kUseRemote;
  }

  // Ignore local changes for extensions/apps when server had a delete, to
  // avoid unwanted reinstall of an uninstalled extension.
  if (type_ == EXTENSIONS || type_ == APPS) {
    DVLOG(1) << "Resolving conflict, ignoring local changes for extension/app";
    return ConflictResolution::kUseRemote;
  }

  return ConflictResolution::kUseLocal;
}

void SyncableServiceBasedBridge::ApplyDisableSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);

  in_memory_store_.clear();
  store_->DeleteAllDataAndMetadata(base::DoNothing());

  if (syncable_service_started_) {
    syncable_service_->StopSyncing(type_);
    syncable_service_started_ = false;
  }
}

size_t SyncableServiceBasedBridge::EstimateSyncOverheadMemoryUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::trace_event::EstimateMemoryUsage(in_memory_store_);
}

// static
std::unique_ptr<SyncChangeProcessor>
SyncableServiceBasedBridge::CreateLocalChangeProcessorForTesting(
    DataType type,
    DataTypeStore* store,
    InMemoryStore* in_memory_store,
    DataTypeLocalChangeProcessor* other) {
  return std::make_unique<LocalChangeProcessor>(
      type, /*error_callback=*/base::DoNothing(), store, in_memory_store,
      other);
}

void SyncableServiceBasedBridge::OnStoreCreated(
    const std::optional<ModelError>& error,
    std::unique_ptr<DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  DCHECK(store);
  store_ = std::move(store);

  auto in_memory_store = std::make_unique<InMemoryStore>();
  InMemoryStore* raw_in_memory_store = in_memory_store.get();
  store_->ReadAllDataAndPreprocess(
      base::BindOnce(&ParseInMemoryStoreOnBackendSequence,
                     base::Unretained(raw_in_memory_store)),
      base::BindOnce(&SyncableServiceBasedBridge::OnReadAllDataForInit,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(in_memory_store)));
}

void SyncableServiceBasedBridge::OnReadAllDataForInit(
    std::unique_ptr<InMemoryStore> in_memory_store,
    const std::optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(in_memory_store.get());
  DCHECK(in_memory_store_.empty());

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  in_memory_store_ = std::move(*in_memory_store);

  store_->ReadAllMetadata(
      base::BindOnce(&SyncableServiceBasedBridge::OnReadAllMetadataForInit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SyncableServiceBasedBridge::OnReadAllMetadataForInit(
    const std::optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "SyncableServiceBasedBridge::OnReadAllMetadataForInit");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncable_service_started_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  syncable_service_->WaitUntilReadyToSync(base::BindOnce(
      &SyncableServiceBasedBridge::OnSyncableServiceReady,
      weak_ptr_factory_.GetWeakPtr(), std::move(metadata_batch)));
}

void SyncableServiceBasedBridge::OnSyncableServiceReady(
    std::unique_ptr<MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "SyncableServiceBasedBridge::OnSyncableServiceReady");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncable_service_started_);

  // Guard against inconsistent state, and recover from it by starting from
  // scratch, which will cause the eventual refetching of all entities from the
  // server.
  if (!IsInitialSyncDone(
          metadata_batch->GetDataTypeState().initial_sync_state()) &&
      !in_memory_store_.empty()) {
    in_memory_store_.clear();
    store_->DeleteAllDataAndMetadata(base::DoNothing());
    change_processor()->ModelReadyToSync(std::make_unique<MetadataBatch>());
    DCHECK(!change_processor()->IsTrackingMetadata());
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // If sync was previously enabled according to the loaded metadata, then
  // immediately start the SyncableService to track as many local changes as
  // possible (regardless of whether sync actually starts or not). Otherwise,
  // the SyncableService will be started from MergeFullSyncData().
  if (change_processor()->IsTrackingMetadata()) {
    if (auto error = StartSyncableService()) {
      change_processor()->ReportError(*error);
    } else {
      // Using the same range as Sync.DataTypeConfigurationTime.* metric.
      base::UmaHistogramCustomTimes(
          base::StringPrintf("Sync.SyncableServiceStartTime.%s",
                             DataTypeToHistogramSuffix(type_)),
          base::Time::Now() - init_start_time_,
          /*min=*/base::Milliseconds(1),
          /*max=*/base::Seconds(60), /*buckets=*/50);
    }
  }
}

std::optional<ModelError> SyncableServiceBasedBridge::StartSyncableService() {
  DCHECK(store_);
  DCHECK(!syncable_service_started_);
  DCHECK(change_processor()->IsTrackingMetadata());

  // Sync enabled, so exercise MergeDataAndStartSyncing() immediately, since
  // this function is reached only if sync is starting already.
  SyncDataList initial_sync_data;
  initial_sync_data.reserve(in_memory_store_.size());
  for (const auto& [storage_key, persisted_entity_data] : in_memory_store_) {
    // Note that client tag hash is used as storage key too.
    initial_sync_data.push_back(
        SyncData::CreateRemoteData(persisted_entity_data.specifics(),
                                   ClientTagHash::FromHashed(storage_key)));
  }

  auto error_callback =
      base::BindRepeating(&SyncableServiceBasedBridge::ReportErrorIfSet,
                          weak_ptr_factory_.GetWeakPtr());
  auto local_change_processor = std::make_unique<LocalChangeProcessor>(
      type_, error_callback, store_.get(), &in_memory_store_,
      change_processor());

  const std::optional<ModelError> merge_error =
      syncable_service_->MergeDataAndStartSyncing(
          type_, initial_sync_data, std::move(local_change_processor));

  if (!merge_error) {
    syncable_service_started_ = true;
  }

  return merge_error;
}

SyncChangeList SyncableServiceBasedBridge::StoreAndConvertRemoteChanges(
    std::unique_ptr<MetadataChangeList> initial_metadata_change_list,
    EntityChangeList input_entity_change_list) {
  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();
  batch->TakeMetadataChangesFrom(std::move(initial_metadata_change_list));

  SyncChangeList output_sync_change_list;
  output_sync_change_list.reserve(input_entity_change_list.size());

  for (const std::unique_ptr<EntityChange>& change : input_entity_change_list) {
    switch (change->type()) {
      case EntityChange::ACTION_DELETE: {
        const std::string& storage_key = change->storage_key();
        DCHECK_NE(0U, in_memory_store_.count(storage_key));
        DVLOG(1) << DataTypeToDebugString(type_)
                 << ": Processing deletion with storage key: " << storage_key;
        output_sync_change_list.emplace_back(
            FROM_HERE, SyncChange::ACTION_DELETE,
            SyncData::CreateRemoteData(
                in_memory_store_[storage_key].specifics(),
                ClientTagHash::FromHashed(storage_key)));

        // For tombstones, there is no actual data, which means no client tag
        // hash either, but the processor provides the storage key.
        DCHECK(!storage_key.empty());
        batch->DeleteData(storage_key);
        in_memory_store_.erase(storage_key);
        break;
      }

      case EntityChange::ACTION_ADD:
        // Because we use the client tag hash as storage key, let the processor
        // know.
        change_processor()->UpdateStorageKey(
            change->data(),
            /*storage_key=*/change->data().client_tag_hash.value(),
            batch->GetMetadataChangeList());
        [[fallthrough]];

      case EntityChange::ACTION_UPDATE: {
        const std::string& storage_key = change->data().client_tag_hash.value();
        DVLOG(1) << DataTypeToDebugString(type_)
                 << ": Processing add/update with key: " << storage_key;

        output_sync_change_list.emplace_back(
            FROM_HERE, ConvertToSyncChangeType(change->type()),
            SyncData::CreateRemoteData(change->data().specifics,
                                       change->data().client_tag_hash));

        sync_pb::PersistedEntityData persisted_entity_data =
            CreatePersistedFromRemoteData(change->data());
        batch->WriteData(storage_key,
                         persisted_entity_data.SerializeAsString());
        in_memory_store_[storage_key] = std::move(persisted_entity_data);
        break;
      }
    }
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&SyncableServiceBasedBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  return output_sync_change_list;
}

void SyncableServiceBasedBridge::ReportErrorIfSet(
    const std::optional<ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace syncer

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/syncable_service_based_bridge.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/persisted_entity_data.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"

namespace syncer {
namespace {

// Same as kInvalidId in syncable/base_node.h.
constexpr int64_t kInvalidNodeId = 0;

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

sync_pb::PersistedEntityData CreatePersistedFromEntityData(
    const EntityData& entity_data) {
  sync_pb::PersistedEntityData persisted;
  persisted.set_name(entity_data.name);
  *persisted.mutable_specifics() = entity_data.specifics;
  return persisted;
}

sync_pb::PersistedEntityData CreatePersistedFromSyncData(
    const SyncDataLocal& sync_data) {
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
  NOTREACHED();
  return SyncChange::ACTION_INVALID;
}

base::Optional<ModelError> ConvertToModelError(const SyncError& sync_error) {
  if (sync_error.IsSet()) {
    return ModelError(sync_error.location(), sync_error.message());
  }
  return base::nullopt;
}

// Parses the content of |record_list| into |*in_memory_store|. The output
// parameter is first for binding purposes.
base::Optional<ModelError> ParseInMemoryStoreOnBackendSequence(
    SyncableServiceBasedBridge::InMemoryStore* in_memory_store,
    std::unique_ptr<ModelTypeStore::RecordList> record_list) {
  DCHECK(in_memory_store);
  DCHECK(in_memory_store->empty());
  DCHECK(record_list);

  for (const ModelTypeStore::Record& record : *record_list) {
    sync_pb::PersistedEntityData persisted_entity;
    if (!persisted_entity.ParseFromString(record.value)) {
      return ModelError(FROM_HERE, "Failed deserializing data.");
    }

    in_memory_store->emplace(record.id,
                             std::move(*persisted_entity.mutable_specifics()));
  }

  return base::nullopt;
}

// Object to propagate local changes to the bridge, which will ultimately
// propagate them to the server.
class LocalChangeProcessor : public SyncChangeProcessor {
 public:
  LocalChangeProcessor(
      ModelType type,
      const base::RepeatingCallback<void(const base::Optional<ModelError>&)>&
          error_callback,
      ModelTypeStore* store,
      SyncableServiceBasedBridge::InMemoryStore* in_memory_store,
      ModelTypeChangeProcessor* other)
      : type_(type),
        error_callback_(error_callback),
        store_(store),
        in_memory_store_(in_memory_store),
        other_(other) {
    DCHECK(store);
    DCHECK(other);
  }

  ~LocalChangeProcessor() override {}

  SyncError ProcessSyncChanges(const base::Location& from_here,
                               const SyncChangeList& change_list) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Reject changes if the processor has already experienced errors.
    base::Optional<ModelError> processor_error = other_->GetError();
    if (processor_error) {
      return SyncError(processor_error->location(),
                       SyncError::UNRECOVERABLE_ERROR,
                       processor_error->message(), type_);
    }

    std::unique_ptr<ModelTypeStore::WriteBatch> batch =
        store_->CreateWriteBatch();

    for (const SyncChange& change : change_list) {
      switch (change.change_type()) {
        case SyncChange::ACTION_INVALID:
          NOTREACHED() << " from " << change.location().ToString();
          break;

        case SyncChange::ACTION_ADD:
        case SyncChange::ACTION_UPDATE: {
          DCHECK_EQ(type_, change.sync_data().GetDataType());
          DCHECK(change.sync_data().IsLocal())
              << " from " << change.location().ToString();

          SyncDataLocal sync_data(change.sync_data());
          DCHECK(sync_data.IsValid())
              << " from " << change.location().ToString();

          const ClientTagHash client_tag_hash =
              ClientTagHash::FromUnhashed(type_, sync_data.GetTag());
          const std::string storage_key = client_tag_hash.value();
          DCHECK(!storage_key.empty());

          (*in_memory_store_)[storage_key] = sync_data.GetSpecifics();
          sync_pb::PersistedEntityData persisted_entity =
              CreatePersistedFromSyncData(sync_data);
          batch->WriteData(storage_key, persisted_entity.SerializeAsString());

          other_->Put(storage_key,
                      ConvertPersistedToEntityData(
                          /*client_tag_hash=*/client_tag_hash,
                          std::move(persisted_entity)),
                      batch->GetMetadataChangeList());
          break;
        }

        case SyncChange::ACTION_DELETE: {
          std::string storage_key;
          // Both SyncDataLocal and SyncDataRemote are allowed for deletions.
          if (change.sync_data().IsLocal()) {
            SyncDataLocal sync_data(change.sync_data());
            DCHECK(sync_data.IsValid())
                << " from " << change.location().ToString();
            storage_key =
                ClientTagHash::FromUnhashed(type_, sync_data.GetTag()).value();
          } else {
            SyncDataRemote sync_data(change.sync_data());
            storage_key = sync_data.GetClientTagHash().value();
          }

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
            other_->Delete(storage_key, batch->GetMetadataChangeList());
          }

          in_memory_store_->erase(storage_key);
          batch->DeleteData(storage_key);

          break;
        }
      }
    }

    store_->CommitWriteBatch(std::move(batch), error_callback_);

    return SyncError();
  }

  SyncDataList GetAllSyncData(ModelType type) const override {
    // This function is not supported and not exercised by the relevant
    // datatypes (that are integrated with this bridge).
    NOTREACHED();
    return SyncDataList();
  }

  SyncError UpdateDataTypeContext(ModelType type,
                                  ContextRefreshStatus refresh_status,
                                  const std::string& context) override {
    // This function is not supported and not exercised by anyone, since
    // the USS flow doesn't use SharedChangeProcessor.
    // TODO(crbug.com/870624): Remove this function altogether when the
    // directory codebase is removed.
    NOTREACHED();
    return SyncError();
  }

  void AddLocalChangeObserver(LocalChangeObserver* observer) override {
    // This function is not supported and not exercised by the relevant
    // datatypes (that are integrated with this bridge).
    NOTREACHED();
  }

  void RemoveLocalChangeObserver(LocalChangeObserver* observer) override {
    // This function is not supported and not exercised by the relevant
    // datatypes (that are integrated with this bridge).
    NOTREACHED();
  }

 private:
  const ModelType type_;
  const base::RepeatingCallback<void(const base::Optional<ModelError>&)>
      error_callback_;
  ModelTypeStore* const store_;
  SyncableServiceBasedBridge::InMemoryStore* const in_memory_store_;
  ModelTypeChangeProcessor* const other_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LocalChangeProcessor);
};

class SyncErrorFactoryImpl : public SyncErrorFactory {
 public:
  explicit SyncErrorFactoryImpl(ModelType type) : type_(type) {}
  ~SyncErrorFactoryImpl() override = default;

  SyncError CreateAndUploadError(const base::Location& location,
                                 const std::string& message) override {
    // Uploading is not supported, we simply return the error.
    return SyncError(location, SyncError::DATATYPE_ERROR, message, type_);
  }

 private:
  const ModelType type_;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorFactoryImpl);
};

}  // namespace

SyncableServiceBasedBridge::SyncableServiceBasedBridge(
    ModelType type,
    OnceModelTypeStoreFactory store_factory,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor,
    SyncableService* syncable_service)
    : ModelTypeSyncBridge(std::move(change_processor)),
      type_(type),
      syncable_service_(syncable_service),
      syncable_service_started_(false) {
  DCHECK(syncable_service_);

  std::move(store_factory)
      .Run(type_, base::BindOnce(&SyncableServiceBasedBridge::OnStoreCreated,
                                 weak_ptr_factory_.GetWeakPtr()));
}

SyncableServiceBasedBridge::~SyncableServiceBasedBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Stop the syncable service to make sure instances of LocalChangeProcessor
  // are not continued to be used.
  if (syncable_service_started_) {
    syncable_service_->StopSyncing(type_);
  }
}

std::unique_ptr<MetadataChangeList>
SyncableServiceBasedBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

base::Optional<ModelError> SyncableServiceBasedBridge::MergeSyncData(
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

base::Optional<ModelError> SyncableServiceBasedBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(syncable_service_started_);

  SyncChangeList sync_change_list = StoreAndConvertRemoteChanges(
      std::move(metadata_change_list), std::move(entity_change_list));

  if (sync_change_list.empty()) {
    return base::nullopt;
  }

  return ConvertToModelError(
      syncable_service_->ProcessSyncChanges(FROM_HERE, sync_change_list));
}

void SyncableServiceBasedBridge::GetData(StorageKeyList storage_keys,
                                         DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  store_->ReadData(
      storage_keys,
      base::BindOnce(&SyncableServiceBasedBridge::OnReadDataForProcessor,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SyncableServiceBasedBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  store_->ReadAllData(
      base::BindOnce(&SyncableServiceBasedBridge::OnReadAllDataForProcessor,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::string SyncableServiceBasedBridge::GetClientTag(
    const EntityData& entity_data) {
  // Not supported as per SupportsGetClientTag().
  NOTREACHED();
  return std::string();
}

std::string SyncableServiceBasedBridge::GetStorageKey(
    const EntityData& entity_data) {
  // Not supported as per SupportsGetStorageKey().
  NOTREACHED();
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

void SyncableServiceBasedBridge::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);

  // If Sync is being stopped only temporarily (i.e. we want to keep tracking
  // metadata), then there's nothing to do here.
  if (!delete_metadata_change_list) {
    return;
  }

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
    ModelType type,
    ModelTypeStore* store,
    InMemoryStore* in_memory_store,
    ModelTypeChangeProcessor* other) {
  return std::make_unique<LocalChangeProcessor>(
      type, /*error_callback=*/base::DoNothing(), store, in_memory_store,
      other);
}

void SyncableServiceBasedBridge::OnStoreCreated(
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore> store) {
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
    const base::Optional<ModelError>& error) {
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
    const base::Optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncable_service_started_);

  // Guard against inconsistent state, and recover from it by starting from
  // scratch, which will cause the eventual refetching of all entities from the
  // server.
  if (!metadata_batch->GetModelTypeState().initial_sync_done() &&
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
  // the SyncableService will be started from MergeSyncData().
  if (change_processor()->IsTrackingMetadata()) {
    ReportErrorIfSet(StartSyncableService());
  }
}

base::Optional<ModelError> SyncableServiceBasedBridge::StartSyncableService() {
  DCHECK(store_);
  DCHECK(!syncable_service_started_);
  DCHECK(change_processor()->IsTrackingMetadata());

  const base::TimeTicks start_time = base::TimeTicks::Now();

  // Sync enabled, so exercise MergeDataAndStartSyncing() immediately, since
  // this function is reached only if sync is starting already.
  SyncDataList initial_sync_data;
  initial_sync_data.reserve(in_memory_store_.size());
  for (const std::pair<const std::string, sync_pb::EntitySpecifics>& record :
       in_memory_store_) {
    initial_sync_data.push_back(SyncData::CreateRemoteData(
        /*id=*/kInvalidNodeId, std::move(record.second),
        /*client_tag_hash=*/record.first));
  }

  auto error_callback =
      base::BindRepeating(&SyncableServiceBasedBridge::ReportErrorIfSet,
                          weak_ptr_factory_.GetWeakPtr());
  auto local_change_processor = std::make_unique<LocalChangeProcessor>(
      type_, error_callback, store_.get(), &in_memory_store_,
      change_processor());

  const base::Optional<ModelError> merge_error = ConvertToModelError(
      syncable_service_
          ->MergeDataAndStartSyncing(
              type_, initial_sync_data, std::move(local_change_processor),
              std::make_unique<SyncErrorFactoryImpl>(type_))
          .error());

  RecordAssociationTime(base::TimeTicks::Now() - start_time);

  if (!merge_error) {
    syncable_service_started_ = true;
  }

  return merge_error;
}

SyncChangeList SyncableServiceBasedBridge::StoreAndConvertRemoteChanges(
    std::unique_ptr<MetadataChangeList> initial_metadata_change_list,
    EntityChangeList input_entity_change_list) {
  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->TakeMetadataChangesFrom(std::move(initial_metadata_change_list));

  SyncChangeList output_sync_change_list;
  output_sync_change_list.reserve(input_entity_change_list.size());

  for (const std::unique_ptr<EntityChange>& change : input_entity_change_list) {
    switch (change->type()) {
      case EntityChange::ACTION_DELETE: {
        const std::string& storage_key = change->storage_key();
        DCHECK_NE(0U, in_memory_store_.count(storage_key));
        DVLOG(1) << ModelTypeToString(type_)
                 << ": Processing deletion with storage key: " << storage_key;
        output_sync_change_list.emplace_back(
            FROM_HERE, SyncChange::ACTION_DELETE,
            SyncData::CreateRemoteData(
                /*id=*/kInvalidNodeId, in_memory_store_[storage_key],
                /*client_tag_hash=*/""));

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
        FALLTHROUGH;

      case EntityChange::ACTION_UPDATE: {
        const std::string& storage_key = change->data().client_tag_hash.value();
        DVLOG(1) << ModelTypeToString(type_)
                 << ": Processing add/update with key: " << storage_key;

        output_sync_change_list.emplace_back(
            FROM_HERE, ConvertToSyncChangeType(change->type()),
            SyncData::CreateRemoteData(
                /*id=*/kInvalidNodeId, change->data().specifics,
                change->data().client_tag_hash.value()));

        batch->WriteData(
            storage_key,
            CreatePersistedFromEntityData(change->data()).SerializeAsString());
        in_memory_store_[storage_key] = change->data().specifics;
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

void SyncableServiceBasedBridge::OnReadDataForProcessor(
    DataCallback callback,
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore::RecordList> record_list,
    std::unique_ptr<ModelTypeStore::IdList> missing_id_list) {
  OnReadAllDataForProcessor(std::move(callback), error, std::move(record_list));
}

void SyncableServiceBasedBridge::OnReadAllDataForProcessor(
    DataCallback callback,
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore::RecordList> record_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const ModelTypeStore::Record& record : *record_list) {
    sync_pb::PersistedEntityData persisted_entity;
    if (record.id.empty() || !persisted_entity.ParseFromString(record.value)) {
      change_processor()->ReportError(
          {FROM_HERE, "Failed deserializing data."});
      return;
    }

    // Note that client tag hash is used as storage key too.
    batch->Put(record.id, ConvertPersistedToEntityData(
                              ClientTagHash::FromHashed(record.id),
                              std::move(persisted_entity)));
  }
  std::move(callback).Run(std::move(batch));
}

void SyncableServiceBasedBridge::ReportErrorIfSet(
    const base::Optional<ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void SyncableServiceBasedBridge::RecordAssociationTime(
    base::TimeDelta time) const {
// This mimics the implementation in SharedChangeProcessor.
#define PER_DATA_TYPE_MACRO(type_str) \
  UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type_);
#undef PER_DATA_TYPE_MACRO
}

}  // namespace syncer

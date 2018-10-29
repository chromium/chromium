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
#include "components/sync/base/hash_util.h"
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
    const std::string& client_tag_hash,
    sync_pb::PersistedEntityData data) {
  DCHECK(!client_tag_hash.empty());
  DCHECK(!data.non_unique_name().empty());
  auto entity_data = std::make_unique<EntityData>();

  entity_data->non_unique_name = std::move(*data.mutable_non_unique_name());
  entity_data->specifics = std::move(*data.mutable_specifics());
  entity_data->client_tag_hash = client_tag_hash;

  // Purposefully crash if we have client only data, as this could result in
  // sending password in plain text.
  CHECK(!entity_data->specifics.password().has_client_only_encrypted_data());

  return entity_data;
}

sync_pb::PersistedEntityData CreatePersistedFromEntityData(
    const EntityData& entity_data) {
  DCHECK(!entity_data.non_unique_name.empty());

  sync_pb::PersistedEntityData persisted;
  persisted.set_non_unique_name(entity_data.non_unique_name);
  *persisted.mutable_specifics() = entity_data.specifics;
  return persisted;
}

sync_pb::PersistedEntityData CreatePersistedFromSyncData(
    const SyncDataLocal& sync_data) {
  DCHECK(!sync_data.GetTitle().empty());
  sync_pb::PersistedEntityData persisted;
  persisted.set_non_unique_name(sync_data.GetTitle());
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

// Object to propagate local changes to the bridge, which will ultimately
// propagate them to the server.
class ChangeProcessorImpl : public SyncChangeProcessor {
 public:
  ChangeProcessorImpl(
      ModelType type,
      const base::RepeatingCallback<void(const base::Optional<ModelError>&)>&
          error_callback,
      ModelTypeStore* store,
      std::map<std::string, sync_pb::PersistedEntityData>* in_memory_store,
      scoped_refptr<SyncableServiceBasedBridge::ModelCryptographer>
          cryptographer,
      ModelTypeChangeProcessor* other)
      : type_(type),
        error_callback_(error_callback),
        store_(store),
        in_memory_store_(in_memory_store),
        cryptographer_(std::move(cryptographer)),
        other_(other) {
    DCHECK(store);
    DCHECK(other);
  }

  ~ChangeProcessorImpl() override {}

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
          const std::string storage_key =
              GenerateSyncableHash(type_, sync_data.GetTag());
          DCHECK(!storage_key.empty());

          sync_pb::PersistedEntityData persisted_entity =
              CreatePersistedFromSyncData(sync_data);
          // Production code uses a cryptographer only for PASSWORDS.
          if (cryptographer_) {
            const base::Optional<ModelError> error =
                cryptographer_->Encrypt(persisted_entity.mutable_specifics());
            if (error) {
              other_->ReportError(*error);
              return SyncError(error->location(), SyncError::CRYPTO_ERROR,
                               error->message(), type_);
            }
            persisted_entity.set_non_unique_name("encrypted");
          }

          // Purposefully crash if we have client only data, as this could
          // result in storing password in plain text.
          CHECK(!persisted_entity.specifics()
                     .password()
                     .has_client_only_encrypted_data());

          (*in_memory_store_)[storage_key] = persisted_entity;
          batch->WriteData(storage_key, persisted_entity.SerializeAsString());
          other_->Put(
              storage_key,
              ConvertPersistedToEntityData(
                  /*client_tag_hash=*/storage_key, std::move(persisted_entity)),
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
            storage_key = GenerateSyncableHash(type_, sync_data.GetTag());
          } else {
            SyncDataRemote sync_data(change.sync_data());
            storage_key = sync_data.GetClientTagHash();
          }

          DCHECK(!storage_key.empty())
              << " from " << change.location().ToString();

          in_memory_store_->erase(storage_key);
          batch->DeleteData(storage_key);

          if (IsActOnceDataType(type_)) {
            batch->GetMetadataChangeList()->ClearMetadata(storage_key);
            other_->UntrackEntityForStorageKey(storage_key);
          } else {
            other_->Delete(storage_key, batch->GetMetadataChangeList());
          }

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
  std::map<std::string, sync_pb::PersistedEntityData>* const in_memory_store_;
  const scoped_refptr<SyncableServiceBasedBridge::ModelCryptographer>
      cryptographer_;
  ModelTypeChangeProcessor* const other_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ChangeProcessorImpl);
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

SyncableServiceBasedBridge::ModelCryptographer::ModelCryptographer() {}

SyncableServiceBasedBridge::ModelCryptographer::~ModelCryptographer() {}

SyncableServiceBasedBridge::SyncableServiceBasedBridge(
    ModelType type,
    OnceModelTypeStoreFactory store_factory,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor,
    SyncableService* syncable_service,
    scoped_refptr<ModelCryptographer> cryptographer)
    : ModelTypeSyncBridge(std::move(change_processor)),
      type_(type),
      syncable_service_(syncable_service),
      cryptographer_(std::move(cryptographer)),
      store_factory_(std::move(store_factory)),
      syncable_service_started_(false),
      weak_ptr_factory_(this) {
  DCHECK(store_factory_);
  DCHECK(syncable_service_);
  DCHECK(cryptographer_ || type_ != PASSWORDS);
}

SyncableServiceBasedBridge::~SyncableServiceBasedBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Stop the syncable service to make sure instances of ChangeProcessorImpl are
  // not continued to be used.
  if (syncable_service_started_) {
    syncable_service_->StopSyncing(type_);
  }
}

std::unique_ptr<MetadataChangeList>
SyncableServiceBasedBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

void SyncableServiceBasedBridge::OnSyncStarting(
    const DataTypeActivationRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncable_service_started_);

  if (!store_factory_) {
    // Sync was have been started earlier, and |store_| is guaranteed to be
    // initialized because stopping of the datatype cannot be completed before
    // ModelReadyToSync().
    DCHECK(store_);
    MaybeStartSyncableService();
    return;
  }

  std::move(store_factory_)
      .Run(type_, base::BindOnce(&SyncableServiceBasedBridge::OnStoreCreated,
                                 weak_ptr_factory_.GetWeakPtr()));
  DCHECK(!store_factory_);
}

base::Optional<ModelError> SyncableServiceBasedBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(!syncable_service_started_);
  DCHECK(in_memory_store_.empty());

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  SyncChangeList sync_change_list;
  const base::Optional<ModelError> error = StoreAndConvertRemoteChanges(
      std::move(batch), std::move(entity_change_list), &sync_change_list);
  if (error) {
    return error;
  }

  SyncDataList initial_sync_data;
  initial_sync_data.reserve(sync_change_list.size());
  for (const SyncChange& change : sync_change_list) {
    initial_sync_data.push_back(change.sync_data());
  }

  auto error_callback =
      base::BindRepeating(&SyncableServiceBasedBridge::ReportErrorIfSet,
                          weak_ptr_factory_.GetWeakPtr());
  auto processor_impl = std::make_unique<ChangeProcessorImpl>(
      type_, error_callback, store_.get(), &in_memory_store_, cryptographer_,
      change_processor());

  const base::Optional<ModelError> merge_error = ConvertToModelError(
      syncable_service_
          ->MergeDataAndStartSyncing(
              type_, initial_sync_data, std::move(processor_impl),
              std::make_unique<SyncErrorFactoryImpl>(type_))
          .error());

  if (!merge_error) {
    syncable_service_started_ = true;
  }

  return merge_error;
}

base::Optional<ModelError> SyncableServiceBasedBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  return ApplySyncChangesWithBatch(std::move(metadata_change_list),
                                   std::move(entity_change_list),
                                   store_->CreateWriteBatch());
}

void SyncableServiceBasedBridge::GetData(StorageKeyList storage_keys,
                                         DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);

  auto batch = std::make_unique<MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    auto it = in_memory_store_.find(storage_key);
    if (it == in_memory_store_.end()) {
      // Suggests orphan metadata, which the processor should handle.
      continue;
    }

    batch->Put(storage_key, ConvertPersistedToEntityData(
                                /*client_tag_hash=*/storage_key, it->second));
  }

  std::move(callback).Run(std::move(batch));
}

void SyncableServiceBasedBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);

  auto batch = std::make_unique<MutableDataBatch>();
  for (const std::pair<const std::string, sync_pb::PersistedEntityData>&
           record : in_memory_store_) {
    batch->Put(record.first,
               ConvertPersistedToEntityData(
                   /*client_tag_hash=*/record.first, record.second));
  }

  std::move(callback).Run(std::move(batch));
}

std::string SyncableServiceBasedBridge::GetClientTag(
    const EntityData& entity_data) {
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
    const EntityData& local_data,
    const EntityData& remote_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_data.is_deleted()) {
    return ConflictResolution::UseRemote();
  }

  DCHECK(!local_data.is_deleted());

  // Ignore local changes for extensions/apps when server had a delete, to
  // avoid unwanted reinstall of an uninstalled extension.
  if (type_ == EXTENSIONS || type_ == APPS) {
    DVLOG(1) << "Resolving conflict, ignoring local changes for extension/app";
    return ConflictResolution::UseRemote();
  }

  return ConflictResolution::UseLocal();
}

ModelTypeSyncBridge::StopSyncResponse
SyncableServiceBasedBridge::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);

  if (delete_metadata_change_list) {
    in_memory_store_.clear();
    store_->DeleteAllDataAndMetadata(base::DoNothing());
  }

  if (syncable_service_started_) {
    syncable_service_->StopSyncing(type_);
    syncable_service_started_ = false;
  }

  return StopSyncResponse::kModelStillReadyToSync;
}

size_t SyncableServiceBasedBridge::EstimateSyncOverheadMemoryUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::trace_event::EstimateMemoryUsage(in_memory_store_);
}

base::Optional<ModelError>
SyncableServiceBasedBridge::ApplySyncChangesWithNewEncryptionRequirements(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);

  std::unique_ptr<ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  if (cryptographer_) {
    // This is used by PASSWORDS only.
    base::Optional<ModelError> error = ReencryptEverything(batch.get());
    if (error) {
      return error;
    }
  }

  return ApplySyncChangesWithBatch(std::move(metadata_change_list),
                                   std::move(entity_changes), std::move(batch));
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

  store_->ReadAllData(
      base::BindOnce(&SyncableServiceBasedBridge::OnReadAllDataForInit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SyncableServiceBasedBridge::OnReadAllDataForInit(
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore::RecordList> record_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(in_memory_store_.empty());

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const ModelTypeStore::Record& record : *record_list) {
    sync_pb::PersistedEntityData persisted_entity;
    if (!persisted_entity.ParseFromString(record.value)) {
      change_processor()->ReportError(
          {FROM_HERE, "Failed deserializing data."});
      return;
    }

    in_memory_store_[record.id] = persisted_entity;
  }

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

  MaybeStartSyncableService();
}

void SyncableServiceBasedBridge::MaybeStartSyncableService() {
  DCHECK(!syncable_service_started_);
  DCHECK(store_);

  // If sync wasn't enabled according to the loaded metadata, let's wait until
  // MergeSyncData() is called before starting the SyncableService.
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  // Sync enabled, so exercise MergeDataAndStartSyncing() immediately, since
  // this function is reached only if sync is starting already.
  SyncDataList initial_sync_data;
  initial_sync_data.reserve(in_memory_store_.size());
  for (const std::pair<const std::string, sync_pb::PersistedEntityData>&
           record : in_memory_store_) {
    sync_pb::EntitySpecifics specifics = record.second.specifics();

    // Production code uses a cryptographer only for PASSWORDS.
    if (cryptographer_) {
      const base::Optional<ModelError> error =
          cryptographer_->Decrypt(&specifics);
      if (error) {
        change_processor()->ReportError(*error);
        return;
      }
    }

    initial_sync_data.push_back(SyncData::CreateRemoteData(
        /*id=*/kInvalidNodeId, std::move(specifics),
        /*last_modified_time=*/base::Time(),  // Used by legacy sessions only.
        /*client_tag_hash=*/record.first));
  }

  auto error_callback =
      base::BindRepeating(&SyncableServiceBasedBridge::ReportErrorIfSet,
                          weak_ptr_factory_.GetWeakPtr());
  auto processor_impl = std::make_unique<ChangeProcessorImpl>(
      type_, error_callback, store_.get(), &in_memory_store_, cryptographer_,
      change_processor());

  const base::Optional<ModelError> merge_error = ConvertToModelError(
      syncable_service_
          ->MergeDataAndStartSyncing(
              type_, initial_sync_data, std::move(processor_impl),
              std::make_unique<SyncErrorFactoryImpl>(type_))
          .error());

  if (merge_error) {
    change_processor()->ReportError(*merge_error);
  } else {
    syncable_service_started_ = true;
  }
}

base::Optional<ModelError>
SyncableServiceBasedBridge::StoreAndConvertRemoteChanges(
    std::unique_ptr<ModelTypeStore::WriteBatch> batch,
    EntityChangeList input_entity_change_list,
    SyncChangeList* output_sync_change_list) {
  DCHECK(output_sync_change_list);

  output_sync_change_list->clear();
  output_sync_change_list->reserve(input_entity_change_list.size());

  for (const EntityChange& change : input_entity_change_list) {
    switch (change.type()) {
      case EntityChange::ACTION_DELETE: {
        const std::string& storage_key = change.storage_key();
        DCHECK_NE(0U, in_memory_store_.count(storage_key));
        DVLOG(1) << ModelTypeToString(type_)
                 << ": Processing deletion with storage key: " << storage_key;

        sync_pb::EntitySpecifics specifics =
            std::move(*in_memory_store_[storage_key].mutable_specifics());
        in_memory_store_.erase(storage_key);

        // Production code uses a cryptographer only for PASSWORDS.
        if (cryptographer_) {
          const base::Optional<ModelError> error =
              cryptographer_->Decrypt(&specifics);
          if (error) {
            return error;
          }
        }

        output_sync_change_list->emplace_back(
            FROM_HERE, SyncChange::ACTION_DELETE,
            SyncData::CreateRemoteData(
                /*id=*/kInvalidNodeId, std::move(specifics),
                change.data().modification_time,
                change.data().client_tag_hash));

        // For tombstones, there is no actual data, which means no client tag
        // hash either, but the processor provides the storage key.
        DCHECK(!storage_key.empty());
        batch->DeleteData(storage_key);
        break;
      }

      case EntityChange::ACTION_ADD:
        // Because we use the client tag hash as storage key, let the processor
        // know.
        change_processor()->UpdateStorageKey(
            change.data(), /*storage_key=*/change.data().client_tag_hash,
            batch->GetMetadataChangeList());
        FALLTHROUGH;

      case EntityChange::ACTION_UPDATE: {
        const std::string& storage_key = change.data().client_tag_hash;
        DVLOG(1) << ModelTypeToString(type_)
                 << ": Processing add/update with key: " << storage_key;

        sync_pb::PersistedEntityData persisted_entity =
            CreatePersistedFromEntityData(change.data());

        // Purposefully crash if we have client only data, as this could
        // result in storing password in plain text.
        CHECK(!persisted_entity.specifics()
                   .password()
                   .has_client_only_encrypted_data());

        batch->WriteData(storage_key, persisted_entity.SerializeAsString());
        in_memory_store_[storage_key] = persisted_entity;

        // Production code uses a cryptographer only for PASSWORDS.
        if (cryptographer_) {
          const base::Optional<ModelError> error =
              cryptographer_->Decrypt(persisted_entity.mutable_specifics());
          if (error) {
            return error;
          }
        }

        output_sync_change_list->emplace_back(
            FROM_HERE, ConvertToSyncChangeType(change.type()),
            SyncData::CreateRemoteData(
                /*id=*/kInvalidNodeId,
                std::move(*persisted_entity.mutable_specifics()),
                change.data().modification_time,
                change.data().client_tag_hash));

        break;
      }
    }
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&SyncableServiceBasedBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  return base::nullopt;
}

void SyncableServiceBasedBridge::ReportErrorIfSet(
    const base::Optional<ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

base::Optional<ModelError> SyncableServiceBasedBridge::ReencryptEverything(
    ModelTypeStore::WriteBatch* batch) {
  DCHECK(cryptographer_);

  DVLOG(1) << "Encryption key changed: reencrypting all data for "
           << ModelTypeToString(type_) << " with " << in_memory_store_.size()
           << " entities";

  for (std::pair<const std::string, sync_pb::PersistedEntityData>& record :
       in_memory_store_) {
    const std::string& storage_key = record.first;
    sync_pb::EntitySpecifics* specifics = record.second.mutable_specifics();

    base::Optional<ModelError> error;
    error = cryptographer_->Decrypt(specifics);
    if (error) {
      return error;
    }

    error = cryptographer_->Encrypt(specifics);
    if (error) {
      return error;
    }

    // Purposefully crash if we have client only data, as this could
    // result in storing password in plain text.
    CHECK(!specifics->password().has_client_only_encrypted_data());

    batch->WriteData(storage_key, record.second.SerializeAsString());

    // No need to call Put() because the processor will recommit all entries
    // anyway.
  }

  return base::nullopt;
}

base::Optional<ModelError>
SyncableServiceBasedBridge::ApplySyncChangesWithBatch(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_change_list,
    std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_);
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(syncable_service_started_);

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  SyncChangeList sync_change_list;
  const base::Optional<ModelError> error = StoreAndConvertRemoteChanges(
      std::move(batch), std::move(entity_change_list), &sync_change_list);
  if (error) {
    return error;
  }

  if (sync_change_list.empty()) {
    return base::nullopt;
  }

  return ConvertToModelError(
      syncable_service_->ProcessSyncChanges(FROM_HERE, sync_change_list));
}

}  // namespace syncer

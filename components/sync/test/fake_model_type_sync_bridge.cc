// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_model_type_sync_bridge.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::EntityMetadata;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;

namespace syncer {

namespace {

// MetadataChangeList implementaton that forwards writes metadata to a store.
class TestMetadataChangeList : public MetadataChangeList {
 public:
  explicit TestMetadataChangeList(FakeModelTypeSyncBridge::Store* db)
      : db_(db) {}
  ~TestMetadataChangeList() override = default;

  // MetadataChangeList implementation.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override {
    db_->set_model_type_state(model_type_state);
  }

  void ClearModelTypeState() override {
    db_->set_model_type_state(ModelTypeState());
  }

  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override {
    DCHECK(!storage_key.empty());
    db_->PutMetadata(storage_key, metadata);
  }

  void ClearMetadata(const std::string& storage_key) override {
    DCHECK(!storage_key.empty());
    db_->RemoveMetadata(storage_key);
  }

 private:
  raw_ptr<FakeModelTypeSyncBridge::Store> db_;
};

}  // namespace

// static
std::string FakeModelTypeSyncBridge::ClientTagFromKey(const std::string& key) {
  return "ClientTag_" + key;
}

FakeModelTypeSyncBridge::Store::Store() = default;
FakeModelTypeSyncBridge::Store::~Store() = default;

void FakeModelTypeSyncBridge::Store::PutData(const std::string& key,
                                             const EntityData& data) {
  data_change_count_++;
  data_store_[key] = CopyEntityData(data);
}

void FakeModelTypeSyncBridge::Store::PutMetadata(
    const std::string& key,
    const EntityMetadata& metadata) {
  metadata_change_count_++;
  metadata_store_[key] = metadata;
}

void FakeModelTypeSyncBridge::Store::RemoveData(const std::string& key) {
  data_change_count_++;
  data_store_.erase(key);
}

void FakeModelTypeSyncBridge::Store::ClearAllData() {
  data_change_count_++;
  data_store_.clear();
}

void FakeModelTypeSyncBridge::Store::RemoveMetadata(const std::string& key) {
  metadata_change_count_++;
  metadata_store_.erase(key);
}

bool FakeModelTypeSyncBridge::Store::HasData(const std::string& key) const {
  return data_store_.find(key) != data_store_.end();
}

bool FakeModelTypeSyncBridge::Store::HasMetadata(const std::string& key) const {
  return metadata_store_.find(key) != metadata_store_.end();
}

const EntityData& FakeModelTypeSyncBridge::Store::GetData(
    const std::string& key) const {
  DCHECK(data_store_.count(key) != 0) << " for key " << key;
  return *data_store_.find(key)->second;
}

const sync_pb::EntityMetadata& FakeModelTypeSyncBridge::Store::GetMetadata(
    const std::string& key) const {
  return metadata_store_.find(key)->second;
}

std::unique_ptr<MetadataBatch>
FakeModelTypeSyncBridge::Store::CreateMetadataBatch() const {
  auto metadata_batch = std::make_unique<MetadataBatch>();
  metadata_batch->SetModelTypeState(model_type_state_);
  for (const auto& [storage_key, metadata] : metadata_store_) {
    metadata_batch->AddMetadata(
        storage_key, std::make_unique<sync_pb::EntityMetadata>(metadata));
  }
  return metadata_batch;
}

void FakeModelTypeSyncBridge::Store::Reset() {
  data_change_count_ = 0;
  metadata_change_count_ = 0;
  data_store_.clear();
  metadata_store_.clear();
  model_type_state_.Clear();
}

FakeModelTypeSyncBridge::FakeModelTypeSyncBridge(
    ModelType type,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)),
      db_(std::make_unique<Store>()),
      type_(type) {}

FakeModelTypeSyncBridge::~FakeModelTypeSyncBridge() {
  EXPECT_FALSE(error_next_);
}

// Overloaded form to allow passing of custom entity data.
void FakeModelTypeSyncBridge::WriteItem(
    const std::string& key,
    std::unique_ptr<EntityData> entity_data) {
  DCHECK(EntityHasClientTag(*entity_data));
  db_->PutData(key, *entity_data);
  if (change_processor()->IsTrackingMetadata()) {
    std::unique_ptr<MetadataChangeList> change_list =
        CreateMetadataChangeList();
    change_processor()->Put(key, std::move(entity_data), change_list.get());
    ApplyMetadataChangeList(std::move(change_list));
  }
}

void FakeModelTypeSyncBridge::DeleteItem(const std::string& key) {
  db_->RemoveData(key);
  if (change_processor()->IsTrackingMetadata()) {
    std::unique_ptr<MetadataChangeList> change_list =
        CreateMetadataChangeList();
    change_processor()->Delete(key, change_list.get());
    ApplyMetadataChangeList(std::move(change_list));
  }
}

void FakeModelTypeSyncBridge::MimicBugToLooseItemWithoutNotifyingProcessor(
    const std::string& key) {
  db_->RemoveData(key);
}

std::unique_ptr<MetadataChangeList>
FakeModelTypeSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<InMemoryMetadataChangeList>();
}

absl::optional<ModelError> FakeModelTypeSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  if (error_next_) {
    error_next_ = false;
    return ModelError(FROM_HERE, "boom");
  }

  std::set<std::string> remote_storage_keys;
  // Store any new remote entities.
  for (const std::unique_ptr<EntityChange>& change : entity_data) {
    EXPECT_FALSE(change->data().is_deleted());
    EXPECT_EQ(EntityChange::ACTION_ADD, change->type());
    std::string storage_key = change->storage_key();
    EXPECT_NE(SupportsGetStorageKey(), storage_key.empty());
    if (storage_key.empty()) {
      if (type_ == PREFERENCES &&
          base::Contains(values_to_ignore_,
                         change->data().specifics.preference().value())) {
        change_processor()->UntrackEntityForClientTagHash(
            change->data().client_tag_hash);
        continue;
      }

      storage_key = GenerateStorageKey(change->data());
      change_processor()->UpdateStorageKey(change->data(), storage_key,
                                           metadata_change_list.get());
    }
    remote_storage_keys.insert(storage_key);
    DCHECK(EntityHasClientTag(change->data()));
    db_->PutData(storage_key, change->data());
  }

  // Commit any local entities that aren't being overwritten by the server.
  for (const auto& [storage_key, local_entity_data] : db_->all_data()) {
    if (remote_storage_keys.find(storage_key) == remote_storage_keys.end()) {
      change_processor()->Put(storage_key, CopyEntityData(*local_entity_data),
                              metadata_change_list.get());
    }
  }

  ApplyMetadataChangeList(std::move(metadata_change_list));
  return {};
}

absl::optional<ModelError> FakeModelTypeSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_changes,
    EntityChangeList entity_changes) {
  if (error_next_) {
    error_next_ = false;
    return ModelError(FROM_HERE, "boom");
  }

  for (const std::unique_ptr<EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case EntityChange::ACTION_ADD: {
        std::string storage_key = change->storage_key();
        EXPECT_NE(SupportsGetStorageKey(), storage_key.empty());
        if (storage_key.empty()) {
          storage_key = GenerateStorageKey(change->data());
          change_processor()->UpdateStorageKey(change->data(), storage_key,
                                               metadata_changes.get());
        }
        DCHECK(EntityHasClientTag(change->data()));
        EXPECT_FALSE(db_->HasData(storage_key));
        db_->PutData(storage_key, change->data());
        break;
      }
      case EntityChange::ACTION_UPDATE:
        DCHECK(EntityHasClientTag(change->data()));
        EXPECT_TRUE(db_->HasData(change->storage_key()));
        db_->PutData(change->storage_key(), change->data());
        break;
      case EntityChange::ACTION_DELETE:
        EXPECT_TRUE(db_->HasData(change->storage_key()));
        db_->RemoveData(change->storage_key());
        break;
    }
  }
  ApplyMetadataChangeList(std::move(metadata_changes));
  return {};
}

void FakeModelTypeSyncBridge::ApplyMetadataChangeList(
    std::unique_ptr<MetadataChangeList> mcl) {
  InMemoryMetadataChangeList* in_memory_mcl =
      static_cast<InMemoryMetadataChangeList*>(mcl.get());
  // Use TestMetadataChangeList to commit all metadata changes to the store.
  TestMetadataChangeList db_mcl(db_.get());
  in_memory_mcl->TransferChangesTo(&db_mcl);
}

void FakeModelTypeSyncBridge::GetData(StorageKeyList keys,
                                      DataCallback callback) {
  if (error_next_) {
    error_next_ = false;
    change_processor()->ReportError({FROM_HERE, "boom"});
    return;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const std::string& key : keys) {
    if (db_->HasData(key)) {
      batch->Put(key, CopyEntityData(db_->GetData(key)));
    } else {
      DLOG(WARNING) << "No data for " << key;
    }
  }
  std::move(callback).Run(std::move(batch));
}

void FakeModelTypeSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  if (error_next_) {
    error_next_ = false;
    change_processor()->ReportError({FROM_HERE, "boom"});
    return;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& [storage_key, entity_data] : db_->all_data()) {
    batch->Put(storage_key, CopyEntityData(*entity_data));
  }
  std::move(callback).Run(std::move(batch));
}

std::string FakeModelTypeSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  DCHECK(supports_get_client_tag_);
  return ClientTagFromKey(GetStorageKeyInternal(entity_data));
}

std::string FakeModelTypeSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  DCHECK(supports_get_storage_key_);
  return GetStorageKeyInternal(entity_data);
}

std::string FakeModelTypeSyncBridge::GetStorageKeyInternal(
    const EntityData& entity_data) {
  switch (type_) {
    case PREFERENCES:
      return entity_data.specifics.preference().name();
    case USER_EVENTS:
      return base::NumberToString(
          entity_data.specifics.user_event().event_time_usec());
    default:
      // If you need support for more types, add them here.
      NOTREACHED();
  }
  return std::string();
}

std::string FakeModelTypeSyncBridge::GenerateStorageKey(
    const EntityData& entity_data) {
  if (supports_get_storage_key_) {
    return GetStorageKey(entity_data);
  } else {
    return base::NumberToString(++last_generated_storage_key_);
  }
}

bool FakeModelTypeSyncBridge::SupportsGetClientTag() const {
  return supports_get_client_tag_;
}

bool FakeModelTypeSyncBridge::SupportsGetStorageKey() const {
  return supports_get_storage_key_;
}

ConflictResolution FakeModelTypeSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const EntityData& remote_data) const {
  return conflict_resolution_;
}

void FakeModelTypeSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  ModelTypeSyncBridge::ApplyStopSyncChanges(
      std::move(delete_metadata_change_list));
}

sync_pb::EntitySpecifics FakeModelTypeSyncBridge::TrimRemoteSpecificsForCaching(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  if (entity_specifics.unknown_fields().empty()) {
    return sync_pb::EntitySpecifics();
  }

  // Keep top-level unknown fields for testing without specific data type
  // trimming (e.g. in processor unit tests).
  trimmed_specifics_change_count_++;
  sync_pb::EntitySpecifics trimmed_specifics;
  *trimmed_specifics.mutable_unknown_fields() =
      entity_specifics.unknown_fields();
  return trimmed_specifics;
}

void FakeModelTypeSyncBridge::SetConflictResolution(
    ConflictResolution resolution) {
  conflict_resolution_ = resolution;
}

void FakeModelTypeSyncBridge::ErrorOnNextCall() {
  EXPECT_FALSE(error_next_);
  error_next_ = true;
}

std::unique_ptr<EntityData> FakeModelTypeSyncBridge::CopyEntityData(
    const EntityData& old_data) {
  auto new_data = std::make_unique<EntityData>();
  new_data->id = old_data.id;
  new_data->client_tag_hash = old_data.client_tag_hash;
  new_data->name = old_data.name;
  new_data->specifics = old_data.specifics;
  new_data->creation_time = old_data.creation_time;
  new_data->modification_time = old_data.modification_time;
  return new_data;
}

void FakeModelTypeSyncBridge::SetSupportsGetClientTag(
    bool supports_get_client_tag) {
  supports_get_client_tag_ = supports_get_client_tag;
}

bool FakeModelTypeSyncBridge::EntityHasClientTag(const EntityData& entity) {
  return supports_get_client_tag_ || !entity.client_tag_hash.value().empty();
}

void FakeModelTypeSyncBridge::SetSupportsGetStorageKey(
    bool supports_get_storage_key) {
  supports_get_storage_key_ = supports_get_storage_key;
}

std::string FakeModelTypeSyncBridge::GetLastGeneratedStorageKey() const {
  // Verify that GenerateStorageKey() was called at least once.
  EXPECT_NE(0, last_generated_storage_key_);
  return base::NumberToString(last_generated_storage_key_);
}

void FakeModelTypeSyncBridge::AddPrefValueToIgnore(const std::string& value) {
  DCHECK_EQ(type_, PREFERENCES);
  values_to_ignore_.insert(value);
}

}  // namespace syncer

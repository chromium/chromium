// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_data_type_sync_bridge.h"

#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using sync_pb::EntitySpecifics;

namespace syncer {

namespace {

// MetadataChangeList implementaton that forwards writes metadata to a store.
class TestMetadataChangeList : public MetadataChangeList {
 public:
  explicit TestMetadataChangeList(FakeDataTypeSyncBridge::Store* db)
      : db_(db) {}
  ~TestMetadataChangeList() override = default;

  // MetadataChangeList implementation.
  void UpdateDataTypeState(
      const sync_pb::DataTypeState& data_type_state) override {
    db_->set_data_type_state(data_type_state);
  }

  void ClearDataTypeState() override {
    db_->set_data_type_state(DataTypeState());
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
  const raw_ptr<FakeDataTypeSyncBridge::Store> db_;
};

}  // namespace

// static
std::string FakeDataTypeSyncBridge::ClientTagFromKey(const std::string& key) {
  return "ClientTag_" + key;
}

FakeDataTypeSyncBridge::Store::Store() = default;
FakeDataTypeSyncBridge::Store::~Store() = default;

void FakeDataTypeSyncBridge::Store::PutData(const std::string& key,
                                            const EntityData& data) {
  data_change_count_++;
  data_store_[key] = CopyEntityData(data);
}

void FakeDataTypeSyncBridge::Store::PutMetadata(
    const std::string& key,
    const EntityMetadata& metadata) {
  metadata_change_count_++;
  metadata_store_[key] = metadata;
}

void FakeDataTypeSyncBridge::Store::RemoveData(const std::string& key) {
  data_change_count_++;
  data_store_.erase(key);
}

void FakeDataTypeSyncBridge::Store::ClearAllData() {
  data_change_count_++;
  data_store_.clear();
}

void FakeDataTypeSyncBridge::Store::RemoveMetadata(const std::string& key) {
  metadata_change_count_++;
  metadata_store_.erase(key);
}

bool FakeDataTypeSyncBridge::Store::HasData(const std::string& key) const {
  return data_store_.find(key) != data_store_.end();
}

bool FakeDataTypeSyncBridge::Store::HasMetadata(const std::string& key) const {
  return metadata_store_.find(key) != metadata_store_.end();
}

const EntityData& FakeDataTypeSyncBridge::Store::GetData(
    const std::string& key) const {
  DCHECK(data_store_.count(key) != 0) << " for key " << key;
  return *data_store_.find(key)->second;
}

const sync_pb::EntityMetadata& FakeDataTypeSyncBridge::Store::GetMetadata(
    const std::string& key) const {
  return metadata_store_.find(key)->second;
}

std::unique_ptr<MetadataBatch>
FakeDataTypeSyncBridge::Store::CreateMetadataBatch() const {
  auto metadata_batch = std::make_unique<MetadataBatch>();
  metadata_batch->SetDataTypeState(data_type_state_);
  for (const auto& [storage_key, metadata] : metadata_store_) {
    metadata_batch->AddMetadata(
        storage_key, std::make_unique<sync_pb::EntityMetadata>(metadata));
  }
  return metadata_batch;
}

void FakeDataTypeSyncBridge::Store::Reset() {
  data_change_count_ = 0;
  metadata_change_count_ = 0;
  data_store_.clear();
  metadata_store_.clear();
  data_type_state_.Clear();
}

FakeDataTypeSyncBridge::FakeDataTypeSyncBridge(
    DataType type,
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)),
      db_(std::make_unique<Store>()),
      type_(type) {}

FakeDataTypeSyncBridge::~FakeDataTypeSyncBridge() {
  EXPECT_FALSE(error_next_);
}

// Overloaded form to allow passing of custom entity data.
void FakeDataTypeSyncBridge::WriteItem(
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

void FakeDataTypeSyncBridge::DeleteItem(const std::string& key) {
  db_->RemoveData(key);
  if (change_processor()->IsTrackingMetadata()) {
    std::unique_ptr<MetadataChangeList> change_list =
        CreateMetadataChangeList();
    change_processor()->Delete(key, syncer::DeletionOrigin::Unspecified(),
                               change_list.get());
    ApplyMetadataChangeList(std::move(change_list));
  }
}

void FakeDataTypeSyncBridge::MimicBugToLooseItemWithoutNotifyingProcessor(
    const std::string& key) {
  db_->RemoveData(key);
}

std::unique_ptr<MetadataChangeList>
FakeDataTypeSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<InMemoryMetadataChangeList>();
}

std::optional<ModelError> FakeDataTypeSyncBridge::MergeFullSyncData(
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
          values_to_ignore_.contains(
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

std::optional<ModelError> FakeDataTypeSyncBridge::ApplyIncrementalSyncChanges(
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
        if (change->is_deleted_collaboration_membership()) {
          deleted_collaboration_membership_storage_keys_.insert(
              change->storage_key());
        }
        break;
    }
  }
  ApplyMetadataChangeList(std::move(metadata_changes));
  return {};
}

void FakeDataTypeSyncBridge::ApplyMetadataChangeList(
    std::unique_ptr<MetadataChangeList> mcl) {
  InMemoryMetadataChangeList* in_memory_mcl =
      static_cast<InMemoryMetadataChangeList*>(mcl.get());
  // Use TestMetadataChangeList to commit all metadata changes to the store.
  TestMetadataChangeList db_mcl(db_.get());
  in_memory_mcl->TransferChangesTo(&db_mcl);
}

std::unique_ptr<DataBatch> FakeDataTypeSyncBridge::GetDataForCommit(
    StorageKeyList keys) {
  if (error_next_) {
    error_next_ = false;
    change_processor()->ReportError({FROM_HERE, "boom"});
    return nullptr;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const std::string& key : keys) {
    if (db_->HasData(key)) {
      batch->Put(key, CopyEntityData(db_->GetData(key)));
    } else {
      DLOG(WARNING) << "No data for " << key;
    }
  }
  return batch;
}

std::unique_ptr<DataBatch> FakeDataTypeSyncBridge::GetAllDataForDebugging() {
  if (error_next_) {
    error_next_ = false;
    change_processor()->ReportError({FROM_HERE, "boom"});
    return nullptr;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& [storage_key, entity_data] : db_->all_data()) {
    batch->Put(storage_key, CopyEntityData(*entity_data));
  }
  return batch;
}

std::string FakeDataTypeSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  DCHECK(supports_get_client_tag_);
  return ClientTagFromKey(GetStorageKeyInternal(entity_data));
}

std::string FakeDataTypeSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  DCHECK(supports_get_storage_key_);
  return GetStorageKeyInternal(entity_data);
}

std::string FakeDataTypeSyncBridge::GetStorageKeyInternal(
    const EntityData& entity_data) {
  switch (type_) {
    case PREFERENCES:
      return entity_data.specifics.preference().name();
    case USER_EVENTS:
      return base::NumberToString(
          entity_data.specifics.user_event().event_time_usec());
    case SHARED_TAB_GROUP_DATA:
      return entity_data.specifics.shared_tab_group_data().guid();
    default:
      // If you need support for more types, add them here.
      NOTREACHED_IN_MIGRATION();
  }
  return std::string();
}

std::string FakeDataTypeSyncBridge::GenerateStorageKey(
    const EntityData& entity_data) {
  if (supports_get_storage_key_) {
    return GetStorageKey(entity_data);
  } else {
    return base::NumberToString(++last_generated_storage_key_);
  }
}

bool FakeDataTypeSyncBridge::SupportsGetClientTag() const {
  return supports_get_client_tag_;
}

bool FakeDataTypeSyncBridge::SupportsGetStorageKey() const {
  return supports_get_storage_key_;
}

bool FakeDataTypeSyncBridge::SupportsUniquePositions() const {
  return !extract_unique_positions_callback_.is_null();
}

sync_pb::UniquePosition FakeDataTypeSyncBridge::GetUniquePosition(
    const sync_pb::EntitySpecifics& specifics) const {
  return extract_unique_positions_callback_.Run(specifics);
}

ConflictResolution FakeDataTypeSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const EntityData& remote_data) const {
  return conflict_resolution_;
}

sync_pb::EntitySpecifics
FakeDataTypeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
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

bool FakeDataTypeSyncBridge::IsEntityDataValid(
    const EntityData& entity_data) const {
  return invalid_remote_updates_.find(entity_data.client_tag_hash) ==
         invalid_remote_updates_.end();
}

void FakeDataTypeSyncBridge::SetConflictResolution(
    ConflictResolution resolution) {
  conflict_resolution_ = resolution;
}

void FakeDataTypeSyncBridge::ErrorOnNextCall() {
  EXPECT_FALSE(error_next_);
  error_next_ = true;
}

std::unique_ptr<EntityData> FakeDataTypeSyncBridge::CopyEntityData(
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

void FakeDataTypeSyncBridge::SetSupportsGetClientTag(
    bool supports_get_client_tag) {
  supports_get_client_tag_ = supports_get_client_tag;
}

bool FakeDataTypeSyncBridge::EntityHasClientTag(const EntityData& entity) {
  return supports_get_client_tag_ || !entity.client_tag_hash.value().empty();
}

void FakeDataTypeSyncBridge::SetSupportsGetStorageKey(
    bool supports_get_storage_key) {
  supports_get_storage_key_ = supports_get_storage_key;
}

std::string FakeDataTypeSyncBridge::GetLastGeneratedStorageKey() const {
  // Verify that GenerateStorageKey() was called at least once.
  EXPECT_NE(0, last_generated_storage_key_);
  return base::NumberToString(last_generated_storage_key_);
}

void FakeDataTypeSyncBridge::AddPrefValueToIgnore(const std::string& value) {
  DCHECK_EQ(type_, PREFERENCES);
  values_to_ignore_.insert(value);
}

void FakeDataTypeSyncBridge::TreatRemoteUpdateAsInvalid(
    const ClientTagHash& client_tag_hash) {
  invalid_remote_updates_.insert(client_tag_hash);
}

void FakeDataTypeSyncBridge::EnableUniquePositionSupport(
    ExtractUniquePositionCallback callback) {
  extract_unique_positions_callback_ = std::move(callback);
}

}  // namespace syncer

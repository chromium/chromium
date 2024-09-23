// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/blocking_data_type_store_impl.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "components/sync/model/data_type_store_backend.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer {

namespace {

// Used for data and metadata explicitly linked to a server-side account. Note
// that this prefix is used *before* the datatype's root tag, which also means
// it shouldn't be a substring or superstring of root tags. Conveniently, root
// tags are guaranteed to be lowercase.
const char kAccountStoragePrefix[] = "A-";

// Key prefix for data/metadata records.
const char kDataPrefix[] = "-dt-";
const char kMetadataPrefix[] = "-md-";

// Key for global metadata record.
const char kGlobalMetadataKey[] = "-GlobalMetadata";

class LevelDbMetadataChangeList : public MetadataChangeList {
 public:
  LevelDbMetadataChangeList(DataType data_type,
                            StorageType storage_type,
                            leveldb::WriteBatch* leveldb_write_batch)
      : leveldb_write_batch_(leveldb_write_batch),
        metadata_prefix_(FormatMetaPrefix(data_type, storage_type)),
        global_metadata_key_(FormatGlobalMetadataKey(data_type, storage_type)) {
    DCHECK(leveldb_write_batch_);
  }

  // MetadataChangeList implementation.
  void UpdateDataTypeState(
      const sync_pb::DataTypeState& data_type_state) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    leveldb_write_batch_->Put(global_metadata_key_,
                              data_type_state.SerializeAsString());
  }

  void ClearDataTypeState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    leveldb_write_batch_->Delete(global_metadata_key_);
  }

  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    leveldb_write_batch_->Put(FormatMetadataKey(storage_key),
                              metadata.SerializeAsString());
  }

  void ClearMetadata(const std::string& storage_key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    leveldb_write_batch_->Delete(FormatMetadataKey(storage_key));
  }

 private:
  // Format key for metadata records with given id.
  std::string FormatMetadataKey(const std::string& id) const {
    return metadata_prefix_ + id;
  }

  const raw_ptr<leveldb::WriteBatch, DanglingUntriaged> leveldb_write_batch_;

  // Key for this type's metadata records.
  const std::string metadata_prefix_;
  const std::string global_metadata_key_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class LevelDbWriteBatch : public BlockingDataTypeStoreImpl::WriteBatch {
 public:
  static std::unique_ptr<leveldb::WriteBatch> ToLevelDbWriteBatch(
      std::unique_ptr<LevelDbWriteBatch> batch) {
    return std::move(batch->leveldb_write_batch_);
  }

  LevelDbWriteBatch(DataType data_type, StorageType storage_type)
      : type_(data_type),
        data_prefix_(FormatDataPrefix(data_type, storage_type)),
        leveldb_write_batch_(std::make_unique<leveldb::WriteBatch>()),
        metadata_change_list_(data_type,
                              storage_type,
                              leveldb_write_batch_.get()) {}

  ~LevelDbWriteBatch() override = default;

  DataType GetDataType() const { return type_; }

  // WriteBatch implementation.
  void WriteData(const std::string& id, const std::string& value) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    leveldb_write_batch_->Put(FormatDataKey(id), value);
  }

  void DeleteData(const std::string& id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    leveldb_write_batch_->Delete(FormatDataKey(id));
  }

  MetadataChangeList* GetMetadataChangeList() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &metadata_change_list_;
  }

 private:
  // Format key for data records with given id.
  std::string FormatDataKey(const std::string& id) const {
    return data_prefix_ + id;
  }

  const DataType type_;

  // Key prefix for data records of this data type.
  const std::string data_prefix_;

  std::unique_ptr<leveldb::WriteBatch> leveldb_write_batch_;
  LevelDbMetadataChangeList metadata_change_list_;

  SEQUENCE_CHECKER(sequence_checker_);
};

std::string GetStorageTypePrefix(StorageType storage_type) {
  switch (storage_type) {
    case StorageType::kUnspecified:
      // Historically no prefix was used for the default storage, and that
      // continues to be the case to avoid data migrations.
      return "";
    case StorageType::kAccount:
      return kAccountStoragePrefix;
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

}  // namespace

// Formats key prefix for data records of |data_type| using |storage_type|.
std::string FormatDataPrefix(DataType data_type, StorageType storage_type) {
  return base::StrCat(
      {BlockingDataTypeStoreImpl::FormatPrefixForDataTypeAndStorageType(
           data_type, storage_type),
       kDataPrefix});
}

// Formats key prefix for metadata records of |data_type| using |storage_type|.
std::string FormatMetaPrefix(DataType data_type, StorageType storage_type) {
  return base::StrCat(
      {BlockingDataTypeStoreImpl::FormatPrefixForDataTypeAndStorageType(
           data_type, storage_type),
       kMetadataPrefix});
}

// Formats key for global metadata record of |data_type| using |storage_type|.
std::string FormatGlobalMetadataKey(DataType data_type,
                                    StorageType storage_type) {
  return base::StrCat(
      {BlockingDataTypeStoreImpl::FormatPrefixForDataTypeAndStorageType(
           data_type, storage_type),
       kGlobalMetadataKey});
}

BlockingDataTypeStoreImpl::BlockingDataTypeStoreImpl(
    DataType data_type,
    StorageType storage_type,
    scoped_refptr<DataTypeStoreBackend> backend)
    : data_type_(data_type),
      storage_type_(storage_type),
      backend_(std::move(backend)),
      data_prefix_(FormatDataPrefix(data_type, storage_type)),
      metadata_prefix_(FormatMetaPrefix(data_type, storage_type)),
      global_metadata_key_(FormatGlobalMetadataKey(data_type, storage_type)) {
  DCHECK(backend_);
}

BlockingDataTypeStoreImpl::~BlockingDataTypeStoreImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<ModelError> BlockingDataTypeStoreImpl::ReadData(
    const IdList& id_list,
    RecordList* data_records,
    IdList* missing_id_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_records);
  DCHECK(missing_id_list);
  return backend_->ReadRecordsWithPrefix(data_prefix_, id_list, data_records,
                                         missing_id_list);
}

std::optional<ModelError> BlockingDataTypeStoreImpl::ReadAllData(
    RecordList* data_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_records);
  return backend_->ReadAllRecordsWithPrefix(data_prefix_, data_records);
}

std::optional<ModelError> BlockingDataTypeStoreImpl::ReadAllMetadata(
    MetadataBatch* metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_batch);

  // Read global metadata.
  RecordList global_metadata_records;
  IdList missing_global_metadata_id;
  std::optional<ModelError> error = backend_->ReadRecordsWithPrefix(
      /*prefix=*/std::string(), {global_metadata_key_},
      &global_metadata_records, &missing_global_metadata_id);
  if (error.has_value()) {
    return error;
  }

  // If global metadata is missing, no need to read further.
  if (!missing_global_metadata_id.empty()) {
    // Missing global metadata record is not an error; we can just return the
    // default instance.
    DCHECK_EQ(global_metadata_key_, missing_global_metadata_id[0]);
    DCHECK(global_metadata_records.empty());
  } else {
    sync_pb::DataTypeState state;
    if (!state.ParseFromString(global_metadata_records[0].value)) {
      return ModelError(FROM_HERE, "Failed to deserialize data type state.");
    }
    metadata_batch->SetDataTypeState(state);
  }

  // Read individual metadata records.
  RecordList metadata_records;
  error =
      backend_->ReadAllRecordsWithPrefix(metadata_prefix_, &metadata_records);
  if (error.has_value()) {
    return error;
  }

  for (const Record& r : metadata_records) {
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (!entity_metadata->ParseFromString(r.value)) {
      return ModelError(FROM_HERE, "Failed to deserialize entity metadata.");
    }
    metadata_batch->AddMetadata(r.id, std::move(entity_metadata));
  }

  return std::nullopt;
}

std::unique_ptr<BlockingDataTypeStoreImpl::WriteBatch>
BlockingDataTypeStoreImpl::CreateWriteBatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateWriteBatch(data_type_, storage_type_);
}

std::optional<ModelError> BlockingDataTypeStoreImpl::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(write_batch);
  std::unique_ptr<LevelDbWriteBatch> write_batch_impl(
      static_cast<LevelDbWriteBatch*>(write_batch.release()));
  DCHECK_EQ(write_batch_impl->GetDataType(), data_type_);
  return backend_->WriteModifications(
      LevelDbWriteBatch::ToLevelDbWriteBatch(std::move(write_batch_impl)));
}

std::optional<ModelError>
BlockingDataTypeStoreImpl::DeleteAllDataAndMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backend_->DeleteDataAndMetadataForPrefix(
      base::StrCat({GetStorageTypePrefix(storage_type_),
                    DataTypeToStableLowerCaseString(data_type_)}));
}

// static
std::unique_ptr<BlockingDataTypeStoreImpl::WriteBatch>
BlockingDataTypeStoreImpl::CreateWriteBatch(DataType data_type,
                                            StorageType storage_type) {
  return std::make_unique<LevelDbWriteBatch>(data_type, storage_type);
}

// static
std::string BlockingDataTypeStoreImpl::FormatPrefixForDataTypeAndStorageType(
    DataType data_type,
    StorageType storage_type) {
  return base::StrCat({GetStorageTypePrefix(storage_type),
                       DataTypeToStableLowerCaseString(data_type)});
}

}  // namespace syncer

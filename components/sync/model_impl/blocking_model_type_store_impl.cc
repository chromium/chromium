// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/blocking_model_type_store_impl.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model_impl/model_type_store_backend.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer {

namespace {

// Key prefix for data/metadata records.
const char kDataPrefix[] = "-dt-";
const char kMetadataPrefix[] = "-md-";

// Key for global metadata record.
const char kGlobalMetadataKey[] = "-GlobalMetadata";

// Formats key prefix for data records of |type|.
std::string FormatDataPrefix(ModelType type) {
  return std::string(GetModelTypeRootTag(type)) + kDataPrefix;
}

// Formats key prefix for metadata records of |type|.
std::string FormatMetaPrefix(ModelType type) {
  return std::string(GetModelTypeRootTag(type)) + kMetadataPrefix;
}

// Formats key for global metadata record of |type|.
std::string FormatGlobalMetadataKey(ModelType type) {
  return std::string(GetModelTypeRootTag(type)) + kGlobalMetadataKey;
}

class LevelDbMetadataChangeList : public MetadataChangeList {
 public:
  LevelDbMetadataChangeList(ModelType type,
                            leveldb::WriteBatch* leveldb_write_batch)
      : leveldb_write_batch_(leveldb_write_batch),
        metadata_prefix_(FormatMetaPrefix(type)),
        global_metadata_key_(FormatGlobalMetadataKey(type)) {
    DCHECK(leveldb_write_batch_);
  }

  // MetadataChangeList implementation.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    leveldb_write_batch_->Put(global_metadata_key_,
                              model_type_state.SerializeAsString());
  }

  void ClearModelTypeState() override {
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

  leveldb::WriteBatch* const leveldb_write_batch_;

  // Key for this type's metadata records.
  const std::string metadata_prefix_;
  const std::string global_metadata_key_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class LevelDbWriteBatch : public BlockingModelTypeStoreImpl::WriteBatch {
 public:
  static std::unique_ptr<leveldb::WriteBatch> ToLevelDbWriteBatch(
      std::unique_ptr<LevelDbWriteBatch> batch) {
    return std::move(batch->leveldb_write_batch_);
  }

  explicit LevelDbWriteBatch(ModelType type)
      : type_(type),
        data_prefix_(FormatDataPrefix(type)),
        leveldb_write_batch_(std::make_unique<leveldb::WriteBatch>()),
        metadata_change_list_(type, leveldb_write_batch_.get()) {}

  ~LevelDbWriteBatch() override {}

  ModelType GetModelType() const { return type_; }

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

  const ModelType type_;

  // Key prefix for data records of this model type.
  const std::string data_prefix_;

  std::unique_ptr<leveldb::WriteBatch> leveldb_write_batch_;
  LevelDbMetadataChangeList metadata_change_list_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

BlockingModelTypeStoreImpl::BlockingModelTypeStoreImpl(
    ModelType type,
    scoped_refptr<ModelTypeStoreBackend> backend)
    : type_(type),
      backend_(std::move(backend)),
      data_prefix_(FormatDataPrefix(type)),
      metadata_prefix_(FormatMetaPrefix(type)),
      global_metadata_key_(FormatGlobalMetadataKey(type)) {
  DCHECK(backend_);
}

BlockingModelTypeStoreImpl::~BlockingModelTypeStoreImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::Optional<ModelError> BlockingModelTypeStoreImpl::ReadData(
    const IdList& id_list,
    RecordList* data_records,
    IdList* missing_id_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_records);
  DCHECK(missing_id_list);
  return backend_->ReadRecordsWithPrefix(data_prefix_, id_list, data_records,
                                         missing_id_list);
}

base::Optional<ModelError> BlockingModelTypeStoreImpl::ReadAllData(
    RecordList* data_records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_records);
  return backend_->ReadAllRecordsWithPrefix(data_prefix_, data_records);
}

base::Optional<ModelError> BlockingModelTypeStoreImpl::ReadAllMetadata(
    MetadataBatch* metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_batch);

  // Read global metadata.
  RecordList global_metadata_records;
  IdList missing_global_metadata_id;
  base::Optional<ModelError> error = backend_->ReadRecordsWithPrefix(
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
    sync_pb::ModelTypeState state;
    if (!state.ParseFromString(global_metadata_records[0].value)) {
      return ModelError(FROM_HERE, "Failed to deserialize model type state.");
    }
    metadata_batch->SetModelTypeState(state);
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

  return base::nullopt;
}

std::unique_ptr<BlockingModelTypeStoreImpl::WriteBatch>
BlockingModelTypeStoreImpl::CreateWriteBatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateWriteBatchForType(type_);
}

base::Optional<ModelError> BlockingModelTypeStoreImpl::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(write_batch);
  std::unique_ptr<LevelDbWriteBatch> write_batch_impl(
      static_cast<LevelDbWriteBatch*>(write_batch.release()));
  DCHECK_EQ(write_batch_impl->GetModelType(), type_);
  UMA_HISTOGRAM_ENUMERATION("Sync.ModelTypeStoreCommitCount",
                            ModelTypeHistogramValue(type_));
  return backend_->WriteModifications(
      LevelDbWriteBatch::ToLevelDbWriteBatch(std::move(write_batch_impl)));
}

base::Optional<ModelError>
BlockingModelTypeStoreImpl::DeleteAllDataAndMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backend_->DeleteDataAndMetadataForPrefix(GetModelTypeRootTag(type_));
}

// static
std::unique_ptr<BlockingModelTypeStoreImpl::WriteBatch>
BlockingModelTypeStoreImpl::CreateWriteBatchForType(ModelType type) {
  return std::make_unique<LevelDbWriteBatch>(type);
}

}  // namespace syncer

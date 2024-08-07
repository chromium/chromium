// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_BLOCKING_DATA_TYPE_STORE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_BLOCKING_DATA_TYPE_STORE_IMPL_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/blocking_data_type_store.h"

namespace syncer {

// TODO(andreaorru): The following functions are public only
// to support Lacros migration. Make them private again once
// they are not needed anymore. See crbug.com/1147556 for more
// context on move migration.

// Formats key prefix for data records of |data_type| using |storage_type|.
std::string FormatDataPrefix(DataType data_type, StorageType storage_type);

// Formats key prefix for metadata records of |data_type| using |storage_type|.
std::string FormatMetaPrefix(DataType data_type, StorageType storage_type);

// Formats key for global metadata record of |data_type| using |storage_type|.
std::string FormatGlobalMetadataKey(DataType data_type,
                                    StorageType storage_type);

class DataTypeStoreBackend;

class BlockingDataTypeStoreImpl : public BlockingDataTypeStore {
 public:
  // |backend| must not be null.
  BlockingDataTypeStoreImpl(DataType data_type,
                            StorageType storage_type,
                            scoped_refptr<DataTypeStoreBackend> backend);

  BlockingDataTypeStoreImpl(const BlockingDataTypeStoreImpl&) = delete;
  BlockingDataTypeStoreImpl& operator=(const BlockingDataTypeStoreImpl&) =
      delete;

  ~BlockingDataTypeStoreImpl() override;

  // BlockingDataTypeStore implementation.
  std::optional<ModelError> ReadData(const IdList& id_list,
                                     RecordList* data_records,
                                     IdList* missing_id_list) override;
  std::optional<ModelError> ReadAllData(RecordList* data_records) override;
  std::optional<ModelError> ReadAllMetadata(
      MetadataBatch* metadata_batch) override;
  std::unique_ptr<WriteBatch> CreateWriteBatch() override;
  std::optional<ModelError> CommitWriteBatch(
      std::unique_ptr<WriteBatch> write_batch) override;
  std::optional<ModelError> DeleteAllDataAndMetadata() override;

  // For advanced uses that require cross-thread batch posting. Avoid if
  // possible.
  static std::unique_ptr<WriteBatch> CreateWriteBatch(DataType data_type,
                                                      StorageType storage_type);

  // Returns the common prefix for all records (data, metadata, and global
  // metadata aka data type state) with a given DataType and StorageType. Can
  // be useful for data migrations; should not be required otherwise.
  static std::string FormatPrefixForDataTypeAndStorageType(
      DataType data_type,
      StorageType storage_type);

 private:
  const DataType data_type_;
  const StorageType storage_type_;
  const scoped_refptr<DataTypeStoreBackend> backend_;

  // Key prefix for data/metadata records of this data type.
  const std::string data_prefix_;
  const std::string metadata_prefix_;

  // Key for this type's global metadata record.
  const std::string global_metadata_key_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_BLOCKING_DATA_TYPE_STORE_IMPL_H_

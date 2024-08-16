// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_IMPL_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/data_type_store.h"

namespace syncer {

class BlockingDataTypeStoreImpl;

// DataTypeStoreImpl handles details of store initialization and threading.
// Actual leveldb IO calls are performed in BlockingDataTypeStoreImpl (in the
// underlying DataTypeStoreBackend).
class DataTypeStoreImpl : public DataTypeStore {
 public:
  // |backend_store| must not be null and must have been created in
  // |backend_task_runner|.
  DataTypeStoreImpl(
      DataType data_type,
      StorageType storage_type,
      std::unique_ptr<BlockingDataTypeStoreImpl, base::OnTaskRunnerDeleter>
          backend_store,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  DataTypeStoreImpl(const DataTypeStoreImpl&) = delete;
  DataTypeStoreImpl& operator=(const DataTypeStoreImpl&) = delete;

  ~DataTypeStoreImpl() override;

  // DataTypeStore implementation.
  void ReadData(const IdList& id_list, ReadDataCallback callback) override;
  void ReadAllData(ReadAllDataCallback callback) override;
  void ReadAllMetadata(ReadMetadataCallback callback) override;
  void ReadAllDataAndMetadata(ReadAllDataAndMetadataCallback callback) override;
  void ReadAllDataAndPreprocess(
      PreprocessCallback preprocess_on_backend_sequence_callback,
      CallbackWithResult completion_on_frontend_sequence_callback) override;
  std::unique_ptr<WriteBatch> CreateWriteBatch() override;
  void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                        CallbackWithResult callback) override;
  void DeleteAllDataAndMetadata(CallbackWithResult callback) override;

 private:
  // Callbacks for different calls to DataTypeStoreBackend.
  void ReadDataDone(ReadDataCallback callback,
                    std::unique_ptr<RecordList> record_list,
                    std::unique_ptr<IdList> missing_id_list,
                    const std::optional<ModelError>& error);
  void ReadAllDataDone(ReadAllDataCallback callback,
                       std::unique_ptr<RecordList> record_list,
                       const std::optional<ModelError>& error);
  void ReadMetadataAfterReadAllDataDone(
      ReadAllDataAndMetadataCallback callback,
      const std::optional<ModelError>& error,
      std::unique_ptr<RecordList> record_list);
  void ReadAllDataAndMetadataDone(
      ReadAllDataAndMetadataCallback callback,
      std::unique_ptr<RecordList> record_list,
      const std::optional<ModelError>& error,
      std::unique_ptr<MetadataBatch> metadata_batch);
  void ReadAllMetadataDone(ReadMetadataCallback callback,
                           std::unique_ptr<MetadataBatch> metadata_batch,
                           const std::optional<ModelError>& error);
  void ReadAllDataAndPreprocessDone(CallbackWithResult callback,
                                    const std::optional<ModelError>& error);
  void WriteModificationsDone(CallbackWithResult callback,
                              const std::optional<ModelError>& error);

  const DataType data_type_;
  const StorageType storage_type_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  // |backend_store_| should be deleted on backend thread.
  std::unique_ptr<BlockingDataTypeStoreImpl, base::OnTaskRunnerDeleter>
      backend_store_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataTypeStoreImpl> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_IMPL_H_

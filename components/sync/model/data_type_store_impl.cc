// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_store_impl.h"

#include <map>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/model/blocking_data_type_store_impl.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"

namespace syncer {

namespace {

std::optional<ModelError> ReadAllDataAndPreprocessOnBackendSequence(
    BlockingDataTypeStoreImpl* blocking_store,
    DataTypeStore::PreprocessCallback preprocess_on_backend_sequence_callback) {
  DCHECK(blocking_store);

  auto record_list = std::make_unique<DataTypeStoreBase::RecordList>();
  std::optional<ModelError> error =
      blocking_store->ReadAllData(record_list.get());
  if (error) {
    return error;
  }

  return std::move(preprocess_on_backend_sequence_callback)
      .Run(std::move(record_list));
}

}  // namespace

DataTypeStoreImpl::DataTypeStoreImpl(
    DataType data_type,
    StorageType storage_type,
    std::unique_ptr<BlockingDataTypeStoreImpl, base::OnTaskRunnerDeleter>
        backend_store,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : data_type_(data_type),
      storage_type_(storage_type),
      backend_task_runner_(std::move(backend_task_runner)),
      backend_store_(std::move(backend_store)) {
  DCHECK(backend_store_);
  DCHECK(backend_task_runner_);
}

DataTypeStoreImpl::~DataTypeStoreImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Note on pattern for communicating with backend:
//  - API function (e.g. ReadData) allocates lists for output.
//  - API function prepares two callbacks: task that will be posted on backend
//    thread and reply which will be posted on data type thread once task
//    finishes.
//  - Task for backend thread takes raw pointers to output lists while reply
//    takes ownership of those lists. This allows backend interface to be simple
//    while ensuring proper objects' lifetime.
//  - Function bound by reply calls consumer's callback and passes ownership of
//    output lists to it.

void DataTypeStoreImpl::ReadData(const IdList& id_list,
                                 ReadDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  std::unique_ptr<IdList> missing_id_list(new IdList());

  auto task = base::BindOnce(&BlockingDataTypeStore::ReadData,
                             base::Unretained(backend_store_.get()), id_list,
                             base::Unretained(record_list.get()),
                             base::Unretained(missing_id_list.get()));
  auto reply = base::BindOnce(
      &DataTypeStoreImpl::ReadDataDone, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(record_list), std::move(missing_id_list));
  backend_task_runner_->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                   std::move(reply));
}

void DataTypeStoreImpl::ReadDataDone(ReadDataCallback callback,
                                     std::unique_ptr<RecordList> record_list,
                                     std::unique_ptr<IdList> missing_id_list,
                                     const std::optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error, std::move(record_list),
                          std::move(missing_id_list));
}

void DataTypeStoreImpl::ReadAllData(ReadAllDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  auto task = base::BindOnce(&BlockingDataTypeStore::ReadAllData,
                             base::Unretained(backend_store_.get()),
                             base::Unretained(record_list.get()));
  auto reply = base::BindOnce(&DataTypeStoreImpl::ReadAllDataDone,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), std::move(record_list));
  backend_task_runner_->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                   std::move(reply));
}

void DataTypeStoreImpl::ReadAllDataDone(
    ReadAllDataCallback callback,
    std::unique_ptr<RecordList> record_list,
    const std::optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error, std::move(record_list));
}

void DataTypeStoreImpl::ReadAllMetadata(ReadMetadataCallback callback) {
  TRACE_EVENT0("sync", "DataTypeStoreImpl::ReadAllMetadata");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  auto metadata_batch = std::make_unique<MetadataBatch>();
  auto task = base::BindOnce(&BlockingDataTypeStore::ReadAllMetadata,
                             base::Unretained(backend_store_.get()),
                             base::Unretained(metadata_batch.get()));
  auto reply = base::BindOnce(&DataTypeStoreImpl::ReadAllMetadataDone,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), std::move(metadata_batch));
  backend_task_runner_->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                   std::move(reply));
}

void DataTypeStoreImpl::ReadAllMetadataDone(
    ReadMetadataCallback callback,
    std::unique_ptr<MetadataBatch> metadata_batch,
    const std::optional<ModelError>& error) {
  TRACE_EVENT0("sync", "DataTypeStoreImpl::ReadAllMetadataDone");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    std::move(callback).Run(*error, std::make_unique<MetadataBatch>());
    return;
  }

  std::move(callback).Run({}, std::move(metadata_batch));
}

void DataTypeStoreImpl::ReadAllDataAndMetadata(
    ReadAllDataAndMetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  // Read data and metadata by calling `ReadAllData()` and `ReadAllMetadata()`
  // in sequence - aborting early if an error occurs.
  ReadAllData(
      base::BindOnce(&DataTypeStoreImpl::ReadMetadataAfterReadAllDataDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DataTypeStoreImpl::ReadMetadataAfterReadAllDataDone(
    ReadAllDataAndMetadataCallback callback,
    const std::optional<ModelError>& error,
    std::unique_ptr<RecordList> record_list) {
  if (error) {
    std::move(callback).Run(error, std::make_unique<RecordList>(),
                            std::make_unique<MetadataBatch>());
    return;
  }
  ReadAllMetadata(base::BindOnce(&DataTypeStoreImpl::ReadAllDataAndMetadataDone,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback), std::move(record_list)));
}

void DataTypeStoreImpl::ReadAllDataAndMetadataDone(
    ReadAllDataAndMetadataCallback callback,
    std::unique_ptr<RecordList> record_list,
    const std::optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  if (error) {
    std::move(callback).Run(error, std::make_unique<RecordList>(),
                            std::make_unique<MetadataBatch>());
  } else {
    std::move(callback).Run(std::nullopt, std::move(record_list),
                            std::move(metadata_batch));
  }
}

void DataTypeStoreImpl::ReadAllDataAndPreprocess(
    PreprocessCallback preprocess_on_backend_sequence_callback,
    CallbackWithResult completion_on_frontend_sequence_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!preprocess_on_backend_sequence_callback.is_null());
  DCHECK(!completion_on_frontend_sequence_callback.is_null());
  auto task =
      base::BindOnce(&ReadAllDataAndPreprocessOnBackendSequence,
                     base::Unretained(backend_store_.get()),
                     std::move(preprocess_on_backend_sequence_callback));
  // ReadAllDataAndPreprocessDone() is only needed to guarantee that callbacks
  // get cancelled if |this| gets destroyed.
  auto reply =
      base::BindOnce(&DataTypeStoreImpl::ReadAllDataAndPreprocessDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_on_frontend_sequence_callback));
  backend_task_runner_->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                   std::move(reply));
}

void DataTypeStoreImpl::ReadAllDataAndPreprocessDone(
    CallbackWithResult callback,
    const std::optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error);
}

void DataTypeStoreImpl::DeleteAllDataAndMetadata(CallbackWithResult callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  auto task = base::BindOnce(&BlockingDataTypeStore::DeleteAllDataAndMetadata,
                             base::Unretained(backend_store_.get()));
  auto reply =
      base::BindOnce(&DataTypeStoreImpl::WriteModificationsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  backend_task_runner_->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                   std::move(reply));
}

std::unique_ptr<DataTypeStore::WriteBatch>
DataTypeStoreImpl::CreateWriteBatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return BlockingDataTypeStoreImpl::CreateWriteBatch(data_type_, storage_type_);
}

void DataTypeStoreImpl::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    CallbackWithResult callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  auto task = base::BindOnce(&BlockingDataTypeStore::CommitWriteBatch,
                             base::Unretained(backend_store_.get()),
                             std::move(write_batch));
  auto reply =
      base::BindOnce(&DataTypeStoreImpl::WriteModificationsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  backend_task_runner_->PostTaskAndReplyWithResult(FROM_HERE, std::move(task),
                                                   std::move(reply));
}

void DataTypeStoreImpl::WriteModificationsDone(
    CallbackWithResult callback,
    const std::optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error);
}

}  // namespace syncer

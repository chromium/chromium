// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_impl.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/task_runner_util.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model_impl/blocking_model_type_store_impl.h"

namespace syncer {

namespace {

base::Optional<ModelError> ReadAllDataAndPreprocessOnBackendSequence(
    BlockingModelTypeStoreImpl* blocking_store,
    ModelTypeStore::PreprocessCallback
        preprocess_on_backend_sequence_callback) {
  DCHECK(blocking_store);

  auto record_list = std::make_unique<ModelTypeStoreBase::RecordList>();
  base::Optional<ModelError> error =
      blocking_store->ReadAllData(record_list.get());
  if (error) {
    return error;
  }

  return std::move(preprocess_on_backend_sequence_callback)
      .Run(std::move(record_list));
}

}  // namespace

ModelTypeStoreImpl::ModelTypeStoreImpl(
    ModelType type,
    std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
        backend_store,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : type_(type),
      backend_task_runner_(std::move(backend_task_runner)),
      backend_store_(std::move(backend_store)) {
  DCHECK(backend_store_);
  DCHECK(backend_task_runner_);
}

ModelTypeStoreImpl::~ModelTypeStoreImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Note on pattern for communicating with backend:
//  - API function (e.g. ReadData) allocates lists for output.
//  - API function prepares two callbacks: task that will be posted on backend
//    thread and reply which will be posted on model type thread once task
//    finishes.
//  - Task for backend thread takes raw pointers to output lists while reply
//    takes ownership of those lists. This allows backend interface to be simple
//    while ensuring proper objects' lifetime.
//  - Function bound by reply calls consumer's callback and passes ownership of
//    output lists to it.

void ModelTypeStoreImpl::ReadData(const IdList& id_list,
                                  ReadDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  std::unique_ptr<IdList> missing_id_list(new IdList());

  auto task = base::BindOnce(&BlockingModelTypeStore::ReadData,
                             base::Unretained(backend_store_.get()), id_list,
                             base::Unretained(record_list.get()),
                             base::Unretained(missing_id_list.get()));
  auto reply = base::BindOnce(
      &ModelTypeStoreImpl::ReadDataDone, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(record_list), std::move(missing_id_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::ReadDataDone(ReadDataCallback callback,
                                      std::unique_ptr<RecordList> record_list,
                                      std::unique_ptr<IdList> missing_id_list,
                                      const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error, std::move(record_list),
                          std::move(missing_id_list));
}

void ModelTypeStoreImpl::ReadAllData(ReadAllDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  auto task = base::BindOnce(&BlockingModelTypeStore::ReadAllData,
                             base::Unretained(backend_store_.get()),
                             base::Unretained(record_list.get()));
  auto reply = base::BindOnce(&ModelTypeStoreImpl::ReadAllDataDone,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), std::move(record_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::ReadAllDataDone(
    ReadAllDataCallback callback,
    std::unique_ptr<RecordList> record_list,
    const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error, std::move(record_list));
}

void ModelTypeStoreImpl::ReadAllMetadata(ReadMetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());

  auto metadata_batch = std::make_unique<MetadataBatch>();
  auto task = base::BindOnce(&BlockingModelTypeStore::ReadAllMetadata,
                             base::Unretained(backend_store_.get()),
                             base::Unretained(metadata_batch.get()));
  auto reply = base::BindOnce(&ModelTypeStoreImpl::ReadAllMetadataDone,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback), std::move(metadata_batch));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::ReadAllMetadataDone(
    ReadMetadataCallback callback,
    std::unique_ptr<MetadataBatch> metadata_batch,
    const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    std::move(callback).Run(*error, std::make_unique<MetadataBatch>());
    return;
  }

  std::move(callback).Run({}, std::move(metadata_batch));
}

void ModelTypeStoreImpl::ReadAllDataAndPreprocess(
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
      base::BindOnce(&ModelTypeStoreImpl::ReadAllDataAndPreprocessDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(completion_on_frontend_sequence_callback));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::ReadAllDataAndPreprocessDone(
    CallbackWithResult callback,
    const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error);
}

void ModelTypeStoreImpl::DeleteAllDataAndMetadata(CallbackWithResult callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  auto task = base::BindOnce(&BlockingModelTypeStore::DeleteAllDataAndMetadata,
                             base::Unretained(backend_store_.get()));
  auto reply =
      base::BindOnce(&ModelTypeStoreImpl::WriteModificationsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

std::unique_ptr<ModelTypeStore::WriteBatch>
ModelTypeStoreImpl::CreateWriteBatch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return BlockingModelTypeStoreImpl::CreateWriteBatchForType(type_);
}

void ModelTypeStoreImpl::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    CallbackWithResult callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  auto task = base::BindOnce(&BlockingModelTypeStore::CommitWriteBatch,
                             base::Unretained(backend_store_.get()),
                             std::move(write_batch));
  auto reply =
      base::BindOnce(&ModelTypeStoreImpl::WriteModificationsDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

void ModelTypeStoreImpl::WriteModificationsDone(
    CallbackWithResult callback,
    const base::Optional<ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(error);
}

}  // namespace syncer

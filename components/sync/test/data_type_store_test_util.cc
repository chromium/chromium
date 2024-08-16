// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/data_type_store_test_util.h"

#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/blocking_data_type_store_impl.h"
#include "components/sync/model/data_type_store_backend.h"
#include "components/sync/model/data_type_store_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Implementation of DataTypeStore that delegates all calls to another
// instance, as injected in the constructor, useful for APIs that take ownership
// of DataTypeStore.
class ForwardingDataTypeStore : public DataTypeStore {
 public:
  explicit ForwardingDataTypeStore(DataTypeStore* other) : other_(other) {}

  void ReadData(const IdList& id_list, ReadDataCallback callback) override {
    other_->ReadData(id_list, std::move(callback));
  }

  void ReadAllData(ReadAllDataCallback callback) override {
    other_->ReadAllData(std::move(callback));
  }

  void ReadAllMetadata(ReadMetadataCallback callback) override {
    other_->ReadAllMetadata(std::move(callback));
  }
  void ReadAllDataAndMetadata(
      ReadAllDataAndMetadataCallback callback) override {
    other_->ReadAllDataAndMetadata(std::move(callback));
  }

  void ReadAllDataAndPreprocess(
      PreprocessCallback preprocess_on_backend_sequence_callback,
      CallbackWithResult completion_on_frontend_sequence_callback) override {
    other_->ReadAllDataAndPreprocess(
        std::move(preprocess_on_backend_sequence_callback),
        std::move(completion_on_frontend_sequence_callback));
  }

  std::unique_ptr<WriteBatch> CreateWriteBatch() override {
    return other_->CreateWriteBatch();
  }

  void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                        CallbackWithResult callback) override {
    other_->CommitWriteBatch(std::move(write_batch), std::move(callback));
  }

  void DeleteAllDataAndMetadata(CallbackWithResult callback) override {
    other_->DeleteAllDataAndMetadata(std::move(callback));
  }

 private:
  const raw_ptr<DataTypeStore, DanglingUntriaged> other_;
};

}  // namespace

// static
std::unique_ptr<DataTypeStore>
DataTypeStoreTestUtil::CreateInMemoryStoreForTest(DataType type,
                                                  StorageType storage_type) {
  std::unique_ptr<BlockingDataTypeStoreImpl, base::OnTaskRunnerDeleter>
      blocking_store(new BlockingDataTypeStoreImpl(
                         type, storage_type,
                         DataTypeStoreBackend::CreateInMemoryForTest()),
                     base::OnTaskRunnerDeleter(
                         base::SequencedTaskRunner::GetCurrentDefault()));
  // Not all tests issue a RunUntilIdle() at the very end, to guarantee that
  // the backend is properly destroyed. They also don't need to verify that, so
  // let keep memory sanitizers happy.
  ANNOTATE_LEAKING_OBJECT_PTR(blocking_store.get());
  return std::make_unique<DataTypeStoreImpl>(
      type, storage_type, std::move(blocking_store),
      base::SequencedTaskRunner::GetCurrentDefault());
}

// static
RepeatingDataTypeStoreFactory
DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest() {
  return base::BindRepeating(
      [](DataType type, DataTypeStore::InitCallback callback) {
        std::move(callback).Run(/*error=*/std::nullopt,
                                CreateInMemoryStoreForTest(type));
      });
}

// static
OnceDataTypeStoreFactory DataTypeStoreTestUtil::MoveStoreToFactory(
    std::unique_ptr<DataTypeStore> store) {
  return base::BindOnce(
      [](std::unique_ptr<DataTypeStore> store, DataType type,
         DataTypeStore::InitCallback callback) {
        std::move(callback).Run(/*error=*/std::nullopt, std::move(store));
      },
      std::move(store));
}

// static
RepeatingDataTypeStoreFactory DataTypeStoreTestUtil::FactoryForForwardingStore(
    DataTypeStore* target) {
  return base::BindRepeating(
      [](DataTypeStore* target, DataType,
         DataTypeStore::InitCallback callback) {
        std::move(callback).Run(
            /*error=*/std::nullopt,
            std::make_unique<ForwardingDataTypeStore>(target));
      },
      base::Unretained(target));
}

}  // namespace syncer

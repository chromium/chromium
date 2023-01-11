// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/model_type_store_test_util.h"

#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/blocking_model_type_store_impl.h"
#include "components/sync/model/model_type_store_backend.h"
#include "components/sync/model/model_type_store_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Implementation of ModelTypeStore that delegates all calls to another
// instance, as injected in the constructor, useful for APIs that take ownership
// of ModelTypeStore.
class ForwardingModelTypeStore : public ModelTypeStore {
 public:
  explicit ForwardingModelTypeStore(ModelTypeStore* other) : other_(other) {}

  void ReadData(const IdList& id_list, ReadDataCallback callback) override {
    other_->ReadData(id_list, std::move(callback));
  }

  void ReadAllData(ReadAllDataCallback callback) override {
    other_->ReadAllData(std::move(callback));
  }

  void ReadAllMetadata(ReadMetadataCallback callback) override {
    other_->ReadAllMetadata(std::move(callback));
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
  raw_ptr<ModelTypeStore> other_;
};

}  // namespace

// static
std::unique_ptr<ModelTypeStore>
ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(ModelType type) {
  std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
      blocking_store(new BlockingModelTypeStoreImpl(
                         type, StorageType::kUnspecified,
                         ModelTypeStoreBackend::CreateInMemoryForTest()),
                     base::OnTaskRunnerDeleter(
                         base::SequencedTaskRunner::GetCurrentDefault()));
  // Not all tests issue a RunUntilIdle() at the very end, to guarantee that
  // the backend is properly destroyed. They also don't need to verify that, so
  // let keep memory sanitizers happy.
  ANNOTATE_LEAKING_OBJECT_PTR(blocking_store.get());
  return std::make_unique<ModelTypeStoreImpl>(
      type, StorageType::kUnspecified, std::move(blocking_store),
      base::SequencedTaskRunner::GetCurrentDefault());
}

// static
RepeatingModelTypeStoreFactory
ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest() {
  return base::BindRepeating(
      [](ModelType type, ModelTypeStore::InitCallback callback) {
        std::move(callback).Run(/*error=*/absl::nullopt,
                                CreateInMemoryStoreForTest(type));
      });
}

// static
OnceModelTypeStoreFactory ModelTypeStoreTestUtil::MoveStoreToFactory(
    std::unique_ptr<ModelTypeStore> store) {
  return base::BindOnce(
      [](std::unique_ptr<ModelTypeStore> store, ModelType type,
         ModelTypeStore::InitCallback callback) {
        std::move(callback).Run(/*error=*/absl::nullopt, std::move(store));
      },
      std::move(store));
}

// static
RepeatingModelTypeStoreFactory
ModelTypeStoreTestUtil::FactoryForForwardingStore(ModelTypeStore* target) {
  return base::BindRepeating(
      [](ModelTypeStore* target, ModelType,
         ModelTypeStore::InitCallback callback) {
        std::move(callback).Run(
            /*error=*/absl::nullopt,
            std::make_unique<ForwardingModelTypeStore>(target));
      },
      base::Unretained(target));
}

}  // namespace syncer

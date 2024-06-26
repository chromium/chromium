// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_with_in_memory_cache.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"

namespace syncer {

// static
void ModelTypeStoreWithInMemoryCache::CreateAndLoad(
    OnceModelTypeStoreFactory store_factory,
    ModelType type,
    CreateCallback callback) {
  // Initialization happens in two phases:
  // 1. Create the underlying ModelTypeStore.
  // 2. Load the data and metadata, create the ModelTypeStoreWithInMemoryCache,
  //    and pass it and the metadata on to the caller.

  // Stage 2: Run when data+metadata has been loaded from the underlying store.
  auto on_loaded = base::BindOnce(
      [](CreateCallback callback,
         std::unique_ptr<syncer::ModelTypeStore> underlying_store,
         const std::optional<ModelError>& error,
         std::unique_ptr<RecordList> data_records,
         std::unique_ptr<MetadataBatch> metadata_batch) {
        if (error) {
          std::move(callback).Run(error, nullptr, nullptr);
        } else {
          // WrapUnique because the constructor is private.
          auto store = base::WrapUnique(new ModelTypeStoreWithInMemoryCache(
              std::move(underlying_store), std::move(data_records)));
          std::move(callback).Run(std::nullopt, std::move(store),
                                  std::move(metadata_batch));
        }
      });

  // Stage 1: Creates the ModelTypeStore, then (if successful) kicks off loading
  // of data+metadata.
  auto on_store_created = base::BindOnce(
      [](decltype(on_loaded) on_loaded_callback, CreateCallback create_callback,
         const std::optional<syncer::ModelError>& error,
         std::unique_ptr<syncer::ModelTypeStore> underlying_store) {
        if (error) {
          std::move(create_callback).Run(error, nullptr, nullptr);
        } else {
          syncer::ModelTypeStore* underlying_store_raw = underlying_store.get();
          underlying_store_raw->ReadAllDataAndMetadata(base::BindOnce(
              std::move(on_loaded_callback), std::move(create_callback),
              std::move(underlying_store)));
        }
      },
      std::move(on_loaded), std::move(callback));

  std::move(store_factory).Run(type, std::move(on_store_created));
}

ModelTypeStoreWithInMemoryCache::ModelTypeStoreWithInMemoryCache(
    std::unique_ptr<ModelTypeStore> underlying_store,
    std::unique_ptr<RecordList> data_records)
    : underlying_store_(std::move(underlying_store)) {
  for (Record& record : *data_records) {
    in_memory_data_[std::move(record.id)] = std::move(record.value);
  }
}

ModelTypeStoreWithInMemoryCache::~ModelTypeStoreWithInMemoryCache() = default;

std::unique_ptr<ModelTypeStoreBase::WriteBatch>
ModelTypeStoreWithInMemoryCache::CreateWriteBatch() {
  return std::make_unique<WriteBatchWrapper>(
      underlying_store_->CreateWriteBatch());
}

void ModelTypeStoreWithInMemoryCache::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    CallbackWithResult callback) {
  std::unique_ptr<WriteBatchWrapper> write_batch_wrapper =
      base::WrapUnique(static_cast<WriteBatchWrapper*>(write_batch.release()));

  std::map<std::string, std::optional<std::string>> changes =
      write_batch_wrapper->ExtractChanges();
  for (auto& [id, update] : changes) {
    if (update.has_value()) {
      in_memory_data_[id] = *update;
    } else {
      in_memory_data_.erase(id);
    }
  }

  underlying_store_->CommitWriteBatch(
      WriteBatchWrapper::ExtractUnderlying(std::move(write_batch_wrapper)),
      std::move(callback));
}

void ModelTypeStoreWithInMemoryCache::DeleteAllDataAndMetadata(
    CallbackWithResult callback) {
  in_memory_data_.clear();
  underlying_store_->DeleteAllDataAndMetadata(std::move(callback));
}

ModelTypeStoreWithInMemoryCache::WriteBatchWrapper::WriteBatchWrapper(
    std::unique_ptr<WriteBatch> underlying_batch)
    : underlying_batch_(std::move(underlying_batch)) {}

ModelTypeStoreWithInMemoryCache::WriteBatchWrapper::~WriteBatchWrapper() =
    default;

// static
std::unique_ptr<ModelTypeStoreBase::WriteBatch>
ModelTypeStoreWithInMemoryCache::WriteBatchWrapper::ExtractUnderlying(
    std::unique_ptr<WriteBatchWrapper> wrapper) {
  return std::move(wrapper->underlying_batch_);
}

void ModelTypeStoreWithInMemoryCache::WriteBatchWrapper::WriteData(
    const std::string& id,
    const std::string& value) {
  underlying_batch_->WriteData(id, value);
  changes_[id] = value;
}

void ModelTypeStoreWithInMemoryCache::WriteBatchWrapper::DeleteData(
    const std::string& id) {
  underlying_batch_->DeleteData(id);
  changes_[id] = std::nullopt;
}

MetadataChangeList*
ModelTypeStoreWithInMemoryCache::WriteBatchWrapper::GetMetadataChangeList() {
  return underlying_batch_->GetMetadataChangeList();
}

std::map<std::string, std::optional<std::string>>
ModelTypeStoreWithInMemoryCache::WriteBatchWrapper::ExtractChanges() {
  return std::move(changes_);
}

}  // namespace syncer

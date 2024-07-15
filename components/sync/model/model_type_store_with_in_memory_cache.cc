// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/model_type_store_with_in_memory_cache.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/security_event_specifics.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"

namespace syncer {

// static
template <typename Entry>
void ModelTypeStoreWithInMemoryCache<Entry>::CreateAndLoad(
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
         std::unique_ptr<ModelTypeStoreBase::RecordList> data_records,
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

template <typename Entry>
ModelTypeStoreWithInMemoryCache<Entry>::ModelTypeStoreWithInMemoryCache(
    std::unique_ptr<ModelTypeStore> underlying_store,
    std::unique_ptr<ModelTypeStoreBase::RecordList> data_records)
    : underlying_store_(std::move(underlying_store)) {
  for (ModelTypeStoreBase::Record& record : *data_records) {
    Entry entry;
    if (entry.ParseFromString(record.value)) {
      in_memory_data_[std::move(record.id)] = std::move(entry);
    }
  }
}

template <typename Entry>
ModelTypeStoreWithInMemoryCache<Entry>::~ModelTypeStoreWithInMemoryCache() =
    default;

template <typename Entry>
std::unique_ptr<typename ModelTypeStoreWithInMemoryCache<Entry>::WriteBatch>
ModelTypeStoreWithInMemoryCache<Entry>::CreateWriteBatch() {
  return std::make_unique<WriteBatchImpl>(
      underlying_store_->CreateWriteBatch());
}

template <typename Entry>
void ModelTypeStoreWithInMemoryCache<Entry>::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    CallbackWithResult callback) {
  std::unique_ptr<WriteBatchImpl> write_batch_impl =
      base::WrapUnique(static_cast<WriteBatchImpl*>(write_batch.release()));

  std::map<std::string, std::optional<Entry>> changes =
      write_batch_impl->ExtractChanges();
  for (auto& [id, update] : changes) {
    if (update.has_value()) {
      in_memory_data_[id] = *update;
    } else {
      in_memory_data_.erase(id);
    }
  }

  underlying_store_->CommitWriteBatch(
      WriteBatchImpl::ExtractUnderlying(std::move(write_batch_impl)),
      std::move(callback));
}

template <typename Entry>
void ModelTypeStoreWithInMemoryCache<Entry>::DeleteAllDataAndMetadata(
    CallbackWithResult callback) {
  in_memory_data_.clear();
  underlying_store_->DeleteAllDataAndMetadata(std::move(callback));
}

// static
template <typename Entry>
std::unique_ptr<ModelTypeStore>
ModelTypeStoreWithInMemoryCache<Entry>::ExtractUnderlyingStoreForTest(
    std::unique_ptr<ModelTypeStoreWithInMemoryCache> store) {
  return std::move(store->underlying_store_);
}

template <typename Entry>
ModelTypeStoreWithInMemoryCache<Entry>::WriteBatchImpl::WriteBatchImpl(
    std::unique_ptr<ModelTypeStoreBase::WriteBatch> underlying_batch)
    : underlying_batch_(std::move(underlying_batch)) {}

template <typename Entry>
ModelTypeStoreWithInMemoryCache<Entry>::WriteBatchImpl::~WriteBatchImpl() =
    default;

// static
template <typename Entry>
std::unique_ptr<ModelTypeStoreBase::WriteBatch>
ModelTypeStoreWithInMemoryCache<Entry>::WriteBatchImpl::ExtractUnderlying(
    std::unique_ptr<WriteBatchImpl> wrapper) {
  return std::move(wrapper->underlying_batch_);
}

template <typename Entry>
void ModelTypeStoreWithInMemoryCache<Entry>::WriteBatchImpl::WriteData(
    const std::string& id,
    Entry value) {
  underlying_batch_->WriteData(id, value.SerializeAsString());
  changes_[id] = std::move(value);
}

template <typename Entry>
void ModelTypeStoreWithInMemoryCache<Entry>::WriteBatchImpl::DeleteData(
    const std::string& id) {
  underlying_batch_->DeleteData(id);
  changes_[id] = std::nullopt;
}

template <typename Entry>
MetadataChangeList* ModelTypeStoreWithInMemoryCache<
    Entry>::WriteBatchImpl::GetMetadataChangeList() {
  return underlying_batch_->GetMetadataChangeList();
}

template <typename Entry>
void ModelTypeStoreWithInMemoryCache<Entry>::WriteBatchImpl::
    TakeMetadataChangesFrom(std::unique_ptr<MetadataChangeList> mcl) {
  static_cast<InMemoryMetadataChangeList*>(mcl.get())->TransferChangesTo(
      GetMetadataChangeList());
}

template <typename Entry>
std::map<std::string, std::optional<Entry>>
ModelTypeStoreWithInMemoryCache<Entry>::WriteBatchImpl::ExtractChanges() {
  return std::move(changes_);
}

// Explicit instantiations for all required entry types.
template class ModelTypeStoreWithInMemoryCache<sync_pb::CookieSpecifics>;
template class ModelTypeStoreWithInMemoryCache<sync_pb::SecurityEventSpecifics>;
template class ModelTypeStoreWithInMemoryCache<sync_pb::UserConsentSpecifics>;
template class ModelTypeStoreWithInMemoryCache<sync_pb::UserEventSpecifics>;

}  // namespace syncer

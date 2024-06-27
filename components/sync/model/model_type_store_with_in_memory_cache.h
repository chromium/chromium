// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_WITH_IN_MEMORY_CACHE_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_WITH_IN_MEMORY_CACHE_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_base.h"

namespace syncer {

class MetadataBatch;
class MetadataChangeList;

// A convenience class that wraps around a ModelTypeStore, but also maintains a
// cache of all the data (not metadata) in memory. It only implements a subset
// of the full ModelTypeStore API, specifically the parts that are usually
// required by commit-only data types (i.e. the ones that need the in-memory
// cache).
// `Entry` is meant to be a proto, since the class performs proto serialization
// and deserialization.
// NOTE: This template class has explicit instantiations for all required entry
// types at the end of the .cc file. If you want to use it with a new entry
// type, add a corresponding specialization there!
template <typename Entry>
class ModelTypeStoreWithInMemoryCache {
 public:
  using CreateCallback = base::OnceCallback<void(
      const std::optional<ModelError>& error,
      std::unique_ptr<ModelTypeStoreWithInMemoryCache> store,
      std::unique_ptr<MetadataBatch> metadata_batch)>;
  using CallbackWithResult =
      base::OnceCallback<void(const std::optional<ModelError>& error)>;

  class WriteBatch {
   public:
    virtual ~WriteBatch() = default;

    virtual void WriteData(const std::string& id, Entry value) = 0;
    virtual void DeleteData(const std::string& id) = 0;
    virtual MetadataChangeList* GetMetadataChangeList() = 0;
    virtual void TakeMetadataChangesFrom(
        std::unique_ptr<MetadataChangeList> mcl) = 0;
  };

  // Factory function: Creates the store, loads the data and metadata, populates
  // the in-memory cache, and returns the ready-to-use store to the callback.
  // In case of errors, both store and metadata_batch will be nullptr.
  static void CreateAndLoad(OnceModelTypeStoreFactory store_factory,
                            ModelType type,
                            CreateCallback callback);

  ~ModelTypeStoreWithInMemoryCache();

  // Parts of the ModelTypeStore API; see comments there.
  std::unique_ptr<WriteBatch> CreateWriteBatch();
  void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                        CallbackWithResult callback);
  void DeleteAllDataAndMetadata(CallbackWithResult callback);

  // Synchronous access to the in-memory data cache.
  const std::map<std::string, Entry>& in_memory_data() const {
    return in_memory_data_;
  }

  static std::unique_ptr<ModelTypeStore> ExtractUnderlyingStoreForTest(
      std::unique_ptr<ModelTypeStoreWithInMemoryCache> store);

 private:
  class WriteBatchImpl : public WriteBatch {
   public:
    explicit WriteBatchImpl(
        std::unique_ptr<ModelTypeStoreBase::WriteBatch> underlying_batch);
    ~WriteBatchImpl() override;

    static std::unique_ptr<ModelTypeStoreBase::WriteBatch> ExtractUnderlying(
        std::unique_ptr<WriteBatchImpl> wrapper);

    void WriteData(const std::string& id, Entry value) override;
    void DeleteData(const std::string& id) override;
    MetadataChangeList* GetMetadataChangeList() override;
    void TakeMetadataChangesFrom(
        std::unique_ptr<MetadataChangeList> mcl) override;

    std::map<std::string, std::optional<Entry>> ExtractChanges();

   private:
    std::unique_ptr<ModelTypeStoreBase::WriteBatch> underlying_batch_;

    // A nullopt value represents a deletion.
    std::map<std::string, std::optional<Entry>> changes_;
  };

  ModelTypeStoreWithInMemoryCache(
      std::unique_ptr<ModelTypeStore> underlying_store,
      std::unique_ptr<ModelTypeStoreBase::RecordList> data_records);

  std::unique_ptr<ModelTypeStore> underlying_store_;
  std::map<std::string, Entry> in_memory_data_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_WITH_IN_MEMORY_CACHE_H_

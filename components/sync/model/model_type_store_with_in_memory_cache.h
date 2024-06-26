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

// A convenience class that wraps around a ModelTypeStore, but also maintains a
// cache of all the data (not metadata) in memory. It only implements a subset
// of the full ModelTypeStore API, specifically the parts that are usually
// required by commit-only data types (i.e. the ones that need the in-memory
// cache).
class ModelTypeStoreWithInMemoryCache : public ModelTypeStoreBase {
 public:
  using CreateCallback = base::OnceCallback<void(
      const std::optional<ModelError>& error,
      std::unique_ptr<ModelTypeStoreWithInMemoryCache> store,
      std::unique_ptr<MetadataBatch> metadata_batch)>;
  using CallbackWithResult =
      base::OnceCallback<void(const std::optional<ModelError>& error)>;

  // Factory function: Creates the store, loads the data and metadata, populates
  // the in-memory cache, and returns the ready-to-use store to the callback.
  // In case of errors, both store and metadata_batch will be nullptr.
  static void CreateAndLoad(OnceModelTypeStoreFactory store_factory,
                            ModelType type,
                            CreateCallback callback);

  ~ModelTypeStoreWithInMemoryCache() override;

  // Parts of the ModelTypeStore API; see comments there.
  std::unique_ptr<WriteBatch> CreateWriteBatch();
  void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                        CallbackWithResult callback);
  void DeleteAllDataAndMetadata(CallbackWithResult callback);

  // Synchronous access to the in-memory data cache.
  const std::map<std::string, std::string>& in_memory_data() const {
    return in_memory_data_;
  }

 private:
  ModelTypeStoreWithInMemoryCache(
      std::unique_ptr<ModelTypeStore> underlying_store,
      std::unique_ptr<RecordList> data_records);

  class WriteBatchWrapper : public WriteBatch {
   public:
    explicit WriteBatchWrapper(std::unique_ptr<WriteBatch> underlying_batch);
    ~WriteBatchWrapper() override;

    static std::unique_ptr<WriteBatch> ExtractUnderlying(
        std::unique_ptr<WriteBatchWrapper> wrapper);

    void WriteData(const std::string& id, const std::string& value) override;
    void DeleteData(const std::string& id) override;
    MetadataChangeList* GetMetadataChangeList() override;

    std::map<std::string, std::optional<std::string>> ExtractChanges();

   private:
    std::unique_ptr<WriteBatch> underlying_batch_;

    // A nullopt value represents a deletion.
    std::map<std::string, std::optional<std::string>> changes_;
  };

  const std::unique_ptr<ModelTypeStore> underlying_store_;
  std::map<std::string, std::string> in_memory_data_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_WITH_IN_MEMORY_CACHE_H_

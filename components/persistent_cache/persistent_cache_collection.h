// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_COLLECTION_H_
#define COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_COLLECTION_H_

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/persistent_cache/backend_params_manager.h"
#include "components/persistent_cache/persistent_cache.h"

namespace persistent_cache {

// Use PersistentCacheCollection to seamlessly access multiple PersistentCache
// instances. For example when used instead of double-keying with backends that
// use disk storage this can result in smaller separated files. Unlike
// PersistentCache itself PersistentCacheCollection is not thread-safe in any
// way.
//
// Example:
//    PersistentCacheCollection collection(
//      std::make_unique<BackendParamsManager>(temp_dir.GetPath()));
//    collection.Insert("first_cache_id", "key", value_span);
//    collection.Insert("second_cache_id","key", value_span);
//    auto entry = collection.Find("first_cache_id", "key");
//
// Use PersistentCacheCollection to store and retrieve key-value pairs from
// multiple `PersistentCache`s which are created just-in-time.
class COMPONENT_EXPORT(PERSISTENT_CACHE) PersistentCacheCollection {
 public:
  PersistentCacheCollection(
      std::unique_ptr<BackendParamsManager> params_manager);
  ~PersistentCacheCollection();

  // Not copyable or moveable.
  PersistentCacheCollection(const PersistentCacheCollection&) = delete;
  PersistentCacheCollection(PersistentCacheCollection&&) = delete;
  PersistentCacheCollection& operator=(const PersistentCacheCollection&) =
      delete;
  PersistentCacheCollection& operator=(PersistentCacheCollection&&) = delete;

  // Pass-through to PersistentCache functions that first select the correct
  // cache. See PersistentCache for details. Synchronous.
  std::unique_ptr<Entry> Find(const std::string& cache_id,
                              std::string_view key);
  void Insert(const std::string& cache_id,
              std::string_view key,
              base::span<const uint8_t> content,
              EntryMetadata metadata = EntryMetadata{});

  // Clears out the LRU map for testing.
  void ClearForTesting();

  // Deletes all files handled by the backend params manager.
  void DeleteAllFiles();

 private:
  PersistentCache* GetOrCreateCache(const std::string& cache_id);

  std::unique_ptr<BackendParamsManager> backend_params_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::HashingLRUCache<std::string, std::unique_ptr<PersistentCache>>
      persistent_caches_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_COLLECTION_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_COLLECTION_H_
#define COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_COLLECTION_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/persistent_cache/backend.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/buffer_provider.h"
#include "components/persistent_cache/entry_metadata.h"

namespace persistent_cache {

struct PendingBackend;
class PersistentCache;

// Use PersistentCacheCollection to seamlessly access multiple PersistentCache
// instances. For example when used instead of double-keying with backends that
// use disk storage this can result in smaller separated files. Unlike
// PersistentCache itself PersistentCacheCollection is not thread-safe in any
// way.
//
// Example:
//    PersistentCacheCollection collection(temp_dir.GetPath(), 4096);
//    collection.Insert("first_cache_id", "key", value_span);
//    collection.Insert("second_cache_id","key", value_span);
//    ASSIGN_OR_RETURN(auto entry, collection->Find("first_cache_id", "key"),
//      [](persistent_cache::TransactionError error) {
//        // Translate error to return type here.
//      });
//
// Use PersistentCacheCollection to store and retrieve key-value pairs from
// multiple `PersistentCache`s which are created just-in-time.
//
// PersistentCaches stored in the collection can be shared through exported
// parameters but cannot keep being used after they are evicted from the
// collection. PersistentCacheCollection ensures this doesn't happen by
// automatically abandoning caches when evicted.
class COMPONENT_EXPORT(PERSISTENT_CACHE) PersistentCacheCollection {
 public:
  static constexpr size_t kDefaultLruCacheCapacity = 100;

  // Constructs an instance that will use the default storage backend for file
  // management within `top_directory`.
  PersistentCacheCollection(base::FilePath top_directory,
                            int64_t target_footprint,
                            size_t lru_capacity = kDefaultLruCacheCapacity);

  // Constructs an instance that will use `storage_delegate` for file management
  // within `top_directory`.
  PersistentCacheCollection(
      base::FilePath top_directory,
      int64_t target_footprint,
      std::unique_ptr<BackendStorage::Delegate> storage_delegate,
      size_t lru_capacity = kDefaultLruCacheCapacity);

  PersistentCacheCollection(const PersistentCacheCollection&) = delete;
  PersistentCacheCollection& operator=(const PersistentCacheCollection&) =
      delete;
  ~PersistentCacheCollection();

  // Pass-through to PersistentCache functions that first select the correct
  // cache. `cache_id` must be a US-ASCII string consisting more-or-less of
  // lower-case letters, numbers, and select punctuation; see
  // `BaseNameFromCacheId()` below for gory details.
  base::expected<std::optional<EntryMetadata>, TransactionError> Find(
      const std::string& cache_id,
      std::string_view key,
      BufferProvider buffer_provider);

  base::expected<void, TransactionError> Insert(
      const std::string& cache_id,
      std::string_view key,
      base::span<const uint8_t> content,
      EntryMetadata metadata = EntryMetadata{});

  // Deletes all files used by the collection, including any present on-disk
  // that are not actively in-use.
  void DeleteAllFiles();

  // Returns a pending backend for an independent read-only connection to the
  // persistent cache at `cache_id`, or nothing if the cache's backend is not
  // operating or the params cannot be exported.
  std::optional<PendingBackend> ShareReadOnlyConnection(
      const std::string& cache_id);

  // Returns a pending backend for an independent read-write connection to the
  // cache at `cache_id`, or nothing if the cache's backend is not operating or
  // the params cannot be exported.
  std::optional<PendingBackend> ShareReadWriteConnection(
      const std::string& cache_id);

 private:
  using PersistentCacheLRUMap =
      base::HashingLRUCache<std::string, std::unique_ptr<PersistentCache>>;

  friend class PersistentCacheCollectionTest;
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest, BaseNameFromCacheId);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest,
                           FullAllowedCharacterSetHandled);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest, RetrievalAfterClear);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest,
                           InstancesAbandonnedOnClear);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest,
                           EvictWhileLockedDeletesFiles);

  // Abandon `cache` associated with `cache_id` in the LRU cache.
  void AbandonCache(const std::string& cache_id,
                    PersistentCache* persistent_cache)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // To be called on receiving a transaction error from the cache at `cache_id`.
  TransactionError HandleTransactionError(const std::string& cache_id,
                                          TransactionError error);

  // Deletes files in the instance's directory from oldest to newest until the
  // instance is using no more than 90% of its target footprint.
  void ReduceFootPrint();

  // Returns the PersistentCache for `cache_id`, creating it if needed. Returns
  // nullptr if creation fails.
  PersistentCache* GetOrCreateCache(const std::string& cache_id);

  // Clears out the LRU map for testing.
  void Clear();

  // Returns the basename of the file(s) used by a backend given a cache id. An
  // extension MUST be added to a returned basename before use. Returns an empty
  // path if `cache_id` contains any character that does not match the following
  // regular expression (where '\' escapes the character it precedes):
  // "[\n !\"#$&'()*+,\-./0-9:;<=>?@[\\\]_a-z|~]". In other words, `cache_id`
  // must be a subset of US-ASCII consisting of newline, space, numbers,
  // lower-case letters, and select punctuation.
  static base::FilePath BaseNameFromCacheId(const std::string& cache_id);

  // Returns a string holding all valid characters for a cache id.
  static std::string GetAllAllowedCharactersInCacheIds();

  // Must outlive `persistent_caches_`.
  BackendStorage backend_storage_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Desired maximum disk footprint for the cache collection in bytes.
  const int64_t target_footprint_;
  const size_t lru_capacity_;

  PersistentCacheLRUMap persistent_caches_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Running tally of how many bytes can be inserted before a footprint
  // reduction is triggered.
  int64_t bytes_until_footprint_reduction_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_COLLECTION_H_

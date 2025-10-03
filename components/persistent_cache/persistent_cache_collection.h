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
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/entry_metadata.h"

namespace persistent_cache {

struct BackendParams;
class Entry;
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
//    auto entry = collection.Find("first_cache_id", "key");
//
// Use PersistentCacheCollection to store and retrieve key-value pairs from
// multiple `PersistentCache`s which are created just-in-time.
class COMPONENT_EXPORT(PERSISTENT_CACHE) PersistentCacheCollection {
 public:
  PersistentCacheCollection(base::FilePath top_directory,
                            int64_t target_footprint);
  PersistentCacheCollection(const PersistentCacheCollection&) = delete;
  PersistentCacheCollection& operator=(const PersistentCacheCollection&) =
      delete;
  ~PersistentCacheCollection();

  // Pass-through to PersistentCache functions that first select the correct
  // cache. `cache_id` must be a US-ASCII string consisting more-or-less of
  // lower-case letters, numbers, and select punctuation; see
  // `BaseNameFromCacheId()` below for gory details.
  std::unique_ptr<Entry> Find(const std::string& cache_id,
                              std::string_view key);
  void Insert(const std::string& cache_id,
              std::string_view key,
              base::span<const uint8_t> content,
              EntryMetadata metadata = EntryMetadata{});

  // Deletes all files used by the collection, including any present on-disk
  // that are not actively in-use.
  void DeleteAllFiles();

  // Returns params for an independent read-only connection to the persistent
  // cache at `cache_id`, or nothing if the cache's backend is not operating or
  // the params cannot be exported.
  std::optional<BackendParams> ExportReadOnlyBackendParams(
      const std::string& cache_id);

  // Returns params for an independent read-write connection to the persistent
  // cache at `cache_id`, or nothing if the cache's backend is not operating or
  // the params cannot be exported.
  std::optional<BackendParams> ExportReadWriteBackendParams(
      const std::string& cache_id);

 private:
  friend class PersistentCacheCollectionTest;
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest, BaseNameFromCacheId);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest,
                           FullAllowedCharacterSetHandled);
  FRIEND_TEST_ALL_PREFIXES(PersistentCacheCollectionTest, RetrievalAfterClear);

  // Deletes files in the instance's directory from oldest to newest until the
  // instance is using no more than 90% of its target footprint.
  void ReduceFootPrint();

  // Returns the PersistentCache for `cache_id`, creating it if needed. Returns
  // nullptr if creation fails.
  PersistentCache* GetOrCreateCache(const std::string& cache_id);

  // Clears out the LRU map for testing.
  void ClearForTesting();

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

  BackendStorage backend_storage_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Desired maximum disk footprint for the cache collection in bytes.
  const int64_t target_footprint_;

  base::HashingLRUCache<std::string, std::unique_ptr<PersistentCache>>
      persistent_caches_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Running tally of how many bytes can be inserted before a footprint
  // reduction is triggered.
  int64_t bytes_until_footprint_reduction_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_COLLECTION_H_

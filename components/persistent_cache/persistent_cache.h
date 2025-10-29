// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_
#define COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/entry_metadata.h"

namespace persistent_cache {

class Entry;
class Backend;
enum class TransactionError;

// Use PersistentCache to store and retrieve key-value pairs across processes or
// threads.
//
// Example:
//    // Create a persistent cache backend.
//    BackendParams backend_params = AcquireParams();
//    auto PersistentCache persistent_cache =
//      PersistentCache::Open(backend_params);
//    if(!persistent_cache){
//      // Handle error.
//    }
//
//    // Add a key-value pair.
//    persistent_cache->Insert("foo", base::byte_span_from_cstring("1"));
//
//    // Retrieve a value. The presence of the key and its value are guaranteed
//    // during the lifetime of `entry`.
//    {
//      ASSIGN_OR_RETURN(auto entry, persistent_cache->Find("foo"),
//        [](persistent_cache::TransactionError error) {
//          // Translate and handle error here.
//        });
//
//      // Warning: The value may have changed since insertion (because the
//      // cache is multi-thread/multi-process), been evicted by the backend, or
//      // the initial insertion may have failed.
//      if (entry) {
//        UseEntry(entry);
//      }
//    }
//
//    // Inserting again overwrites anything in there if present.
//    persistent_cache->Insert("foo", base::byte_span_from_cstring("2"));
//
//
// Error Handling and Recovery:
//   Operations can return a `TransactionError`, which dictates the required
// recovery action.
//
// The error types are:
//  - `TransactionError::kTransient`: A recoverable backend error occurred. The
//    current instance is likely still usable. The caller should take the
//    failure as a cache miss or retry the same operation.
//  - `TransactionError::kConnectionError`: The connection to the backend was
//    lost (e.g., a lock could not be acquired). The caller should destroy the
//    instance and re-open with fresh parameters.
//  - `TransactionError::kPermanent`: A fatal, unrecoverable error occurred,
//    indicating data corruption. The caller should delete the backend storage
//    then destroy the instance. No new instance should be backed by the same
//    files before they are properly deleted and recreated.
//
// Resource Management:
//   A PersistentCache instance holds resources like open file handles for its
// entire lifetime. It does not automatically release these on error. Destroying
// the `PersistentCache` instance is required to release those resources. This
// release then enables the caller to perform actions like deleting the cache
// files if necessary/possible.

// Use PersistentCache to store and retrieve key-value pairs across processes or
// threads.
class COMPONENT_EXPORT(PERSISTENT_CACHE) PersistentCache {
 public:
  explicit PersistentCache(std::unique_ptr<Backend> backend);
  ~PersistentCache();

  // Not copyable or moveable.
  PersistentCache(const PersistentCache&) = delete;
  PersistentCache(PersistentCache&&) = delete;
  PersistentCache& operator=(const PersistentCache&) = delete;
  PersistentCache& operator=(PersistentCache&&) = delete;

  // Used to open a cache with a backend of type `impl`. Returns nullptr in
  // case of failure.
  static std::unique_ptr<PersistentCache> Open(BackendParams backend_params);

  // Used to get a handle to entry associated with `key`. Entry is `nullptr` if
  // `key` is not found. Returned entry will remain valid and its contents will
  // be accessible for its entire lifetime. Note: Persistent caches have to
  // outlive entries they vend. See class comments regarding error management.
  //
  // Thread-safe.
  base::expected<std::unique_ptr<Entry>, TransactionError> Find(
      std::string_view key);

  // Used to add an entry containing `content` and associated with `key`.
  // Metadata associated with the entry can be provided in `metadata` or the
  // object can be default initialized to signify no metadata.
  // Implementations are allowed to free other unused entries on demand to make
  // room or fail when full. Returns a empty value on success and error value
  // otherwise. See class comments regarding error management.
  //
  // Thread-safe.
  base::expected<void, TransactionError> Insert(
      std::string_view key,
      base::span<const uint8_t> content,
      EntryMetadata metadata = EntryMetadata{});

  // Returns params for an independent read-only connection to the instance, or
  // nothing if its backend is not operating or the params cannot be exported.
  std::optional<BackendParams> ExportReadOnlyBackendParams();

  // Returns params for an independent read-write connection to the instance, or
  // nothing if its backend is not operating or the params cannot be exported.
  std::optional<BackendParams> ExportReadWriteBackendParams();

  // Marks a backend as not suitable for use. This property applies to
  // all backends initialized with the same `BackendParam`s. This is different
  // from deleting the backing files which is done to completely get rid of the
  // data contained.
  void Abandon();

  Backend* GetBackendForTesting();

 private:
  std::optional<base::ElapsedTimer> MaybeGetTimerForHistogram();
  std::string GetFullHistogramName(std::string_view name) const;

  std::unique_ptr<Backend> backend_;

  static constexpr double kTimingLoggingProbability = 0.01;
  base::MetricsSubSampler metrics_subsampler_
      GUARDED_BY(metrics_subsampler_lock_);
  base::Lock metrics_subsampler_lock_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_

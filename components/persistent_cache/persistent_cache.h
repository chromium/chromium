// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_
#define COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_

#include <stdint.h>

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
#include "components/persistent_cache/buffer_provider.h"
#include "components/persistent_cache/entry_metadata.h"
#include "components/persistent_cache/lock_state.h"

namespace persistent_cache {

class Backend;
struct PendingBackend;
enum class TransactionError;

// Use PersistentCache to store and retrieve key-value pairs across processes or
// threads.
//
// Example:
//    // Acquire a PendingBackend.
//    PendingBackend pending_backend = AcquirePendingBackend();
//    auto persistent_cache = PersistentCache::Bind(std::move(pending_backend));
//    if (!persistent_cache) {
//      // Handle error.
//    }
//
//    // Add a key-value pair.
//    persistent_cache->Insert("foo", base::byte_span_from_cstring("1"));
//
//    // Retrieve a value.
//    {
//      base::HeapArray<uint8_t> content;
//      ASSIGN_OR_RETURN(
//          auto metadata,
//          persistent_cache->Find("foo", [&content](size_t size) {
//              content = base::HeapArray<uint8_t>::Uninit(size);
//              return base::span(content);
//          }),
//        [](persistent_cache::TransactionError error) {
//          // Translate and handle error here.
//        });
//
//      if (metadata.has_value()) {
//        UseEntry(*std::move(metadata), std::move(content));
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
  // Returns a new instance on success or null on failure. Unconditionally
  // consumes `pending_backend`.
  static std::unique_ptr<PersistentCache> Bind(PendingBackend pending_backend);

  explicit PersistentCache(std::unique_ptr<Backend> backend);
  ~PersistentCache();

  // Not copyable or moveable.
  PersistentCache(const PersistentCache&) = delete;
  PersistentCache(PersistentCache&&) = delete;
  PersistentCache& operator=(const PersistentCache&) = delete;
  PersistentCache& operator=(PersistentCache&&) = delete;

  // Returns the metadata associated with `key` in the cache, or no value if not
  // found. If `key` is found, `buffer_provider` will be called exactly once
  // with the size of the content. If `buffer_provider` returns a non-empty span
  // (which must be sized exactly to the given content size), the content will
  // be written into it. If it returns an empty span, no data is copied. Note
  // that an error may be returned even if `buffer_provider` had been called.
  // See class comments regarding error management.
  //
  // Thread-safe.
  base::expected<std::optional<EntryMetadata>, TransactionError> Find(
      std::string_view key,
      BufferProvider buffer_provider);

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

  // Marks the instance as no longer suitable for use. Returns the state of the
  // shared lock at the moment of abandonment. Once an instance is abandoned,
  // all other instances that share a connection with it will report
  // `TransactionError::kConnectionError` for all operations.
  LockState Abandon();

  Backend* GetBackendForTesting();

 private:
  friend class BackendStorage;

  const Backend& backend() const { return *backend_; }

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

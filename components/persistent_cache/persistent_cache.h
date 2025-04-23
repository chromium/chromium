// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_
#define COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_

#include <memory>
#include <string_view>
#include <utility>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "components/persistent_cache/backend_params.h"
#include "components/persistent_cache/entry_metadata.h"

namespace persistent_cache {

class Entry;
class Backend;

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
//      auto entry = persistent_cache->Find("foo");
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

  // Used to get a handle to entry associated with `key`. Returns `nullptr` if
  // `key` is not found. Returned entry will remain valid and its contents will
  // be accessible for its entire lifetime. Note: Persistent caches have to
  // outlive entries they vend.
  //
  // Thread-safe.
  std::unique_ptr<Entry> Find(std::string_view key);

  // Used to add an entry containing `content` and associated with `key`.
  //
  // Thread-safe.
  void Insert(std::string_view key,
              base::span<const uint8_t> content,
              EntryMetadata metadata = EntryMetadata{});

  Backend* GetBackendForTesting();

 private:
  std::unique_ptr<Backend> backend_;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_PERSISTENT_CACHE_H_

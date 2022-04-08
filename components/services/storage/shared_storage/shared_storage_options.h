// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_OPTIONS_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_OPTIONS_H_

#include <memory>

#include "base/time/time.h"

namespace storage {

struct SharedStorageDatabaseOptions;

// Bundles Finch-configurable constants for the `SharedStorageManager`,
// `AsyncSharedStorageDatabase`, and `SharedStorageDatabase` classes.
struct SharedStorageOptions {
  // The static `Create()` method accesses field trial params to populate one or
  // more attributes, and so must be called on the main thread.
  static std::unique_ptr<SharedStorageOptions> Create();

  SharedStorageOptions(int max_page_size,
                       int max_cache_size,
                       int max_entries_per_origin,
                       int max_string_length,
                       int max_init_tries,
                       int max_iterator_batch_size,
                       base::TimeDelta stale_origin_purge_initial_interval,
                       base::TimeDelta stale_origin_purge_recurring_interval,
                       base::TimeDelta origin_staleness_threshold);

  // Creates a pointer to a smaller bundle of just the constants that need to
  // be forwarded to `AsyncSharedStorageDatabase` and `SharedStorageDatabase`.
  std::unique_ptr<SharedStorageDatabaseOptions> GetDatabaseOptions();

  // The max size of a database page, in bytes. Must be a power of 2 between
  // 512 and 65536 inclusive.
  const int max_page_size;

  // The max size of the database cache, in pages.
  const int max_cache_size;

  // The maximum number of entries allowed per origin.
  const int max_entries_per_origin;

  // The maximum allowed string length for each script key or script value.
  const int max_string_length;

  // The maximum number of times that `SharedStorageDatabase` will try to
  // initialize the SQL database.
  const int max_init_tries;

  // Maximum number of keys or key-value pairs returned per batch by the
  // async `Keys()` and `Entries()` iterators, respectively.
  const int max_iterator_batch_size;

  // The initial interval at which stale origins are purged.
  const base::TimeDelta stale_origin_purge_initial_interval;

  // The recurring interval at which stale origins are purged. May differ from
  // the initial interval.
  const base::TimeDelta stale_origin_purge_recurring_interval;

  // The amount of time that an origin needs to be inactive in order for it to
  // be deemed stale.
  const base::TimeDelta origin_staleness_threshold;
};

// Bundles Finch-configurable constants for the `AsyncSharedStorageDatabase`
// and `SharedStorageDatabase` classes. This smaller class is separate from the
// larger `SharedStorageOptions` (which has the ability to create an instance of
// `SharedStorageDatabaseOptions` from a subset of its members) so that the
// smaller `SharedStorageDatabaseOptions` bundle can be read on an alternate
// thread while the larger class's bundle can continue to be accessed on the
// main thread.
struct SharedStorageDatabaseOptions {
  SharedStorageDatabaseOptions(int max_page_size,
                               int max_cache_size,
                               int max_entries_per_origin,
                               int max_string_length,
                               int max_init_tries,
                               int max_iterator_batch_size);

  // The max size of a database page, in bytes. Must be a power of 2 between
  // 512 and 65536 inclusive.
  const int max_page_size;

  // The max size of the database cache, in pages.
  const int max_cache_size;

  // The maximum number of entries allowed per origin.
  const int max_entries_per_origin;

  // The maximum allowed string length for each script key or script value.
  const int max_string_length;

  // The maximum number of times that `SharedStorageDatabase` will try to
  // initialize the SQL database.
  const int max_init_tries;

  // Maximum number of keys or key-value pairs returned per batch by the
  // async `Keys()` and `Entries()` iterators, respectively.
  const int max_iterator_batch_size;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_OPTIONS_H_

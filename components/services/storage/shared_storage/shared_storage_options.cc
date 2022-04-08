// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_options.h"

#include "base/bits.h"
#include "third_party/blink/public/common/features.h"

namespace storage {

namespace {

bool IsValidPageSize(int page_size) {
  if (page_size < 512 || page_size > 65536)
    return false;
  return base::bits::IsPowerOfTwo(page_size);
}

}  // namespace

// static
std::unique_ptr<SharedStorageOptions> SharedStorageOptions::Create() {
  return std::make_unique<SharedStorageOptions>(
      blink::features::kMaxSharedStoragePageSize.Get(),
      blink::features::kMaxSharedStorageCacheSize.Get(),
      blink::features::kMaxSharedStorageEntriesPerOrigin.Get(),
      blink::features::kMaxSharedStorageStringLength.Get(),
      blink::features::kMaxSharedStorageInitTries.Get(),
      blink::features::kMaxSharedStorageIteratorBatchSize.Get(),
      blink::features::kSharedStorageStaleOriginPurgeInitialInterval.Get(),
      blink::features::kSharedStorageStaleOriginPurgeRecurringInterval.Get(),
      blink::features::kSharedStorageOriginStalenessThreshold.Get());
}

SharedStorageOptions::SharedStorageOptions(
    int max_page_size,
    int max_cache_size,
    int max_entries_per_origin,
    int max_string_length,
    int max_init_tries,
    int max_iterator_batch_size,
    base::TimeDelta stale_origin_purge_initial_interval,
    base::TimeDelta stale_origin_purge_recurring_interval,
    base::TimeDelta origin_staleness_threshold)
    : max_page_size(max_page_size),
      max_cache_size(max_cache_size),
      max_entries_per_origin(max_entries_per_origin),
      max_string_length(max_string_length),
      max_init_tries(max_init_tries),
      max_iterator_batch_size(max_iterator_batch_size),
      stale_origin_purge_initial_interval(stale_origin_purge_initial_interval),
      stale_origin_purge_recurring_interval(
          stale_origin_purge_recurring_interval),
      origin_staleness_threshold(origin_staleness_threshold) {
  DCHECK(IsValidPageSize(max_page_size));
}

std::unique_ptr<SharedStorageDatabaseOptions>
SharedStorageOptions::GetDatabaseOptions() {
  return std::make_unique<SharedStorageDatabaseOptions>(
      max_page_size, max_cache_size, max_entries_per_origin, max_string_length,
      max_init_tries, max_iterator_batch_size);
}

SharedStorageDatabaseOptions::SharedStorageDatabaseOptions(
    int max_page_size,
    int max_cache_size,
    int max_entries_per_origin,
    int max_string_length,
    int max_init_tries,
    int max_iterator_batch_size)
    : max_page_size(max_page_size),
      max_cache_size(max_cache_size),
      max_entries_per_origin(max_entries_per_origin),
      max_string_length(max_string_length),
      max_init_tries(max_init_tries),
      max_iterator_batch_size(max_iterator_batch_size) {
  DCHECK(IsValidPageSize(max_page_size));
}

}  // namespace storage

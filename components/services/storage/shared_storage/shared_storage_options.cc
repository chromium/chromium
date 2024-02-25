// Copyright 2021 The Chromium Authors
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
  return base::bits::IsPowerOfTwoDeprecatedDoNotUse(page_size);
}

}  // namespace

// static
std::unique_ptr<SharedStorageOptions> SharedStorageOptions::Create() {
  return std::make_unique<SharedStorageOptions>(
      blink::features::kMaxSharedStoragePageSize.Get(),
      blink::features::kMaxSharedStorageCacheSize.Get(),
      blink::features::kMaxSharedStorageBytesPerOrigin.Get(),
      blink::features::kMaxSharedStorageInitTries.Get(),
      blink::features::kMaxSharedStorageIteratorBatchSize.Get(),
      blink::features::kSharedStorageBitBudget.Get(),
      blink::features::kSharedStorageBudgetInterval.Get(),
      blink::features::kSharedStorageStalePurgeInitialInterval.Get(),
      blink::features::kSharedStorageStalePurgeRecurringInterval.Get(),
      blink::features::kSharedStorageStalenessThreshold.Get());
}

SharedStorageOptions::SharedStorageOptions(
    int max_page_size,
    int max_cache_size,
    int max_bytes_per_origin,
    int max_init_tries,
    int max_iterator_batch_size,
    int bit_budget,
    base::TimeDelta budget_interval,
    base::TimeDelta stale_purge_initial_interval,
    base::TimeDelta stale_purge_recurring_interval,
    base::TimeDelta staleness_threshold)
    : max_page_size(max_page_size),
      max_cache_size(max_cache_size),
      max_bytes_per_origin(max_bytes_per_origin),
      max_init_tries(max_init_tries),
      max_iterator_batch_size(max_iterator_batch_size),
      bit_budget(bit_budget),
      budget_interval(budget_interval),
      stale_purge_initial_interval(stale_purge_initial_interval),
      stale_purge_recurring_interval(stale_purge_recurring_interval),
      staleness_threshold(staleness_threshold) {
  DCHECK(IsValidPageSize(max_page_size));
  DCHECK_GT(max_bytes_per_origin, 0);
  DCHECK_GT(max_init_tries, 0);
  DCHECK_GT(max_iterator_batch_size, 0);
  DCHECK_GT(bit_budget, 0);
  DCHECK(budget_interval.is_positive());
  DCHECK(stale_purge_initial_interval.is_positive());
  DCHECK(stale_purge_recurring_interval.is_positive());
  DCHECK(staleness_threshold.is_positive());
}

std::unique_ptr<SharedStorageDatabaseOptions>
SharedStorageOptions::GetDatabaseOptions() {
  return std::make_unique<SharedStorageDatabaseOptions>(
      max_page_size, max_cache_size, max_bytes_per_origin, max_init_tries,
      max_iterator_batch_size, bit_budget, budget_interval,
      staleness_threshold);
}

SharedStorageDatabaseOptions::SharedStorageDatabaseOptions(
    int max_page_size,
    int max_cache_size,
    int max_bytes_per_origin,
    int max_init_tries,
    int max_iterator_batch_size,
    int bit_budget,
    base::TimeDelta budget_interval,
    base::TimeDelta staleness_threshold)
    : max_page_size(max_page_size),
      max_cache_size(max_cache_size),
      max_bytes_per_origin(max_bytes_per_origin),
      max_init_tries(max_init_tries),
      max_iterator_batch_size(max_iterator_batch_size),
      bit_budget(bit_budget),
      budget_interval(budget_interval),
      staleness_threshold(staleness_threshold) {
  DCHECK(IsValidPageSize(max_page_size));
  DCHECK_GT(max_bytes_per_origin, 0);
  DCHECK_GT(max_init_tries, 0);
  DCHECK_GT(max_iterator_batch_size, 0);
  DCHECK_GT(bit_budget, 0);
  DCHECK(budget_interval.is_positive());
  DCHECK(staleness_threshold.is_positive());
}

}  // namespace storage

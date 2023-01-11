// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_CONDITIONAL_CACHE_COUNTING_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_CONDITIONAL_CACHE_COUNTING_HELPER_H_

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "net/base/net_errors.h"

namespace content {
class StoragePartition;
}

namespace browsing_data {

// Helper to count the size of the http cache data from a StoragePartition.
class ConditionalCacheCountingHelper {
 public:
  // Returns if this value is an upper estimate and the number bytes in the
  // selected range.
  typedef base::OnceCallback<void(bool, int64_t)> CacheCountCallback;

  ConditionalCacheCountingHelper(const ConditionalCacheCountingHelper&) =
      delete;
  ConditionalCacheCountingHelper& operator=(
      const ConditionalCacheCountingHelper&) = delete;

  // Counts the cache entries according to the specified time range.
  // Must be called on the UI thread.
  //
  // The |completion_callback| will be invoked when the operation completes.
  static void Count(content::StoragePartition* storage_partition,
                    base::Time begin_time,
                    base::Time end_time,
                    CacheCountCallback result_callback);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_CONDITIONAL_CACHE_COUNTING_HELPER_H_

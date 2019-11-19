// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_count_metrics/tab_count_metrics.h"

#include <limits>

#include "base/logging.h"
#include "base/stl_util.h"

namespace tab_count_metrics {

// These values represent the lower bound for each bucket, and define the
// tab count buckets. The final bucket has no upper bound, and each other
// bucket, i, is bounded above by the lower bound of bucket i + 1.
//
// The buckets were determined from the Tabs.MaxTabsInADay histogram,
// approximating the 25th, 50th, 75th, 95th, and 99th percentiles, but with the
// single and zero tab cases separated.
//
// If adding or removing a bucket, update |kNumTabCountBuckets|,
// |kTabCountBucketMins|, and |kTabCountBucketNames|. If adding,
// removing, or changing bucket ranges, the existing metrics that use these
// functions for emitting histograms should be marked as obsolete, and new
// metrics should be created. This can be accomplished by versioning
// |kTabCountBucketNames|, e.g.  ".ByTabCount2.0Tabs", etc., and
// updating the histogram suffixes section of histograms.xml, creating a new
// entry for the new suffixes and marking the old suffixes obsolete.
constexpr size_t kTabCountBucketMins[] = {0, 1, 2, 3, 5, 8, 20, 40};

constexpr const char* kTabBucketNamePrefix[]{".ByTabCount", ".ByLiveTabCount"};

// Text for the tab count portion of metric names. These need to be kept
// in sync with |kTabCountBucketMins|.
constexpr const char* kTabCountBucketNames[]{
    ".0Tabs",    ".1Tab",      ".2Tabs",      ".3To4Tabs",
    ".5To7Tabs", ".8To19Tabs", ".20To39Tabs", ".40OrMoreTabs"};

std::string HistogramName(const std::string prefix,
                          bool live_tabs_only,
                          size_t bucket) {
  static_assert(base::size(kTabCountBucketMins) == kNumTabCountBuckets,
                "kTabCountBucketMins must have kNumTabCountBuckets elements.");
  static_assert(base::size(kTabCountBucketNames) == kNumTabCountBuckets,
                "kTabCountBucketNames must have kNumTabCountBuckets elements.");
  DCHECK_LT(bucket, kNumTabCountBuckets);
  DCHECK(prefix.length());
  return prefix + kTabBucketNamePrefix[live_tabs_only ? 1u : 0u] +
         kTabCountBucketNames[bucket];
}

size_t BucketForTabCount(size_t num_tabs) {
  for (size_t bucket = 0; bucket < kNumTabCountBuckets; bucket++) {
    if (internal::IsInBucket(num_tabs, bucket))
      return bucket;
  }
  // There should be a bucket for any number of tabs >= 0.
  NOTREACHED();
  return kNumTabCountBuckets;
}

namespace internal {

size_t BucketMin(size_t bucket) {
  DCHECK_LT(bucket, kNumTabCountBuckets);
  return kTabCountBucketMins[bucket];
}

size_t BucketMax(size_t bucket) {
  DCHECK_LT(bucket, kNumTabCountBuckets);
  // The last bucket includes everything after the min bucket value.
  if (bucket == kNumTabCountBuckets - 1)
    return std::numeric_limits<size_t>::max();
  return kTabCountBucketMins[bucket + 1] - 1;
}

bool IsInBucket(size_t num_tabs, size_t bucket) {
  DCHECK_LT(bucket, kNumTabCountBuckets);
  return num_tabs >= BucketMin(bucket) && num_tabs <= BucketMax(bucket);
}

}  // namespace internal

}  // namespace tab_count_metrics

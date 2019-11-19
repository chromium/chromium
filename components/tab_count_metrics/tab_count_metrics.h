// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_COUNT_METRICS_TAB_COUNT_METRICS_H_
#define COMPONENTS_TAB_COUNT_METRICS_TAB_COUNT_METRICS_H_

#include <string>

#include "base/component_export.h"

// This namespace contains functions for creating histograms bucketed by number
// of tabs.
//
// All bucket parameters --- number of buckets, bucket sizes, bucket names ---
// are determined at compile time, and these methods are safe to call from any
// thread.
//
// A typical example of creating a histogram bucketed by tab count using
// STATIC_HISTOGRAM_POINTER_GROUP looks something like this:
//  const size_t live_tab_count = GetLiveTabCount();
//  const size_t bucket =
//      tab_count_metrics::BucketForTabCount(live_tab_count);
//  STATIC_HISTOGRAM_POINTER_GROUP(
//      tab_count_metrics::HistogramName(constant_histogram_prefix,
//                                       live_tabs_only, bucket),
//      static_cast<int>(bucket),
//      static_cast<int>(tab_count_metrics::kNumTabCountBuckets),
//      Add(sample),
//      base::Histogram::FactoryGet(
//          tab_count_metrics::HistogramName(constant_histogram_prefix,
//                                           live_tabs_only, bucket),
//          MINIMUM_SAMPLE, MAXIMUM_SAMPLE, BUCKET_COUNT,
//          base::HistogramBase::kUmaTargetedHistogramFlag));
//  }
namespace tab_count_metrics {

// |kNumTabCountBuckets| is used in various constexpr arrays and as a bound
// on the histogram array when using STATIC_HISTOGRAM_POINTER_GROUP. This value
// must be equal to the length of the array of tab count bucket min values
// (|kTabCountBucketMins|) and the array of bucket names
// (|kTabCountBucketNames|) found in the corresponding .cc file.
constexpr size_t kNumTabCountBuckets = 8;

// Returns the histogram name for |bucket|. The histogram name is the
// concatenation of |prefix| and the name corresponding to |bucket|, which is of
// the form |prefix| + ".ByTabCount."/".ByLiveTabCount." + <BucketRangeText>,
// where <BucketRangeText> is a string describing the bucket range, e.g. "1Tab",
// "3To4Tabs", etc. See |kTabCountBucketNames| for all of the bucket names.
// |bucket| must be in the interval [0, |kNumTabCountBuckets|).
COMPONENT_EXPORT(TAB_COUNT_METRICS)
std::string HistogramName(const std::string prefix,
                          bool live_tabs_only,
                          size_t bucket);

// Return the bucket index for the |num_tabs|.
COMPONENT_EXPORT(TAB_COUNT_METRICS)
size_t BucketForTabCount(size_t num_tabs);

// These are exposed for unit tests.
namespace internal {

// Returns the number of tabs corresponding to the minimum value of |bucket|.
// |bucket| must be in the interval [0, |kNumTabCountBuckets|).
COMPONENT_EXPORT(TAB_COUNT_METRICS)
size_t BucketMin(size_t bucket);

// Returns the number of tabs corresponding to the maximum value of |bucket|.
// |bucket| must be in the interval [0, |kNumTabCountBuckets|).
COMPONENT_EXPORT(TAB_COUNT_METRICS)
size_t BucketMax(size_t bucket);

// Returns true if |num_tabs| falls within |bucket|.
// |bucket| must be in the interval [0, |kNumTabCountBuckets|).
COMPONENT_EXPORT(TAB_COUNT_METRICS)
bool IsInBucket(size_t num_tabs, size_t bucket);

}  // namespace internal

}  // namespace tab_count_metrics

#endif  // COMPONENTS_TAB_COUNT_METRICS_TAB_COUNT_METRICS_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_METRICS_CAST_HISTOGRAMS_H_
#define CHROMECAST_BASE_METRICS_CAST_HISTOGRAMS_H_

#include "base/metrics/histogram.h"

// STATIC_HISTOGRAM_POINTER_BLOCK only calls histogram_factory_get_invocation
// at the first time and uses the cached histogram_pointer for subsequent calls
// through base::subtle::Release_Store() and base::subtle::Acquire_Load().
// If the histogram name changes between subsequent calls, use this non-cached
// version that always calls histogram_factory_get_invocation.
#define STATIC_HISTOGRAM_POINTER_BLOCK_NO_CACHE(                               \
    constant_histogram_name, histogram_add_method_invocation,                  \
    histogram_factory_get_invocation)                                          \
  do {                                                                         \
    base::HistogramBase* histogram_pointer = histogram_factory_get_invocation; \
    if (DCHECK_IS_ON())                                                        \
      histogram_pointer->CheckName(constant_histogram_name);                   \
    histogram_pointer->histogram_add_method_invocation;                        \
  } while (0)

#define UMA_HISTOGRAM_CUSTOM_TIMES_NO_CACHE(name, sample, min, max, \
                                            bucket_count)           \
  STATIC_HISTOGRAM_POINTER_BLOCK_NO_CACHE(                          \
      name, AddTimeMillisecondsGranularity(sample),                 \
      base::Histogram::FactoryTimeGet(                              \
          name, min, max, bucket_count,                             \
          base::Histogram::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_CUSTOM_COUNTS_NO_CACHE(name, sample, min, max, \
                                             bucket_count, count) \
    STATIC_HISTOGRAM_POINTER_BLOCK_NO_CACHE(name, AddCount(sample, count), \
        base::Histogram::FactoryGet(name, min, max, bucket_count, \
            base::HistogramBase::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_ENUMERATION_NO_CACHE(name, sample, boundary_value) \
    STATIC_HISTOGRAM_POINTER_BLOCK_NO_CACHE(name, Add(sample), \
        base::LinearHistogram::FactoryGet(name, 1, boundary_value, \
            boundary_value + 1, base::Histogram::kUmaTargetedHistogramFlag))

#endif  // CHROMECAST_BASE_METRICS_CAST_HISTOGRAMS_H_

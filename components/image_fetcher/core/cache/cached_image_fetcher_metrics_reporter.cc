// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"

#include "base/metrics/histogram_macros.h"

namespace image_fetcher {

// static
void CachedImageFetcherMetricsReporter::ReportEvent(
    CachedImageFetcherEvent event) {
  UMA_HISTOGRAM_ENUMERATION("CachedImageFetcher.Events", event);
}

// static
void CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(
    base::Time start_time) {
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES("CachedImageFetcher.ImageLoadFromCacheTime", time_delta);
}

// static
void CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
    base::Time start_time) {
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES("CachedImageFetcher.ImageLoadFromNetworkTime",
                      time_delta);
}

// static
void CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
    base::Time start_time) {
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES("CachedImageFetcher.ImageLoadFromNetworkAfterCacheHit",
                      time_delta);
}

// static
void CachedImageFetcherMetricsReporter::ReportTimeSinceLastCacheLRUEviction(
    base::Time start_time) {
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES("CachedImageFetcher.TimeSinceLastCacheLRUEviction",
                      time_delta);
}

}  // namespace image_fetcher

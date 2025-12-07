// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/numerics/clamped_math.h"

namespace image_fetcher {

const char
    ImageFetcherMetricsReporter::kCachedImageFetcherInternalUmaClientName[] =
        "Internal";

namespace {

// 10 seconds in milliseconds.
const int kMaxReportTimeMs = 10 * 1000;
const int kMaxCacheSizeKb = 1024 * 300; /* 300MB */
const int kMaxCacheMetadataCount = 1000;

constexpr char kEventsHistogram[] = "ImageFetcher.Events";
constexpr char kImageLoadFromCacheHistogram[] =
    "ImageFetcher.ImageLoadFromCacheTime";
constexpr char kImageLoadFromCacheJavaHistogram[] =
    "ImageFetcher.ImageLoadFromCacheTimeJava";
constexpr char kTotalFetchFromNativeTimeJavaHistogram[] =
    "ImageFetcher.ImageLoadFromNativeTimeJava";
constexpr char kImageLoadFromNetworkHistogram[] =
    "ImageFetcher.ImageLoadFromNetworkTime";
constexpr char kImageLoadFromNetworkAfterCacheHitHistogram[] =
    "ImageFetcher.ImageLoadFromNetworkAfterCacheHit";
constexpr char kTimeSinceLastLRUEvictionHistogram[] =
    "ImageFetcher.TimeSinceLastCacheLRUEviction";
constexpr char kLoadImageMetadata[] = "ImageFetcher.LoadImageMetadata";
constexpr char kNetworkRequestStatusCodes[] = "ImageFetcher.RequestStatusCode";
constexpr char kImageCacheSize[] = "ImageFetcher.CacheSize";
constexpr char kImageCacheMetadataCount[] = "ImageFetcher.CacheMetadataCount";

// Returns a raw pointer to a histogram which is owned
base::HistogramBase* GetTimeHistogram(const std::string& histogram_name,
                                      const std::string& client_name) {
  return base::LinearHistogram::FactoryTimeGet(
      histogram_name + "." + client_name, base::TimeDelta(),
      base::Milliseconds(kMaxReportTimeMs),
      /* one bucket every 20ms. */ kMaxReportTimeMs / 20,
      base::Histogram::kUmaTargetedHistogramFlag);
}

// Appends the cache strategy suffix. Needs to match ImageFetcherCacheStrategy
// in histograms.xml.
void AppendSuffix(CacheOption cache_option, std::string* name) {
  DCHECK(name);
  switch (cache_option) {
    case CacheOption::kBestEffort:
      name->append(".BestEffort");
      break;
    case CacheOption::kHoldUntilExpired:
      name->append(".HoldUntilExpired");
      break;
  }
}

}  // namespace

// static
void ImageFetcherMetricsReporter::ReportEvent(const std::string& client_name,
                                              ImageFetcherEvent event) {
  DCHECK(!client_name.empty());
  UMA_HISTOGRAM_ENUMERATION(kEventsHistogram, event);
  base::LinearHistogram::FactoryGet(
      kEventsHistogram + std::string(".") + client_name, 0,
      static_cast<int>(ImageFetcherEvent::kMaxValue),
      static_cast<int>(ImageFetcherEvent::kMaxValue),
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(static_cast<int>(event));
}

// static
void ImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromCacheHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromCacheHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void ImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromCacheJavaHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromCacheJavaHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void ImageFetcherMetricsReporter::ReportTotalFetchFromNativeTimeJava(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kTotalFetchFromNativeTimeJavaHistogram, time_delta);
  GetTimeHistogram(kTotalFetchFromNativeTimeJavaHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void ImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromNetworkHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromNetworkHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void ImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
    const std::string& client_name,
    base::Time start_time) {
  DCHECK(!client_name.empty());
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kImageLoadFromNetworkAfterCacheHitHistogram, time_delta);
  GetTimeHistogram(kImageLoadFromNetworkAfterCacheHitHistogram, client_name)
      ->Add(time_delta.InMilliseconds());
}

// static
void ImageFetcherMetricsReporter::ReportTimeSinceLastCacheLRUEviction(
    base::Time start_time) {
  base::TimeDelta time_delta = base::Time::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kTimeSinceLastLRUEvictionHistogram, time_delta);
}

// static
void ImageFetcherMetricsReporter::ReportLoadImageMetadata(
    base::TimeTicks start_time) {
  base::TimeDelta time_delta = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES(kLoadImageMetadata, time_delta);
}

// static
void ImageFetcherMetricsReporter::ReportRequestStatusCode(
    const std::string& client_name,
    int code) {
  DCHECK(!client_name.empty());
  base::UmaHistogramSparse(kNetworkRequestStatusCodes, code);
  base::UmaHistogramSparse(
      kNetworkRequestStatusCodes + std::string(".") + client_name, code);
}

// static
void ImageFetcherMetricsReporter::ReportCacheStatus(CacheOption cache_option,
                                                    size_t total_bytes,
                                                    int metadata_count) {
  std::string cache_size_name = kImageCacheSize;
  AppendSuffix(cache_option, &cache_size_name);
  int total_size_kb = base::saturated_cast<int>(total_bytes / 1024);
  base::UmaHistogramCustomCounts(cache_size_name, total_size_kb, 1,
                                 kMaxCacheSizeKb, 50);

  std::string metadata_count_name = kImageCacheMetadataCount;
  AppendSuffix(cache_option, &metadata_count_name);
  base::UmaHistogramCustomCounts(metadata_count_name, metadata_count, 1,
                                 kMaxCacheMetadataCount, 50);
}

}  // namespace image_fetcher

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_METRICS_REPORTER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_METRICS_REPORTER_H_

#include <string>

#include "base/time/time.h"
#include "components/image_fetcher/core/cache/image_store_types.h"

namespace image_fetcher {

// Results of backing-store operations. Each completed operation contributes
// one sample, including successful operations and missing paths. A missing
// image-file path can also mean that its storage directory was removed.
// Values are persisted to logs and must not be renumbered or reused.
// LINT.IfChange(ImageFetcherCacheResult)
enum class ImageFetcherCacheResult {
  kDataStoreInitializationSucceeded = 0,
  kDataStoreInitializationFailed = 1,
  kDataStoreReadSucceeded = 2,
  kDataStoreReadNotFound = 3,
  kDataStoreReadFailed = 4,
  kDataStoreWriteSucceeded = 5,
  kDataStoreWriteFailed = 6,
  kDataStoreDeleteSucceeded = 7,
  kDataStoreDeleteFailed = 8,
  kMetadataStoreInitializationSucceeded = 9,
  kMetadataStoreInitializationFailed = 10,
  kMetadataStoreReadSucceeded = 11,
  kMetadataStoreReadMiss = 12,
  kMetadataStoreReadFailed = 13,
  kMetadataStoreUpdateSucceeded = 14,
  kMetadataStoreUpdateFailed = 15,
  kMaxValue = kMetadataStoreUpdateFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ImageFetcherCacheResult)

// Enum for the result of the fetch, reported through UMA. Present in enums.xml
// as ImageFetcherEvent. New values should be added at the end and things
// should not be renumbered.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.image_fetcher
// LINT.IfChange(ImageFetcherEvent)
enum class ImageFetcherEvent {
  kImageRequest = 0,
  kCacheHit = 1,
  kCacheMiss = 2,
  kCacheDecodingError = 3,
  kTranscodingError = 4,
  kTotalFailure = 5,
  kCacheStartupEvictionStarted = 6,
  kCacheStartupEvictionFinished = 7,
  kJavaInMemoryCacheHit = 8,
  kJavaDiskCacheHit = 9,
  kImageQueuedForTranscodingDecoded = 10,
  kImageQueuedForTranscodingStoredBack = 11,
  kLoadImageMetadata = 12,
  kCacheDataStoreInitializationFailed = 13,
  kCacheMetadataStoreInitializationFailed = 14,
  kMaxValue = kCacheMetadataStoreInitializationFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ImageFetcherEvent)

class ImageFetcherMetricsReporter {
 public:
  // For use in metrics that aren't client-specific.
  static const char kCachedImageFetcherInternalUmaClientName[];

  // Report cache events, used by CachedImageFetcher and composing classes.
  static void ReportEvent(const std::string& client_name,
                          ImageFetcherEvent event);

  // Report one completed backing-store operation.
  static void ReportCacheResult(ImageFetcherCacheResult result);

  // Report the time it takes to load an image from the cache in native code.
  static void ReportImageLoadFromCacheTime(const std::string& client_name,
                                           base::Time start_time);

  // Report the time it takes to load an image from the cache in java code.
  static void ReportImageLoadFromCacheTimeJava(const std::string& client_name,
                                               base::Time start_time);

  // Report the time it takes to load an image from native code.
  static void ReportTotalFetchFromNativeTimeJava(const std::string& client_name,
                                                 base::Time start_time);

  // Report the time it takes to load an image from the network.
  static void ReportImageLoadFromNetworkTime(const std::string& client_name,
                                             base::Time start_time);

  // Report the time it takes to load an image from the network after a cache
  // hit.
  static void ReportImageLoadFromNetworkAfterCacheHit(
      const std::string& client_name,
      base::Time start_time);

  // Report the time between cache evictions.
  static void ReportTimeSinceLastCacheLRUEviction(base::Time start_time);

  // Report the time it takes to load metadata.
  static void ReportLoadImageMetadata(base::TimeTicks start_time);

  // Report the network error for the network fetch.
  static void ReportRequestStatusCode(const std::string& client_name, int code);

  // Report the total cache size and number of metadata entries for a given
  // CacheOption.
  static void ReportCacheStatus(CacheOption cache_option,
                                size_t total_bytes,
                                int metadata_count);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_METRICS_REPORTER_H_

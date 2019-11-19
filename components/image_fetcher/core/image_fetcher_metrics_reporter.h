// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_METRICS_REPORTER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_METRICS_REPORTER_H_

#include <string>

#include "base/time/time.h"

namespace image_fetcher {

// Enum for the result of the fetch, reported through UMA. Present in enums.xml
// as ImageFetcherEvent. New values should be added at the end and things
// should not be renumbered.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.chrome.browser.image_fetcher)
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
  kMaxValue = kLoadImageMetadata,
};

class ImageFetcherMetricsReporter {
 public:
  // For use in metrics that aren't client-specific.
  static const char kCachedImageFetcherInternalUmaClientName[];

  // Report cache events, used by CachedImageFetcher and composing classes.
  static void ReportEvent(const std::string& client_name,
                          ImageFetcherEvent event);

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
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_METRICS_REPORTER_H_

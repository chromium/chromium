// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_fetcher {

namespace {

const char kUmaClientName[] = "foo";
const char kUmaClientNameOther[] = "bar";

const char kImageFetcherEventHistogramName[] = "ImageFetcher.Events";
const char kCacheLoadHistogramName[] = "ImageFetcher.ImageLoadFromCacheTime";
const char kCacheLoadHistogramNameJava[] =
    "ImageFetcher.ImageLoadFromCacheTimeJava";
constexpr char kTotalFetchFromNativeHistogramNameJava[] =
    "ImageFetcher.ImageLoadFromNativeTimeJava";
const char kNetworkLoadHistogramName[] =
    "ImageFetcher.ImageLoadFromNetworkTime";
const char kNetworkLoadAfterCacheHitHistogram[] =
    "ImageFetcher.ImageLoadFromNetworkAfterCacheHit";
const char kTimeSinceLastCacheLRUEviction[] =
    "ImageFetcher.TimeSinceLastCacheLRUEviction";
constexpr char kNetworkRequestStatusCodes[] = "ImageFetcher.RequestStatusCode";

}  // namespace

class ImageFetcherMetricsReporterTest : public testing::Test {
 public:
  ImageFetcherMetricsReporterTest() {}
  ~ImageFetcherMetricsReporterTest() override = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherMetricsReporterTest);
};

TEST_F(ImageFetcherMetricsReporterTest, TestReportEvent) {
  ImageFetcherMetricsReporter::ReportEvent(kUmaClientName,
                                           ImageFetcherEvent::kCacheHit);
  ImageFetcherMetricsReporter::ReportEvent(kUmaClientNameOther,
                                           ImageFetcherEvent::kCacheHit);
  histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                       ImageFetcherEvent::kCacheHit, 2);
  histogram_tester().ExpectBucketCount(
      std::string(kImageFetcherEventHistogramName)
          .append(".")
          .append(kUmaClientName),
      ImageFetcherEvent::kCacheHit, 1);
  histogram_tester().ExpectBucketCount(
      std::string(kImageFetcherEventHistogramName)
          .append(".")
          .append(kUmaClientNameOther),
      ImageFetcherEvent::kCacheHit, 1);
}

TEST_F(ImageFetcherMetricsReporterTest, TestReportImageLoadFromCacheTime) {
  ImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(kUmaClientName,
                                                            base::Time());
  ImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(kUmaClientNameOther,
                                                            base::Time());
  histogram_tester().ExpectTotalCount(kCacheLoadHistogramName, 2);
  histogram_tester().ExpectTotalCount(
      std::string(kCacheLoadHistogramName).append(".").append(kUmaClientName),
      1);
  histogram_tester().ExpectTotalCount(std::string(kCacheLoadHistogramName)
                                          .append(".")
                                          .append(kUmaClientNameOther),
                                      1);
}

TEST_F(ImageFetcherMetricsReporterTest, TestReportImageLoadFromCacheTimeJava) {
  ImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(kUmaClientName,
                                                                base::Time());
  ImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kCacheLoadHistogramNameJava, 2);
  histogram_tester().ExpectTotalCount(std::string(kCacheLoadHistogramNameJava)
                                          .append(".")
                                          .append(kUmaClientName),
                                      1);
  histogram_tester().ExpectTotalCount(std::string(kCacheLoadHistogramNameJava)
                                          .append(".")
                                          .append(kUmaClientNameOther),
                                      1);
}

TEST_F(ImageFetcherMetricsReporterTest,
       TestReportTotalFetchFromNativeTimeJava) {
  ImageFetcherMetricsReporter::ReportTotalFetchFromNativeTimeJava(
      kUmaClientName, base::Time());
  ImageFetcherMetricsReporter::ReportTotalFetchFromNativeTimeJava(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kTotalFetchFromNativeHistogramNameJava,
                                      2);
  histogram_tester().ExpectTotalCount(
      std::string(kTotalFetchFromNativeHistogramNameJava)
          .append(".")
          .append(kUmaClientName),
      1);
  histogram_tester().ExpectTotalCount(
      std::string(kTotalFetchFromNativeHistogramNameJava)
          .append(".")
          .append(kUmaClientNameOther),
      1);
}

TEST_F(ImageFetcherMetricsReporterTest, TestReportImageLoadFromNetworkTime) {
  ImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(kUmaClientName,
                                                              base::Time());
  ImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kNetworkLoadHistogramName, 2);
  histogram_tester().ExpectTotalCount(
      std::string(kNetworkLoadHistogramName).append(".").append(kUmaClientName),
      1);
  histogram_tester().ExpectTotalCount(std::string(kNetworkLoadHistogramName)
                                          .append(".")
                                          .append(kUmaClientNameOther),
                                      1);
}

TEST_F(ImageFetcherMetricsReporterTest,
       TestReportImageLoadFromNetworkAfterCacheHit) {
  ImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
      kUmaClientName, base::Time());
  ImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
      kUmaClientNameOther, base::Time());
  histogram_tester().ExpectTotalCount(kNetworkLoadAfterCacheHitHistogram, 2);
  histogram_tester().ExpectTotalCount(
      std::string(kNetworkLoadAfterCacheHitHistogram)
          .append(".")
          .append(kUmaClientName),
      1);
  histogram_tester().ExpectTotalCount(
      std::string(kNetworkLoadAfterCacheHitHistogram)
          .append(".")
          .append(kUmaClientNameOther),
      1);
}

TEST_F(ImageFetcherMetricsReporterTest,
       TestReportTimeSinceLastCacheLRUEviction) {
  ImageFetcherMetricsReporter::ReportTimeSinceLastCacheLRUEviction(
      base::Time());
  histogram_tester().ExpectTotalCount(kTimeSinceLastCacheLRUEviction, 1);
}

TEST_F(ImageFetcherMetricsReporterTest, TestReportReponseStatusCode) {
  ImageFetcherMetricsReporter::ReportRequestStatusCode(kUmaClientNameOther,
                                                       200);
  histogram_tester().ExpectTotalCount(kNetworkRequestStatusCodes, 1);
}

}  // namespace image_fetcher

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store_metrics_impl.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(UnsentLogStoreMetricsImplTest, RecordDroppedLogSize) {
  UnsentLogStoreMetricsImpl impl;
  base::HistogramTester histogram_tester;

  impl.RecordDroppedLogSize(99999);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.DroppedSize", 99999, 1);
}

TEST(UnsentLogStoreMetricsImplTest, RecordDroppedLogsNum) {
  UnsentLogStoreMetricsImpl impl;
  base::HistogramTester histogram_tester;

  impl.RecordDroppedLogsNum(17);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.Dropped", 17, 1);
}

TEST(UnsentLogStoreMetricsImplTest, RecordLastUnsentLogMetadataMetrics) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(kRecordLastUnsentLogMetadataMetrics);
  UnsentLogStoreMetricsImpl impl;
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(99, 19999, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 99, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 19999, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage",
                                     99 * 100 / (99 + 19999), 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.PersistedSizeInKB", 63, 1);
}

TEST(UnsentLogStoreMetricsImplTest, DisableRecordLastUnsentLogMetadataMetrics) {
  UnsentLogStoreMetricsImpl impl;
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(99, 19999, 63);
  EXPECT_TRUE(
      histogram_tester.GetAllSamples("UMA.UnsentLogs.UnsentCount").empty());
  EXPECT_TRUE(
      histogram_tester.GetAllSamples("UMA.UnsentLogs.SentCount").empty());
  EXPECT_TRUE(histogram_tester.GetAllSamples("UMA.UnsentLogs.UnsentPercentage")
                  .empty());
  EXPECT_TRUE(histogram_tester.GetAllSamples("UMA.UnsentLogs.PersistedSizeInKB")
                  .empty());
}

TEST(UnsentLogStoreMetricsImplTest, BothUnsentAndSentZeroSample) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(kRecordLastUnsentLogMetadataMetrics);
  UnsentLogStoreMetricsImpl impl;
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(0, 0, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage", 0, 1);
}

TEST(UnsentLogStoreMetricsImplTest, ZeroUnsentSample) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(kRecordLastUnsentLogMetadataMetrics);
  UnsentLogStoreMetricsImpl impl;
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(0, 999999, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 999999, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage", 0, 1);
}

TEST(UnsentLogStoreMetricsImplTest, ZeroSentSample) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(kRecordLastUnsentLogMetadataMetrics);
  UnsentLogStoreMetricsImpl impl;
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(999, 0, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 999, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage", 100, 1);
}

}  // namespace metrics

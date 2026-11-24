// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store_metrics_impl.h"

#include <optional>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/log_store.h"
#include "components/metrics/unsent_log_store_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

// Verifies that the base UnsentLogStoreMetrics implementation used by non-UMA
// stores is a no-op and emits no UMA histograms.
TEST(UnsentLogStoreMetricsImplTest, BaseMetricsNoOp) {
  UnsentLogStoreMetrics base_metrics;
  base::HistogramTester histogram_tester;

  const std::optional<UnsentLogDropReason> kNoDrops[] = {std::nullopt};
  const std::optional<UnsentLogDropReason> kMixedDrops[] = {
      UnsentLogDropReason::kTrimmedQueueLimit, std::nullopt,
      UnsentLogDropReason::kTrimmedLogTooLarge};

  base_metrics.RecordLogReadStatus(UnsentLogStoreMetrics::RECALL_SUCCESS);
  base_metrics.RecordCompressionRatio(50, 100);
  base_metrics.RecordDroppedLog(UnsentLogDropReason::kTrimmedQueueLimit);
  base_metrics.RecordCorruptLogRead(3);
  base_metrics.RecordTrimmedLogs(kNoDrops, /*removed_from_memory=*/true);
  base_metrics.RecordTrimmedLogs(kMixedDrops, /*removed_from_memory=*/false);
  base_metrics.RecordTrimmedLogs(kMixedDrops, /*removed_from_memory=*/true);
  base_metrics.RecordLogsTrimmedOnDiskOnly(2);
  base_metrics.RecordDroppedLogSize(1000);
  base_metrics.RecordDroppedLogsNum(2);
  base_metrics.RecordLastUnsentLogMetadataMetrics(10, 20, 1);

  EXPECT_TRUE(histogram_tester.GetTotalCountsForPrefix("UMA.").empty());
}

TEST(UnsentLogStoreMetricsImplTest, RecordDroppedLogSize) {
  UnsentLogStoreMetricsImpl impl(UnsentLogStoreMetricsImpl::LogQueue::kOngoing);
  base::HistogramTester histogram_tester;

  impl.RecordDroppedLogSize(99999);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.DroppedSize", 99999, 1);
}

TEST(UnsentLogStoreMetricsImplTest, RecordDroppedLogsNum) {
  UnsentLogStoreMetricsImpl impl(UnsentLogStoreMetricsImpl::LogQueue::kOngoing);
  base::HistogramTester histogram_tester;

  impl.RecordDroppedLogsNum(17);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.Dropped", 17, 1);
}

TEST(UnsentLogStoreMetricsImplTest, RecordLastUnsentLogMetadataMetrics) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(kRecordLastUnsentLogMetadataMetrics);
  UnsentLogStoreMetricsImpl impl(UnsentLogStoreMetricsImpl::LogQueue::kOngoing);
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(99, 19999, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 99, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 19999, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage",
                                     99 * 100 / (99 + 19999), 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.PersistedSizeInKB", 63, 1);
}

TEST(UnsentLogStoreMetricsImplTest, DisableRecordLastUnsentLogMetadataMetrics) {
  UnsentLogStoreMetricsImpl impl(UnsentLogStoreMetricsImpl::LogQueue::kOngoing);
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
  UnsentLogStoreMetricsImpl impl(UnsentLogStoreMetricsImpl::LogQueue::kOngoing);
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(0, 0, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage", 0, 1);
}

TEST(UnsentLogStoreMetricsImplTest, ZeroUnsentSample) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(kRecordLastUnsentLogMetadataMetrics);
  UnsentLogStoreMetricsImpl impl(UnsentLogStoreMetricsImpl::LogQueue::kOngoing);
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(0, 999999, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 999999, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage", 0, 1);
}

TEST(UnsentLogStoreMetricsImplTest, ZeroSentSample) {
  base::test::ScopedFeatureList feature_override;
  feature_override.InitAndEnableFeature(kRecordLastUnsentLogMetadataMetrics);
  UnsentLogStoreMetricsImpl impl(UnsentLogStoreMetricsImpl::LogQueue::kOngoing);
  base::HistogramTester histogram_tester;

  impl.RecordLastUnsentLogMetadataMetrics(999, 0, 63);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentCount", 999, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.SentCount", 0, 1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.UnsentPercentage", 100, 1);
}

// Verifies that each log queue records per-queue metrics under its own
// {LogQueue} variant.
TEST(UnsentLogStoreMetricsImplTest, RecordDroppedLogsPerQueue) {
  struct QueueCase {
    UnsentLogStoreMetricsImpl::LogQueue log_queue;
    const char* queue_name;
  };
  const QueueCase kCases[] = {
      {UnsentLogStoreMetricsImpl::LogQueue::kInitial, "Initial"},
      {UnsentLogStoreMetricsImpl::LogQueue::kOngoing, "Ongoing"},
      {UnsentLogStoreMetricsImpl::LogQueue::kUser, "User"},
  };

  for (const QueueCase& test_case : kCases) {
    SCOPED_TRACE(test_case.queue_name);
    UnsentLogStoreMetricsImpl impl(test_case.log_queue);
    base::HistogramTester histogram_tester;
    const std::string prefix =
        base::StrCat({"UMA.UnsentLogs.", test_case.queue_name, "."});

    const std::optional<UnsentLogDropReason> kTrimDrops[] = {
        UnsentLogDropReason::kTrimmedQueueLimit,
        UnsentLogDropReason::kTrimmedQueueLimit, std::nullopt,
        UnsentLogDropReason::kTrimmedLogTooLarge};
    impl.RecordDroppedLogs(UnsentLogDropReason::kUploadBadRequest, 0);
    impl.RecordDroppedLog(UnsentLogDropReason::kUploadBadRequest);
    impl.RecordDroppedLogs(UnsentLogDropReason::kPurged, 4);
    impl.RecordTrimmedLogs(kTrimDrops, /*removed_from_memory=*/false);
    impl.RecordTrimmedLogs(kTrimDrops, /*removed_from_memory=*/true);
    impl.RecordCorruptLogRead(3);

    histogram_tester.ExpectBucketCount(
        prefix + "DropReason", UnsentLogDropReason::kTrimmedQueueLimit, 2);
    histogram_tester.ExpectBucketCount(
        prefix + "DropReason", UnsentLogDropReason::kTrimmedLogTooLarge, 1);
    histogram_tester.ExpectBucketCount(
        prefix + "DropReason", UnsentLogDropReason::kUploadBadRequest, 1);
    histogram_tester.ExpectBucketCount(prefix + "DropReason",
                                       UnsentLogDropReason::kPurged, 4);
    histogram_tester.ExpectBucketCount(prefix + "DropReason",
                                       UnsentLogDropReason::kPrefCorruption, 3);
    histogram_tester.ExpectTotalCount(prefix + "DropReason", 11);
    histogram_tester.ExpectUniqueSample("UMA.UnsentLogs.Dropped", 3, 1);
    histogram_tester.ExpectUniqueSample(prefix + "TrimmedOnDiskOnly", 3, 1);
    histogram_tester.ExpectUniqueSample(
        prefix + "LogReadStatus", UnsentLogStoreMetrics::LOG_STRING_CORRUPTION,
        1);
  }
}

// Verifies that `RecordDroppedLogs()` constructs `DropReason` with the exact
// same bucket ranges as `base::UmaHistogramEnumeration()`.
TEST(UnsentLogStoreMetricsImplTest,
     RecordDroppedLogsMatchesUmaHistogramEnumeration) {
  base::HistogramTester histogram_tester;

  // First create via UmaHistogramEnumeration, then append via
  // RecordDroppedLogs.
  base::UmaHistogramEnumeration("UMA.UnsentLogs.Initial.DropReason",
                                UnsentLogDropReason::kTrimmedQueueLimit);
  UnsentLogStoreMetricsImpl initial_impl(
      UnsentLogStoreMetricsImpl::LogQueue::kInitial);
  initial_impl.RecordDroppedLogs(UnsentLogDropReason::kPurged, 2);

  // Also test the reverse order: create via RecordDroppedLogs, then append via
  // UmaHistogramEnumeration.
  UnsentLogStoreMetricsImpl user_impl(
      UnsentLogStoreMetricsImpl::LogQueue::kUser);
  user_impl.RecordDroppedLogs(UnsentLogDropReason::kUserPreference, 3);
  base::UmaHistogramEnumeration("UMA.UnsentLogs.User.DropReason",
                                UnsentLogDropReason::kPurged);

  histogram_tester.ExpectTotalCount("Histogram.BadConstructionArguments", 0);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.Initial.DropReason",
                                     UnsentLogDropReason::kTrimmedQueueLimit,
                                     1);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.Initial.DropReason",
                                     UnsentLogDropReason::kPurged, 2);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.User.DropReason",
                                     UnsentLogDropReason::kUserPreference, 3);
  histogram_tester.ExpectBucketCount("UMA.UnsentLogs.User.DropReason",
                                     UnsentLogDropReason::kPurged, 1);
}

}  // namespace
}  // namespace metrics

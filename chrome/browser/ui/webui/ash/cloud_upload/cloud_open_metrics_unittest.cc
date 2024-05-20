// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include "base/debug/dump_without_crashing.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cloud_upload {

class MetricTest : public testing::Test {
 public:
  MetricTest() = default;

  MetricTest(const MetricTest&) = delete;
  MetricTest& operator=(const MetricTest&) = delete;

 protected:
  enum class TestEnum {
    kZero = 0,
    kOne = 1,
    kTwo = 2,
    kMaxValue = kTwo,
  };

  Metric<TestEnum> metric_ =
      Metric<TestEnum>("metric_name", "companion_metric_name");
  base::HistogramTester histogram_;
};

// Tests that Metric::Log() returns the correct bool and updates the `value` and
// `state` correctly.
TEST_F(MetricTest, Log) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);
  ASSERT_FALSE(metric_.logged());

  ASSERT_TRUE(metric_.Log(TestEnum::kOne));
  ASSERT_EQ(metric_.value, TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
  ASSERT_TRUE(metric_.logged());

  histogram_.ExpectUniqueSample("metric_name", TestEnum::kOne, 1);

  ASSERT_FALSE(metric_.Log(TestEnum::kZero));
  ASSERT_EQ(metric_.value, TestEnum::kZero);
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLoggedMultipleTimes);
  ASSERT_TRUE(metric_.logged());

  histogram_.ExpectBucketCount("metric_name", TestEnum::kZero, 1);
}

// Tests that Metric::IsNotLogged() returns the correct bool and doesn't update
// the `state` when the metric was not logged.
TEST_F(MetricTest, IsNotLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  ASSERT_TRUE(metric_.IsNotLogged());
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);
}

// Tests that Metric::IsNotLogged() returns the correct bool and updates the
// `state` correctly when the metric was logged.
TEST_F(MetricTest, IsNotLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  ASSERT_FALSE(metric_.IsNotLogged());
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLogged);
}

// Tests that Metric::IsLogged() returns the correct bool and doesn't update the
// `state` when the metric was logged.
TEST_F(MetricTest, IsLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  ASSERT_TRUE(metric_.IsLogged());
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests that Metric::IsLogged() returns the correct bool and doesn't update the
// `state` when the metric was logged twice.
TEST_F(MetricTest, IsLoggedWhenLoggedTwice) {
  metric_.Log(TestEnum::kOne);
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLoggedMultipleTimes);

  ASSERT_TRUE(metric_.IsLogged());
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLoggedMultipleTimes);
}

// Tests that Metric::IsLogged() returns the correct bool and updates the
// `state` correctly when the metric was not logged.
TEST_F(MetricTest, IsLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  ASSERT_FALSE(metric_.IsLogged());
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

class CloudOpenMetricsTest : public testing::Test {
 public:
  CloudOpenMetricsTest() = default;

  static void FakeDumpWithoutCrashing() { number_of_dump_calls_++; }

  static int number_of_dump_calls() { return number_of_dump_calls_; }

 protected:
  void SetUp() override {
    base::debug::SetDumpWithoutCrashingFunction(
        &CloudOpenMetricsTest::FakeDumpWithoutCrashing);
    number_of_dump_calls_ = 0;
  }

  void TearDown() override {
    base::debug::SetDumpWithoutCrashingFunction(nullptr);
    base::debug::ClearMapsForTesting();
  }

  static int number_of_dump_calls_;
  base::HistogramTester histogram_;
};

int CloudOpenMetricsTest::number_of_dump_calls_ = 0;

// Tests that the TaskResult companion metric is set correctly when TaskResult
// is logged.
TEST_F(CloudOpenMetricsTest, TaskResultLogged) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the TaskResult companion metric is set correctly and
// DumpWithoutCrashing is called after the destructor when TaskResult is not
// logged.
TEST_F(CloudOpenMetricsTest, TaskResultNotLogged) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the TaskResult companion metric is set correctly and
// DumpWithoutCrashing is called immediately when TaskResult is logged twice.
TEST_F(CloudOpenMetricsTest, TaskResultLoggedTwice) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyLoggedMultipleTimes,
                                1);
}

// Tests that no DumpWithoutCrashing calls were made and the TaskResult
// companion metric is not logged when when TaskResult is not logged but
// multiple files were selected.
TEST_F(CloudOpenMetricsTest, MultipleFilesSelected) {
  { CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive, 2); }
  histogram_.ExpectTotalCount(kGoogleDriveTaskResultMetricStateMetricName, 0);
  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the TransferRequired, UploadResult and OpenErrors companion
// metrics are set correctly when TaskResult is logged as kFallbackQuickOffice
// and they are logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTaskResultIsFallbackQuickOffice) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFallbackQuickOffice);
    cloud_open_metrics.LogGoogleDriveOpenError(OfficeDriveOpenErrors::kOffline);
  }
  histogram_.ExpectUniqueSample(kDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the TransferRequired, UploadResult and OpenErrors companion
// metrics are set correctly when TaskResult is logged as kFallbackQuickOffice
// and they are logged inconsistently.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenTaskResultIsFallbackQuickOffice) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFallbackQuickOffice);
    cloud_open_metrics.LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    cloud_open_metrics.LogUploadResult(OfficeFilesUploadResult::kCloudError);
    cloud_open_metrics.LogGoogleDriveOpenError(OfficeDriveOpenErrors::kSuccess);
  }
  histogram_.ExpectUniqueSample(kDriveTransferRequiredMetricStateMetric,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the SourceVolume, TransferRequired, UploadResult and OpenErrors
// companion metrics are set correctly when TaskResult is logged as
// kCancelledAtConfirmation and they are logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTaskResultIsCancelledAtConfirmation) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(
        OfficeTaskResult::kCancelledAtConfirmation);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kDownloadsDirectory);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kMove);
  }
  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
}

// Tests that the SourceVolume, TransferRequired, UploadResult and OpenErrors
// companion metrics are set correctly when TaskResult is logged as
// kCancelledAtConfirmation and they are logged inconsistently.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenTaskResultIsCancelledAtConfirmation) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(
        OfficeTaskResult::kCancelledAtConfirmation);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kCloudAccessDenied);
    cloud_open_metrics.LogOneDriveOpenError(
        OfficeOneDriveOpenErrors::kGetActionsAccessDenied);
  }
  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kIncorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kIncorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the OpenErrors and UploadResult companion metrics are set
// correctly when TaskResult is logged as kFailedToUpload and they are logged
// consistently.
TEST_F(CloudOpenMetricsTest, MetricsConsistentWhenTaskResultIsFailedToUpload) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToUpload);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kCloudAccessDenied);
  }

  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the OpenErrors and UploadResult companion metrics are set
// correctly when TaskResult is logged as kFailedToUpload and they are logged
// inconsistently.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenTaskResultIsFailedToUpload) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToUpload);

    // These are incorrect - no OpenError expected, UploadResult should be an
    // error.
    cloud_open_metrics.LogOneDriveOpenError(
        OfficeOneDriveOpenErrors::kInvalidFileSystemURL);
    cloud_open_metrics.LogUploadResult(OfficeFilesUploadResult::kSuccess);
  }

  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the OpenErrors companion metric is set correctly when TaskResult
// is logged as kFailedToOpen and it is logged consistently when opening in
// OneDrive.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTaskResultIsFailedToOpenInOneDrive) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogOneDriveOpenError(
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the OpenErrors companion metric is set correctly when TaskResult
// is logged as kFailedToOpen and it is logged inconsistently when opening in
// OneDrive.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenTaskResultIsFailedToOpenInOneDrive) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogOneDriveOpenError(OfficeOneDriveOpenErrors::kSuccess);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the OpenErrors companion metric is set correctly when TaskResult
// is logged as kFailedToOpen and it is logged consistently when opening in
// Drive.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTaskResultIsFailedToOpenInDrive) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogGoogleDriveOpenError(
        OfficeDriveOpenErrors::kWaitingForUpload);
  }
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the OpenErrors companion metric is set correctly when TaskResult
// is logged as kFailedToOpen and it is logged inconsistently when opening in
// Drive.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenTaskResultIsFailedToOpenInDrive) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogGoogleDriveOpenError(OfficeDriveOpenErrors::kSuccess);
  }
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the OpenErrors, UploadResult and TransferRequired companion
// metrics are set correctly when TaskResult is logged as kOpened and they are
// logged consistently.
TEST_F(CloudOpenMetricsTest, MetricsConsistentWhenTaskResultIsOpened) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
    cloud_open_metrics.LogOneDriveOpenError(OfficeOneDriveOpenErrors::kSuccess);
    cloud_open_metrics.LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the OpenErrors, UploadResult and TransferRequired companion
// metrics are set correctly when TaskResult is logged as kOpened and they are
// logged inconsistently.
TEST_F(CloudOpenMetricsTest, MetricsInconsistentWhenTaskResultIsOpened) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
    cloud_open_metrics.LogUploadResult(OfficeFilesUploadResult::kSuccess);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kCopy);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the OpenErrors, UploadResult and TransferRequired companion
// metrics are set correctly when TaskResult is logged as kMoved and they are
// logged consistently.
TEST_F(CloudOpenMetricsTest, MetricsConsistentWhenTaskResultIsMoved) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kMoved);
    cloud_open_metrics.LogGoogleDriveOpenError(OfficeDriveOpenErrors::kSuccess);
    cloud_open_metrics.LogUploadResult(OfficeFilesUploadResult::kSuccess);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kMove);
  }
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the OpenErrors, UploadResult and TransferRequired companion
// metrics are set correctly when TaskResult is logged as kMoved and they are
// logged inconsistently.
TEST_F(CloudOpenMetricsTest, MetricsInconsistentWhenTaskResultIsMoved) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kMoved);
    cloud_open_metrics.LogGoogleDriveOpenError(
        OfficeDriveOpenErrors::kNoMetadata);
    cloud_open_metrics.LogUploadResult(OfficeFilesUploadResult::kInvalidURL);
  }
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  histogram_.ExpectUniqueSample(kDriveTransferRequiredMetricStateMetric,
                                MetricState::kIncorrectlyNotLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the UploadResult, OpenErrors and SourceVolume companion metrics
// are set correctly when TransferRequired is logged as kNotRequired and they
// are logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTransferRequiredIsNotRequired) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    cloud_open_metrics.LogGoogleDriveOpenError(
        OfficeDriveOpenErrors::kNoMetadata);
    cloud_open_metrics.LogSourceVolume(OfficeFilesSourceVolume::kGoogleDrive);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the UploadResult, OpenErrors and SourceVolume companion metrics
// are set correctly when TransferRequired is logged as kNotRequired and they
// are logged inconsistently.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenTransferRequiredIsNotRequired) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kDestinationUrlError);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kDownloadsDirectory);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the UploadResult and SourceVolume companion metrics are set
// correctly when TransferRequired is logged as kCopy and TaskResult is not
// kCancelledAtConfirmation and they are logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTransferRequiredIsCopyAndTaskResultIsFailedToOpen) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kCopy);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kCloudQuotaFull);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kMicrosoftOneDrive);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the UploadResult and SourceVolume companion metrics are set
// correctly when TransferRequired is logged as kCopy and TaskResult is not
// kCancelledAtConfirmation and they are logged inconsistently.
TEST_F(
    CloudOpenMetricsTest,
    MetricsInconsistentWhenTransferRequiredIsCopyAndTaskResultIsFailedToOpen) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kCopy);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogSourceVolume(OfficeFilesSourceVolume::kGoogleDrive);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the TransferRequired companion metric is set correctly when
// TaskResult is kFileAlreadyBeingOpened and it is logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTaskResultIsFileAlreadyBeingOpened) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFileAlreadyBeingOpened);
  }
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
}

// Tests that the TransferRequired companion metric is set incorrectly when
// TaskResult is kFileAlreadyBeingOpened and it is logged consistently.
TEST_F(
    CloudOpenMetricsTest,
    MetricsInconsistentWhenTaskResultIsFileAlreadyBeingOpenedAndTransferRequiredLogged) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    // No TransferRequired should be logged.
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kCopy);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFileAlreadyBeingOpened);
  }
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kIncorrectlyLogged, 1);
}

// Tests that the UploadResult companion metric is set correctly when
// TransferRequired is logged as kCopy and no TaskResult is logged and it is
// logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTransferRequiredIsCopyAndNoTaskResult) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kCopy);
    cloud_open_metrics.LogUploadResult(OfficeFilesUploadResult::kCloudError);
  }
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the UploadResult companion metric is set correctly when
// TransferRequired is logged as kCopy and no TaskResult is logged and it is
// logged inconsistently.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenTransferRequiredIsCopyAndNoTaskResult) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kCopy);
  }
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
}

// Tests that the CopyError companion metric is set correctly when UploadResult
// is logged as kCopyOperationError and CopyError is logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenUploadResultIsCopyOperationError) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kCopyOperationError);
    cloud_open_metrics.LogCopyError(
        base::File::Error::FILE_ERROR_ACCESS_DENIED);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the CopyError companion metric is set correctly when UploadResult
// is logged as kCopyOperationError and CopyError is logged inconsistently.
TEST_F(CloudOpenMetricsTest,
       MetricsInconsistentWhenUploadResultIsCopyOperationError) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kCopyOperationError);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveCopyErrorMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the UploadResult companion metric is set correctly when MoveError
// is logged and UploadResult is logged consistently.
TEST_F(CloudOpenMetricsTest, MetricsConsistentWhenMoveErrorIsLogged) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogMoveError(base::File::Error::FILE_ERROR_NO_SPACE);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kMoveOperationError);
  }
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the UploadResult companion metric is set correctly when MoveError
// is logged and UploadResult is logged inconsistently.
TEST_F(CloudOpenMetricsTest, MetricsInconsistentWhenMoveErrorIsLogged) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogMoveError(base::File::Error::FILE_ERROR_NO_SPACE);
  }
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that when all metrics are consistent for the cloud open flow, there is
// no dump without crashing.
TEST_F(CloudOpenMetricsTest, NoDumpWhenAllMetricsAreConsistentForOpenFlow) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kMicrosoftOneDrive);
    cloud_open_metrics.LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
    cloud_open_metrics.LogOneDriveOpenError(OfficeOneDriveOpenErrors::kSuccess);
  }
  histogram_.ExpectUniqueSample(kOneDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveMoveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);

  // No Google Drive metrics should be logged.
  histogram_.ExpectUniqueSample(kGoogleDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveMoveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);

  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the SourceVolume companion metric is set correctly when
// TransferRequired is logged as kNotRequired and the file is opened from
// Android Documents Provider with M365.
TEST_F(
    CloudOpenMetricsTest,
    NoDumpWhenAllMetricsAreConsistentForOpenFlowFromAndroidDocumentsProvider) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider);
    cloud_open_metrics.LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
    cloud_open_metrics.LogOneDriveOpenError(OfficeOneDriveOpenErrors::kSuccess);
  }
  histogram_.ExpectUniqueSample(kOneDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveMoveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);

  // No Google Drive metrics should be logged.
  histogram_.ExpectUniqueSample(kGoogleDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveMoveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);

  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the SourceVolume and TransferRequired companion metric are set
// correctly when the file is opened from Android Documents Provider with M365,
// and that the OpenError kAndroidOneDriveUnsupportedLocation is correct, when
// the TaskResult is logged as kOkAtFallbackAfterOpen.
TEST_F(CloudOpenMetricsTest,
       MetricsAreConsistentForAndroidDocumentsProviderUnsupportedLocation) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOkAtFallbackAfterOpen);
    cloud_open_metrics.LogOneDriveOpenError(
        OfficeOneDriveOpenErrors::kAndroidOneDriveUnsupportedLocation);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kAndroidOneDriveDocumentsProvider);
    cloud_open_metrics.LogTransferRequired(
        OfficeFilesTransferRequired::kNotRequired);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the kAndroidOneDriveUnsupportedLocation OpenError is incorrect
// when the TaskResult is a "pre open attempt" fallback result.
TEST_F(CloudOpenMetricsTest, MetricsAreInconsistentForFallbackBeforeOpen) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOkAtFallback);
    cloud_open_metrics.LogOneDriveOpenError(
        OfficeOneDriveOpenErrors::kAndroidOneDriveUnsupportedLocation);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that when all metrics are consistent for the cloud upload flow, there
// is no dump without crashing.
TEST_F(CloudOpenMetricsTest, NoDumpWhenAllMetricsAreConsistentForMoveFlow) {
  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kMicrosoftOneDrive);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kMove);
    cloud_open_metrics.LogUploadResult(OfficeFilesUploadResult::kSuccess);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kMoved);
    cloud_open_metrics.LogGoogleDriveOpenError(OfficeDriveOpenErrors::kSuccess);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveMoveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kGoogleDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);

  // No OneDrive metrics should be logged.
  histogram_.ExpectUniqueSample(kOneDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveMoveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);

  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the right companion metrics are logged when the cloud provider is
// updated and there is no TaskResult.
TEST_F(CloudOpenMetricsTest, set_cloud_provider_NoTaskResult) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.set_cloud_provider(CloudProvider::kOneDrive);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the right metrics and companion metrics are logged when the cloud
// provider is updated before the TaskResult is logged.
TEST_F(CloudOpenMetricsTest, set_cloud_provider_TaskResultLoggedAfter) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.set_cloud_provider(CloudProvider::kOneDrive);

    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kMoved);
    histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricName,
                                  OfficeTaskResult::kMoved, 1);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the right metrics and companion metrics are logged when the cloud
// provider is updated after TaskResult is logged.
TEST_F(CloudOpenMetricsTest, set_cloud_provider_TaskResultLoggedBefore) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kMoved);
    histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricName,
                                  OfficeTaskResult::kMoved, 1);

    cloud_open_metrics.set_cloud_provider(CloudProvider::kOneDrive);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the right metrics and companion metrics are logged when the cloud
// provider is updated between two TaskResult logs.
TEST_F(CloudOpenMetricsTest,
       set_cloud_provider_TaskResultLoggedBeforeAndAfter) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kMoved);
    histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricName,
                                  OfficeTaskResult::kMoved, 1);

    cloud_open_metrics.set_cloud_provider(CloudProvider::kOneDrive);

    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kMoved);
    histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricName,
                                  OfficeTaskResult::kMoved, 1);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the companion metrics are set correctly when UploadResult is
// logged as kUploadNotStartedReauthenticationRequired and they are logged
// consistently.
TEST_F(
    CloudOpenMetricsTest,
    MetricsConsistentWhenUploadResultIsUploadNotStartedReauthenticationRequired) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kDownloadsDirectory);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kMove);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToUpload);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kUploadNotStartedReauthenticationRequired);
  }

  histogram_.ExpectUniqueSample(kOneDriveCopyErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveMoveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);

  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);

  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the companion metrics are set correctly when UploadResult is
// logged as kUploadNotStartedReauthenticationRequired and they are logged
// inconsistently.
TEST_F(
    CloudOpenMetricsTest,
    MetricsInconsistentWhenUploadResultIsUploadNotStartedReauthenticationRequired) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive,
                                        /*file_count=*/1);
    cloud_open_metrics.LogSourceVolume(
        OfficeFilesSourceVolume::kDownloadsDirectory);
    cloud_open_metrics.LogTransferRequired(OfficeFilesTransferRequired::kMove);
    cloud_open_metrics.LogUploadResult(
        OfficeFilesUploadResult::kUploadNotStartedReauthenticationRequired);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToUpload);

    // These are incorrect - no copy or move error expected.
    cloud_open_metrics.LogMoveError(
        base::File::Error::FILE_ERROR_ACCESS_DENIED);
    cloud_open_metrics.LogCopyError(
        base::File::Error::FILE_ERROR_ACCESS_DENIED);
  }

  histogram_.ExpectUniqueSample(kOneDriveCopyErrorMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveMoveErrorMetricStateMetricName,
                                MetricState::kIncorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);

  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyNotLogged, 1);

  histogram_.ExpectUniqueSample(kOneDriveOpenSourceVolumeMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);
  histogram_.ExpectUniqueSample(kOneDriveTransferRequiredMetricStateMetric,
                                MetricState::kCorrectlyLogged, 1);

  histogram_.ExpectUniqueSample(kOneDriveUploadResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);

  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

}  // namespace ash::cloud_upload

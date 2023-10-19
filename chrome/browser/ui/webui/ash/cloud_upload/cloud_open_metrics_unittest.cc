// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

#include "base/debug/dump_without_crashing.h"
#include "base/test/metrics/histogram_tester.h"
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

  Metric<TestEnum> metric_ = Metric<TestEnum>("metric_name");
  base::HistogramTester histogram_;
};

// Tests that Metric::Log() updates the `value` and `state` correctly.
TEST_F(MetricTest, Log) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);
  ASSERT_FALSE(metric_.logged());

  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.value, TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
  ASSERT_TRUE(metric_.logged());

  histogram_.ExpectUniqueSample("metric_name", TestEnum::kOne, 1);

  metric_.Log(TestEnum::kZero);
  ASSERT_EQ(metric_.value, TestEnum::kZero);
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLoggedMultipleTimes);
  ASSERT_TRUE(metric_.logged());

  histogram_.ExpectBucketCount("metric_name", TestEnum::kZero, 1);
}

// Tests that Metric::MakeInconsistentIfLogged() doesn't update the `state` when
// the metric was not logged.
TEST_F(MetricTest, MakeInconsistentIfLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.MakeInconsistentIfLogged();
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);
}

// Tests that Metric::MakeInconsistentIfLogged() updates the `state` correctly
// when the metric was logged.
TEST_F(MetricTest, MakeInconsistentIfLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfLogged();
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyLogged);
}

// Tests that Metric::MakeInconsistentIfLoggedWith() doesn't update the `state`
// when logged with the correct value.
TEST_F(MetricTest, MakeInconsistentIfLoggedWithWhenLoggedWithWrongValue) {
  metric_.Log(TestEnum::kTwo);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests that Metric::MakeInconsistentIfLoggedWith() updates the `state` when
// logged with the incorrect value.
TEST_F(MetricTest, MakeInconsistentIfLoggedWithWhenLoggedWithIncorrectValue) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kWrongValueLogged);
}

// Tests Metric::MakeInconsistentIfLoggedWith() updates the `state` correctly
// when not logged.
TEST_F(MetricTest, MakeInconsistentIfLoggedWithWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.MakeInconsistentIfLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

// Tests that Metric::MakeInconsistentIfNotLogged() doesn't update the `state`
// when the metric was logged.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWhenLogged) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfNotLogged();
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests that Metric::MakeInconsistentIfNotLogged() updates the `state`
// correctly when the metric was not logged.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.MakeInconsistentIfNotLogged();
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

// Tests that Metric::MakeInconsistentIfNotLoggedWith() doesn't update the
// `state` when logged with the correct value.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWithWhenLoggedWithCorrectValue) {
  metric_.Log(TestEnum::kOne);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfNotLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);
}

// Tests Metric::MakeInconsistentIfNotLoggedWith() updates the `state` correctly
// when not logged.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWithWhenNotLogged) {
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyNotLogged);

  metric_.MakeInconsistentIfNotLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kIncorrectlyNotLogged);
}

// Tests that Metric::MakeInconsistentIfNotLoggedWith() updates the `state`
// correctly when logged with the wrong value.
TEST_F(MetricTest, MakeInconsistentIfNotLoggedWithWhenLoggedWithWrongValue) {
  metric_.Log(TestEnum::kTwo);
  ASSERT_EQ(metric_.state, MetricState::kCorrectlyLogged);

  metric_.MakeInconsistentIfNotLoggedWith({TestEnum::kZero, TestEnum::kOne});
  ASSERT_EQ(metric_.state, MetricState::kWrongValueLogged);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the TaskResult companion metric is set correctly when TaskResult
// is not logged.
TEST_F(CloudOpenMetricsTest, TaskResultNotLogged) {
  { CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive); }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyNotLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the TaskResult companion metric is set correctly when TaskResult
// is logged twice.
TEST_F(CloudOpenMetricsTest, TaskResultLoggedTwice) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kOpened);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
  }
  histogram_.ExpectUniqueSample(kGoogleDriveTaskResultMetricStateMetricName,
                                MetricState::kIncorrectlyLoggedMultipleTimes,
                                1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the TransferRequired, UploadResult and OpenErrors companion
// metrics are set correctly when TaskResult is logged as kFallbackQuickOffice
// and they are logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenTaskResultIsFallbackQuickOffice) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
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

// Tests that the SourceVolume companion metric is set correctly when TaskResult
// is logged as kFailedToOpen and it is logged consistently.
TEST_F(CloudOpenMetricsTest, MetricsConsistentWhenTaskResultIsFailedToOpen) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogOneDriveOpenError(
        OfficeOneDriveOpenErrors::kConversionToODFSUrlError);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kCorrectlyLogged, 1);
}

// Tests that the SourceVolume companion metric is set correctly when TaskResult
// is logged as kFailedToOpen and it is logged inconsistently.
TEST_F(CloudOpenMetricsTest, MetricsInconsistentWhenTaskResultIsFailedToOpen) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
    cloud_open_metrics.LogTaskResult(OfficeTaskResult::kFailedToOpen);
    cloud_open_metrics.LogOneDriveOpenError(OfficeOneDriveOpenErrors::kSuccess);
  }
  histogram_.ExpectUniqueSample(kOneDriveErrorMetricStateMetricName,
                                MetricState::kWrongValueLogged, 1);
  ASSERT_EQ(1, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that the OpenErrors, UploadResult and TransferRequired companion
// metrics are set correctly when TaskResult is logged as kOpened and they are
// logged consistently.
TEST_F(CloudOpenMetricsTest, MetricsConsistentWhenTaskResultIsOpened) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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

// Tests that the CopyError companion metric is set correctly when UploadResult
// is logged as kCopyOperationError and CopyError is logged consistently.
TEST_F(CloudOpenMetricsTest,
       MetricsConsistentWhenUploadResultIsCopyOperationError) {
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
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
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kOneDrive);
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
  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

// Tests that when all metrics are consistent for the cloud upload flow, there
// is no dump without crashing.
TEST_F(CloudOpenMetricsTest, NoDumpWhenAllMetricsAreConsistentForMoveFlow) {
  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
  {
    CloudOpenMetrics cloud_open_metrics(CloudProvider::kGoogleDrive);
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
  ASSERT_EQ(0, CloudOpenMetricsTest::number_of_dump_calls());
}

}  // namespace ash::cloud_upload

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestMetricName[] = "Test.";
const char kComposeboxFileDeleted[] =
    "Test.Composebox.Session.File.DeletedCount";
const char kComposeboxSessionDurationTotal[] =
    "Test.Composebox.Session.Duration.Total";
const char kComposeboxSessionAbandonedDuration[] =
    "Test.Composebox.Session.Duration.Abandoned";
const char kComposeboxSessionDurationQuerySubmitted[] =
    "Test.Composebox.Session.Duration.QuerySubmitted";
const char kComposeboxQuerySubmissionTime[] =
    "Test.Composebox.Query.Time.ToSubmission";
const char kComposeboxFileUploadAttemptPdf[] =
    "Test.Composebox.Session.File.Browser.UploadAttemptCount.Pdf";
const char kComposeboxFileUploadSuccessPdf[] =
    "Test.Composebox.Session.File.Browser.UploadSuccessCount.Pdf";
const char kComposeboxFileUploadServerErrorPdf[] =
    "Test.Composebox.Session.File.Browser.UploadFailureCount.Pdf";
const char kComposeboxFileValidationBrowserErrorForPdf[] =
    "Test.Composebox.Session.File.Browser.ValidationFailureCount.Pdf."
    "BrowserProcessingError";
const char kComposeboxFileUploadAttempt[] =
    "Test.Composebox.Session.File.Browser.UploadAttemptCount.";
const char kComposeboxFileUploadSuccess[] =
    "Test.Composebox.Session.File.Browser.UploadSuccessCount.";
const char kComposeboxFileUploadFailure[] =
    "Test.Composebox.Session.File.Browser.UploadFailureCount.";
const char kComposeboxFileValidationErrorTypes[] =
    "Test.Composebox.Session.File.Browser.ValidationFailureCount.";
const char kComposeboxQueryTextLength[] = "Test.Composebox.Query.TextLength";
const char kComposeboxQueryFileCount[] = "Test.Composebox.Query.FileCount";
const char kComposeboxQueryModality[] = "Test.Composebox.Query.Modality";
const char kComposeboxQueryCount[] = "Test.Composebox.Session.QueryCount";
const char kComposeboxFileSizePdf[] = "Test.Composebox.File.Size.Pdf";

std::string UploadStatusToString(FileUploadStatus status) {
  switch (status) {
    case FileUploadStatus::kNotUploaded:
      return "NotUploaded";
    case FileUploadStatus::kProcessing:
      return "Processing";
    case FileUploadStatus::kValidationFailed:
      return "ValidationFailed";
    case FileUploadStatus::kUploadStarted:
      return "UploadStarted";
    case FileUploadStatus::kUploadSuccessful:
      return "UploadSuccessful";
    case FileUploadStatus::kUploadFailed:
      return "UploadFailed";
    default:
      return "Unknown";
  }
}
}  // namespace

class ComposeboxMetricsRecorderTest : public testing::Test {
 public:
  ComposeboxMetricsRecorderTest() = default;
  ~ComposeboxMetricsRecorderTest() override = default;

  void SetUp() override {
    metrics_recorder_ =
        std::make_unique<ComposeboxMetricsRecorder>(kTestMetricName);
  }

  ComposeboxMetricsRecorder& metrics() { return *metrics_recorder_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void DestructMetricsRecorder() { metrics_recorder_.reset(); }

 private:
  std::unique_ptr<ComposeboxMetricsRecorder> metrics_recorder_;
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ComposeboxMetricsRecorderTest, SessionAbandoned) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(60));
  metrics().NotifySessionStateChanged(SessionState::kSessionAbandoned);

  histogram_tester().ExpectTotalCount(kComposeboxSessionAbandonedDuration, 1);
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationTotal, 1);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionAbandonedDuration,
                                            base::Seconds(60), 1);
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationTotal,
                                            base::Seconds(60), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, SessionCompleted) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(10));
  metrics().NotifySessionStateChanged(SessionState::kQuerySubmitted);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  DestructMetricsRecorder();
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationQuerySubmitted,
                                      1);
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationTotal, 1);
  histogram_tester().ExpectTotalCount(kComposeboxQuerySubmissionTime, 1);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(
      kComposeboxSessionDurationQuerySubmitted, base::Seconds(10), 1);
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationTotal,
                                            base::Seconds(10), 1);
  // Check query submission time.
  histogram_tester().ExpectUniqueTimeSample(kComposeboxQuerySubmissionTime,
                                            base::Seconds(10), 1);
}

TEST_F(ComposeboxMetricsRecorderTest, MultiQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));
  metrics().NotifySessionStateChanged(SessionState::kQuerySubmitted);
  metrics().RecordQueryMetrics(/*text_length=*/100, /*file_count=*/1);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  // Mimic the session remaining open when the AIM page is opened in another
  // tab/window. In this case more queries can be submitted.
  task_environment().FastForwardBy(base::Seconds(60));
  metrics().NotifySessionStateChanged(SessionState::kQuerySubmitted);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  metrics().NotifySessionStateChanged(SessionState::kSessionAbandoned);
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationQuerySubmitted,
                                      1);
  histogram_tester().ExpectTotalCount(kComposeboxSessionDurationTotal, 1);
  histogram_tester().ExpectTotalCount(kComposeboxQuerySubmissionTime, 2);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(
      kComposeboxSessionDurationQuerySubmitted, base::Seconds(90), 1);
  histogram_tester().ExpectUniqueTimeSample(kComposeboxSessionDurationTotal,
                                            base::Seconds(90), 1);
  // Check query submission times.
  histogram_tester().ExpectTimeBucketCount(kComposeboxQuerySubmissionTime,
                                           base::Seconds(30), 1);
  histogram_tester().ExpectTimeBucketCount(kComposeboxQuerySubmissionTime,
                                           base::Seconds(90), 1);
  histogram_tester().ExpectBucketCount(kComposeboxQueryFileCount, 1, 1);
  histogram_tester().ExpectBucketCount(kComposeboxQueryCount, 2, 1);
}

TEST_F(ComposeboxMetricsRecorderTest, TextOnlyQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  int text_length = 1000;
  int file_count = 0;
  metrics().RecordQueryMetrics(text_length, file_count);

  histogram_tester().ExpectBucketCount(kComposeboxQueryTextLength, text_length,
                                       1);
  histogram_tester().ExpectBucketCount(
      kComposeboxQueryModality, NtpComposeboxMultimodalState::kTextOnly, 1);
  histogram_tester().ExpectBucketCount(kComposeboxQueryFileCount, file_count,
                                       1);
}

TEST_F(ComposeboxMetricsRecorderTest, FileOnlyQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  int text_length = 0;
  int file_count = 2;
  metrics().RecordQueryMetrics(text_length, file_count);

  histogram_tester().ExpectBucketCount(kComposeboxQueryTextLength, text_length,
                                       1);
  histogram_tester().ExpectBucketCount(
      kComposeboxQueryModality, NtpComposeboxMultimodalState::kFileOnly, 1);
  histogram_tester().ExpectBucketCount(kComposeboxQueryFileCount, file_count,
                                       1);
}

TEST_F(ComposeboxMetricsRecorderTest, MultimodalQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  int text_length = 1000;
  int file_count = 1;
  metrics().RecordQueryMetrics(text_length, file_count);

  histogram_tester().ExpectBucketCount(kComposeboxQueryTextLength, text_length,
                                       1);
  histogram_tester().ExpectBucketCount(
      kComposeboxQueryModality, NtpComposeboxMultimodalState::kTextAndFile, 1);
  histogram_tester().ExpectBucketCount(kComposeboxQueryFileCount, file_count,
                                       1);
}

TEST_F(ComposeboxMetricsRecorderTest, FileUploadSuccess) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));
  // Simulate file upload.
  lens::MimeType file_mime_type = lens::MimeType::kPdf;
  FileUploadStatus upload_status = FileUploadStatus::kProcessing;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);
  // Finally simulate upload success.
  upload_status = FileUploadStatus::kUploadSuccessful;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);

  DestructMetricsRecorder();
  histogram_tester().ExpectTotalCount(kComposeboxFileUploadAttemptPdf, 1);
  histogram_tester().ExpectTotalCount(kComposeboxFileUploadSuccessPdf, 1);
}

TEST_F(ComposeboxMetricsRecorderTest, FileUploadError) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));
  // Simulate file upload.
  lens::MimeType file_mime_type = lens::MimeType::kPdf;
  FileUploadStatus upload_status = FileUploadStatus::kProcessing;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);
  // Next simulate file upload failure.
  upload_status = FileUploadStatus::kUploadFailed;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      FileUploadErrorType::kServerError);

  DestructMetricsRecorder();
  histogram_tester().ExpectTotalCount(kComposeboxFileUploadAttemptPdf, 1);
  histogram_tester().ExpectTotalCount(kComposeboxFileUploadServerErrorPdf, 1);
}

TEST_F(ComposeboxMetricsRecorderTest, FileValidationError) {
  // Setup user flow.
  FileUploadErrorType error = FileUploadErrorType::kBrowserProcessingError;
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));
  // Simulate file validation error.
  lens::MimeType file_mime_type = lens::MimeType::kPdf;
  uint64_t file_size = 1000000;
  metrics().RecordFileSizeMetric(file_mime_type, file_size);
  FileUploadStatus upload_status = FileUploadStatus::kProcessing;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);
  // Next simulate file validation error.
  upload_status = FileUploadStatus::kValidationFailed;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status, error);

  // Simulate another file validation error.
  upload_status = FileUploadStatus::kProcessing;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);
  upload_status = FileUploadStatus::kValidationFailed;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status, error);

  DestructMetricsRecorder();
  histogram_tester().ExpectBucketCount(kComposeboxFileUploadAttemptPdf, 2, 1);
  histogram_tester().ExpectBucketCount(
      kComposeboxFileValidationBrowserErrorForPdf, 2, 1);
  histogram_tester().ExpectBucketCount(kComposeboxFileSizePdf, file_size, 1);
}

TEST_F(ComposeboxMetricsRecorderTest, MultiFileUpload) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));
  // Simulate unsuccessful file upload.
  lens::MimeType file_mime_type = lens::MimeType::kPdf;
  FileUploadStatus upload_status = FileUploadStatus::kProcessing;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);
  upload_status = FileUploadStatus::kUploadFailed;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      FileUploadErrorType::kServerError);

  // Simulate successful file upload.
  upload_status = FileUploadStatus::kProcessing;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);
  upload_status = FileUploadStatus::kUploadSuccessful;
  metrics().OnFileUploadStatusChanged(file_mime_type, upload_status,
                                      std::nullopt);

  DestructMetricsRecorder();
  histogram_tester().ExpectBucketCount(kComposeboxFileUploadAttemptPdf, 2, 1);
  histogram_tester().ExpectTotalCount(kComposeboxFileUploadSuccessPdf, 1);
  histogram_tester().ExpectTotalCount(kComposeboxFileUploadServerErrorPdf, 1);
}

class MetricsRecorderFileTest
    : public ComposeboxMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<FileUploadStatus, lens::MimeType>> {
 public:
  void SetUp() override {
    ComposeboxMetricsRecorderTest::SetUp();
    metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
    metrics().OnFileUploadStatusChanged(
        mime_type_param(), FileUploadStatus::kProcessing, std::nullopt);
    mime_type_string_ = metrics().MimeTypeToString(mime_type_param());
  }
  void TestUploadSuccessMetrics() {
    histogram_tester().ExpectTotalCount(
        kComposeboxFileUploadAttempt + mime_type_string_, 1);
    histogram_tester().ExpectTotalCount(
        kComposeboxFileUploadSuccess + mime_type_string_, 1);
  }
  void TestUploadFailureMetrics() {
    histogram_tester().ExpectTotalCount(
        kComposeboxFileUploadAttempt + mime_type_string_, 1);
    histogram_tester().ExpectTotalCount(
        kComposeboxFileUploadFailure + mime_type_string_, 1);
  }

 protected:
  FileUploadStatus status_param() const { return std::get<0>(GetParam()); }
  lens::MimeType mime_type_param() const { return std::get<1>(GetParam()); }

 private:
  std::string mime_type_string_;
};

TEST_P(MetricsRecorderFileTest, FileUploadStatusChanged) {
  metrics().OnFileUploadStatusChanged(mime_type_param(), status_param(),
                                      std::nullopt);
  DestructMetricsRecorder();
  switch (status_param()) {
    case FileUploadStatus::kUploadSuccessful:
      TestUploadSuccessMetrics();
      break;
    case FileUploadStatus::kUploadFailed:
      TestUploadFailureMetrics();
      break;
    default:
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MetricsRecorderFileTest,
    testing::Combine(testing::Values(FileUploadStatus::kUploadSuccessful,
                                     FileUploadStatus::kUploadFailed),
                     testing::Values(lens::MimeType::kPdf,
                                     lens::MimeType::kImage,
                                     lens::MimeType::kUnknown)));

class MetricsRecorderFileValidationTest
    : public ComposeboxMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<FileUploadErrorType, lens::MimeType>> {
 public:
  void SetUp() override {
    ComposeboxMetricsRecorderTest::SetUp();
    metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
    metrics().OnFileUploadStatusChanged(
        mime_type_param(), FileUploadStatus::kProcessing, std::nullopt);
    mime_type_string_ = metrics().MimeTypeToString(mime_type_param());
    error_type_string_ = metrics().FileErrorToString(error_param());
  }
  void TestValidationFailedMetrics() {
    histogram_tester().ExpectTotalCount(
        kComposeboxFileUploadAttempt + mime_type_string_, 1);
    histogram_tester().ExpectTotalCount(kComposeboxFileValidationErrorTypes +
                                            mime_type_string_ + "." +
                                            error_type_string_,
                                        1);
  }

 protected:
  FileUploadErrorType error_param() const { return std::get<0>(GetParam()); }
  lens::MimeType mime_type_param() const { return std::get<1>(GetParam()); }

 private:
  std::string mime_type_string_;
  std::string error_type_string_;
};

TEST_P(MetricsRecorderFileValidationTest, ValidationError) {
  metrics().OnFileUploadStatusChanged(
      mime_type_param(), FileUploadStatus::kValidationFailed, error_param());
  DestructMetricsRecorder();
  TestValidationFailedMetrics();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MetricsRecorderFileValidationTest,
    testing::Combine(
        testing::Values(FileUploadErrorType::kUnknown,
                        FileUploadErrorType::kBrowserProcessingError,
                        FileUploadErrorType::kNetworkError,
                        FileUploadErrorType::kServerError,
                        FileUploadErrorType::kServerSizeLimitExceeded,
                        FileUploadErrorType::kAborted,
                        FileUploadErrorType::kImageProcessingError),
        testing::Values(lens::MimeType::kPdf,
                        lens::MimeType::kImage,
                        lens::MimeType::kUnknown)));

class MetricsRecorderFileDeletionTest
    : public ComposeboxMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<lens::MimeType, FileUploadStatus>> {
 public:
  void SetUp() override {
    ComposeboxMetricsRecorderTest::SetUp();
    mime_type_string_ = metrics().MimeTypeToString(mime_type_param());
    status_string_ = UploadStatusToString(status_param());
  }

 protected:
  lens::MimeType mime_type_param() const { return std::get<0>(GetParam()); }
  FileUploadStatus status_param() const { return std::get<1>(GetParam()); }
  std::string mime_type_string() const { return mime_type_string_; }
  std::string status_string() const { return status_string_; }

 private:
  std::string mime_type_string_;
  std::string status_string_;
};

TEST_P(MetricsRecorderFileDeletionTest, FileDeleted) {
  std::string file_type = "." + mime_type_string();
  std::string file_status = "." + status_string();
  // Setup user flow.
  metrics().RecordFileDeletedMetrics(true, mime_type_param(), status_param());

  DestructMetricsRecorder();
  histogram_tester().ExpectTotalCount(
      kComposeboxFileDeleted + file_type + file_status, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MetricsRecorderFileDeletionTest,
    testing::Combine(testing::Values(lens::MimeType::kPdf,
                                     lens::MimeType::kImage,
                                     lens::MimeType::kUnknown),
                     testing::Values(FileUploadStatus::kNotUploaded,
                                     FileUploadStatus::kProcessing,
                                     FileUploadStatus::kValidationFailed,
                                     FileUploadStatus::kUploadStarted,
                                     FileUploadStatus::kUploadSuccessful,
                                     FileUploadStatus::kUploadFailed,
                                     FileUploadStatus::kUploadExpired)));

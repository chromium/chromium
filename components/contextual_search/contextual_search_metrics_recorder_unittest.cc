// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_search {

namespace {
const char kContextualSearchSourceUnknownSuffix[] = ".Unknown";
const char kContextualSearchFileDeleted[] =
    "ContextualSearch.Session.File.DeletedCount";
const char kContextualSearchSessionDurationTotal[] =
    "ContextualSearch.Session.Duration.Total.Unknown";
const char kContextualSearchSessionAbandonedDuration[] =
    "ContextualSearch.Session.Duration.Abandoned.Unknown";
const char kContextualSearchSessionDurationQuerySubmitted[] =
    "ContextualSearch.Session.Duration.QuerySubmitted.Unknown";
const char kContextualSearchQuerySubmissionTime[] =
    "ContextualSearch.Query.Time.ToSubmission.Unknown";
const char kContextualSearchFileUploadAttemptPdf[] =
    "ContextualSearch.Session.File.Browser.UploadAttemptCount.Pdf.Unknown";
const char kContextualSearchFileUploadSuccessPdf[] =
    "ContextualSearch.Session.File.Browser.UploadSuccessCount.Pdf.Unknown";
const char kContextualSearchFileUploadSuccessAll[] =
    "ContextualSearch.Session.File.Browser.UploadSuccessCount.Unknown";
const char kContextualSearchFileUploadServerErrorPdf[] =
    "ContextualSearch.Session.File.Browser.UploadFailureCount.Pdf.Unknown";
const char kContextualSearchFileValidationBrowserErrorForPdf[] =
    "ContextualSearch.Session.File.Browser.ValidationFailureCount.Pdf."
    "BrowserProcessingError.Unknown";
const char kContextualSearchFileValidationBrowserErrorAll[] =
    "ContextualSearch.Session.File.Browser.ValidationFailureCount."
    "BrowserProcessingError.Unknown";
const char kContextualSearchFileUploadAttemptAll[] =
    "ContextualSearch.Session.File.Browser.UploadAttemptCount.Unknown";
const char kContextualSearchFileUploadAttempt[] =
    "ContextualSearch.Session.File.Browser.UploadAttemptCount.";
const char kContextualSearchFileUploadSuccess[] =
    "ContextualSearch.Session.File.Browser.UploadSuccessCount.";
const char kContextualSearchFileUploadFailure[] =
    "ContextualSearch.Session.File.Browser.UploadFailureCount.";
const char kContextualSearchFileUploadFailureAll[] =
    "ContextualSearch.Session.File.Browser.UploadFailureCount.Unknown";
const char kContextualSearchFileValidationErrorTypes[] =
    "ContextualSearch.Session.File.Browser.ValidationFailureCount.";
const char kContextualSearchQueryTextLength[] =
    "ContextualSearch.Query.TextLength.Unknown";
const char kContextualSearchQueryFileCount[] =
    "ContextualSearch.Query.FileCount.Unknown";
const char kContextualSearchQueryModality[] =
    "ContextualSearch.Query.Modality.V2.Unknown";
const char kContextualSearchQueryCount[] =
    "ContextualSearch.Session.QueryCount.Unknown";
const char kContextualSearchFileSizePdf[] =
    "ContextualSearch.File.Size.Pdf.Unknown";
const char kContextualSearchFileSizeAll[] =
    "ContextualSearch.File.Size.Unknown";
const char kContextualSearchFileSizeImage[] =
    "ContextualSearch.File.Size.Image.Unknown";
const char kContextualSearchToolMode[] = "ContextualSearch.Tools.Unknown";
const char kContextualSearchModelMode[] = "ContextualSearch.Models.Unknown";
const char kContextualSearchToolModeOnSubmission[] =
    "ContextualSearch.Tools.ModeOnSubmission.Unknown";
const char kContextualSearchModelModeOnSubmission[] =
    "ContextualSearch.Models.ModeOnSubmission.Unknown";
const char kContextualSearchTabContextAdded[] =
    "ContextualSearch.TabContextAdded.V2.Unknown";
const char kContextualSearchTabContextAddedFromSuggestionChip[] =
    "ContextualSearch.TabContextAddedFromTabSuggestionChip.Unknown";
const char kContextualSearchTabContextAddedFromPlusButton[] =
    "ContextualSearch.TabContextAddedFromPlusButton.Unknown";
const char kContextualSearchTabWithDuplicateTitleClicked[] =
    "ContextualSearch.TabWithDuplicateTitleClicked.V2.Unknown";

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

class ContextualSearchMetricsRecorderTest : public testing::Test {
 public:
  ContextualSearchMetricsRecorderTest() = default;
  ~ContextualSearchMetricsRecorderTest() override = default;

  void SetUp() override { CreateMetricsRecorder(); }

  void CreateMetricsRecorder() {
    metrics_recorder_ = std::make_unique<ContextualSearchMetricsRecorder>(
        ContextualSearchSource::kUnknown);
  }

  ContextualSearchMetricsRecorder& metrics() { return *metrics_recorder_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  base::UserActionTester& user_action_tester() { return user_action_tester_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void DestructMetricsRecorder() { metrics_recorder_.reset(); }

 private:
  std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ContextualSearchMetricsRecorderTest, SubmitQueryWithoutContext) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/false,
                                 /*has_non_tab_context=*/false);

  EXPECT_EQ(
      user_action_tester().GetActionCount(
          "ContextualSearch.UserAction.SubmitQuery.WithoutContext.Unknown"),
      1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.UserAction.SubmitQuery.WithoutContext.Unknown", true,
      1);
}

TEST_F(ContextualSearchMetricsRecorderTest, SubmitQueryWithTabContext) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/true,
                                 /*has_non_tab_context=*/false);

  EXPECT_EQ(
      user_action_tester().GetActionCount(
          "ContextualSearch.UserAction.SubmitQuery.WithTabContext.Unknown"),
      1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.UserAction.SubmitQuery.WithTabContext.Unknown", true,
      1);
}

TEST_F(ContextualSearchMetricsRecorderTest, SubmitQueryWithNonTabContext) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/false,
                                 /*has_non_tab_context=*/true);

  EXPECT_EQ(user_action_tester().GetActionCount(
                "ContextualSearch.UserAction.SubmitQuery.WithNonTabContext."
                "Unknown"),
            1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.UserAction.SubmitQuery.WithNonTabContext.Unknown", true,
      1);
}

TEST_F(ContextualSearchMetricsRecorderTest, SubmitQueryWithBothContext) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/true,
                                 /*has_non_tab_context=*/true);

  // Tab context should take precedence.
  EXPECT_EQ(
      user_action_tester().GetActionCount(
          "ContextualSearch.UserAction.SubmitQuery.WithTabContext.Unknown"),
      1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.UserAction.SubmitQuery.WithTabContext.Unknown", true,
      1);
  EXPECT_EQ(user_action_tester().GetActionCount(
                "ContextualSearch.UserAction.SubmitQuery.WithNonTabContext."
                "Unknown"),
            0);
}

TEST_F(ContextualSearchMetricsRecorderTest, SessionAbandoned) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(60));
  metrics().NotifySessionStateChanged(SessionState::kSessionAbandoned);

  histogram_tester().ExpectTotalCount(kContextualSearchSessionAbandonedDuration,
                                      1);
  histogram_tester().ExpectTotalCount(kContextualSearchSessionDurationTotal, 1);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(
      kContextualSearchSessionAbandonedDuration, base::Seconds(60), 1);
  histogram_tester().ExpectUniqueTimeSample(
      kContextualSearchSessionDurationTotal, base::Seconds(60), 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, SessionCompleted) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(10));
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/false,
                                 /*has_non_tab_context=*/false);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  DestructMetricsRecorder();
  histogram_tester().ExpectTotalCount(
      kContextualSearchSessionDurationQuerySubmitted, 1);
  histogram_tester().ExpectTotalCount(kContextualSearchSessionDurationTotal, 1);
  histogram_tester().ExpectTotalCount(kContextualSearchQuerySubmissionTime, 1);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(
      kContextualSearchSessionDurationQuerySubmitted, base::Seconds(10), 1);
  histogram_tester().ExpectUniqueTimeSample(
      kContextualSearchSessionDurationTotal, base::Seconds(10), 1);
  // Check query submission time.
  histogram_tester().ExpectUniqueTimeSample(
      kContextualSearchQuerySubmissionTime, base::Seconds(10), 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, MultiQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/false,
                                 /*has_non_tab_context=*/false);
  metrics().RecordQueryMetrics(/*text_length=*/100, /*file_count=*/1);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  // Mimic the session remaining open when the AIM page is opened in another
  // tab/window. In this case more queries can be submitted.
  task_environment().FastForwardBy(base::Seconds(60));
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/false,
                                 /*has_non_tab_context=*/false);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  metrics().NotifySessionStateChanged(SessionState::kSessionAbandoned);
  histogram_tester().ExpectTotalCount(
      kContextualSearchSessionDurationQuerySubmitted, 1);
  histogram_tester().ExpectTotalCount(kContextualSearchSessionDurationTotal, 1);
  histogram_tester().ExpectTotalCount(kContextualSearchQuerySubmissionTime, 2);
  // Check session duration times.
  histogram_tester().ExpectUniqueTimeSample(
      kContextualSearchSessionDurationQuerySubmitted, base::Seconds(90), 1);
  histogram_tester().ExpectUniqueTimeSample(
      kContextualSearchSessionDurationTotal, base::Seconds(90), 1);
  // Check query submission times.
  histogram_tester().ExpectTimeBucketCount(kContextualSearchQuerySubmissionTime,
                                           base::Seconds(30), 1);
  histogram_tester().ExpectTimeBucketCount(kContextualSearchQuerySubmissionTime,
                                           base::Seconds(90), 1);
  histogram_tester().ExpectBucketCount(kContextualSearchQueryFileCount, 1, 1);
  histogram_tester().ExpectBucketCount(kContextualSearchQueryCount, 2, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, TextOnlyQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  int text_length = 1000;
  int file_count = 0;
  metrics().RecordQueryMetrics(text_length, file_count);

  histogram_tester().ExpectBucketCount(kContextualSearchQueryTextLength,
                                       text_length, 1);
  histogram_tester().ExpectBucketCount(
      kContextualSearchQueryModality,
      ContextualSearchMultimodalState::kTextOnly, 1);
  histogram_tester().ExpectBucketCount(kContextualSearchQueryFileCount,
                                       file_count, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, FileOnlyQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  int text_length = 0;
  int file_count = 2;
  metrics().RecordQueryMetrics(text_length, file_count);

  histogram_tester().ExpectBucketCount(kContextualSearchQueryTextLength,
                                       text_length, 1);
  histogram_tester().ExpectBucketCount(
      kContextualSearchQueryModality,
      ContextualSearchMultimodalState::kFileOnly, 1);
  histogram_tester().ExpectBucketCount(kContextualSearchQueryFileCount,
                                       file_count, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, MultimodalQuerySubmissionSession) {
  // Setup user flow.
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  int text_length = 1000;
  int file_count = 1;
  metrics().RecordQueryMetrics(text_length, file_count);

  histogram_tester().ExpectBucketCount(kContextualSearchQueryTextLength,
                                       text_length, 1);
  histogram_tester().ExpectBucketCount(
      kContextualSearchQueryModality,
      ContextualSearchMultimodalState::kTextAndFile, 1);
  histogram_tester().ExpectBucketCount(kContextualSearchQueryFileCount,
                                       file_count, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, ToolMode) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().RecordToolMode(composebox_query::mojom::ToolMode::kImageGen);
  DestructMetricsRecorder();
  histogram_tester().ExpectUniqueSample(
      kContextualSearchToolMode, composebox_query::mojom::ToolMode::kImageGen,
      1);
}

TEST_F(ContextualSearchMetricsRecorderTest, ModelMode) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().RecordModelMode(composebox_query::mojom::ModelMode::kGeminiPro);
  DestructMetricsRecorder();
  histogram_tester().ExpectUniqueSample(
      kContextualSearchModelMode,
      composebox_query::mojom::ModelMode::kGeminiPro, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, ModesOnSubmission) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().RecordModesOnSubmission(
      composebox_query::mojom::ToolMode::kImageGen,
      composebox_query::mojom::ModelMode::kGeminiPro);
  DestructMetricsRecorder();
  histogram_tester().ExpectUniqueSample(
      kContextualSearchToolModeOnSubmission,
      composebox_query::mojom::ToolMode::kImageGen, 1);
  histogram_tester().ExpectUniqueSample(
      kContextualSearchModelModeOnSubmission,
      composebox_query::mojom::ModelMode::kGeminiPro, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, TabContextAdded) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().RecordTabAddedMetrics(/*has_duplicate_title=*/false, std::nullopt,
                                  /*is_tab_suggestion_chip=*/false);
  metrics().RecordTabAddedMetrics(/*has_duplicate_title=*/true, std::nullopt,
                                  /*is_tab_suggestion_chip=*/false);
  DestructMetricsRecorder();

  histogram_tester().ExpectUniqueSample(kContextualSearchTabContextAdded, 2, 1);
  histogram_tester().ExpectUniqueSample(
      kContextualSearchTabContextAddedFromSuggestionChip, 0, 1);
  histogram_tester().ExpectUniqueSample(
      kContextualSearchTabContextAddedFromPlusButton, 2, 1);
  histogram_tester().ExpectUniqueSample(
      kContextualSearchTabWithDuplicateTitleClicked, 1, 1);

  histogram_tester().ExpectUniqueSample(
      kContextualSearchTabWithDuplicateTitleClicked, 1, 1);

  CreateMetricsRecorder();
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().RecordTabAddedMetrics(/*has_duplicate_title=*/false, std::nullopt,
                                  /*is_tab_suggestion_chip=*/true);
  DestructMetricsRecorder();
  histogram_tester().ExpectBucketCount(
      kContextualSearchTabContextAddedFromSuggestionChip, 1, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, FileUploadSuccess) {
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
  histogram_tester().ExpectTotalCount(kContextualSearchFileUploadAttemptPdf, 1);
  histogram_tester().ExpectTotalCount(kContextualSearchFileUploadSuccessPdf, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, FileUploadError) {
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
  histogram_tester().ExpectTotalCount(kContextualSearchFileUploadAttemptPdf, 1);
  histogram_tester().ExpectTotalCount(kContextualSearchFileUploadServerErrorPdf,
                                      1);
}

TEST_F(ContextualSearchMetricsRecorderTest, AggregatedUploadMetrics) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));

  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kPdf, FileUploadStatus::kProcessing, std::nullopt);
  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kPdf, FileUploadStatus::kUploadSuccessful, std::nullopt);

  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kImage, FileUploadStatus::kProcessing, std::nullopt);
  metrics().OnFileUploadStatusChanged(lens::MimeType::kImage,
                                      FileUploadStatus::kUploadFailed,
                                      FileUploadErrorType::kServerError);

  DestructMetricsRecorder();

  histogram_tester().ExpectUniqueSample(kContextualSearchFileUploadAttemptAll,
                                        2, 1);

  histogram_tester().ExpectUniqueSample(kContextualSearchFileUploadSuccessAll,
                                        1, 1);

  histogram_tester().ExpectUniqueSample(kContextualSearchFileUploadFailureAll,
                                        1, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, FileValidationError) {
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
  histogram_tester().ExpectBucketCount(kContextualSearchFileUploadAttemptPdf, 2,
                                       1);
  histogram_tester().ExpectBucketCount(
      kContextualSearchFileValidationBrowserErrorForPdf, 2, 1);
  histogram_tester().ExpectBucketCount(kContextualSearchFileSizePdf, file_size,
                                       1);
}

TEST_F(ContextualSearchMetricsRecorderTest, AggregatedFileValidationError) {
  FileUploadErrorType error = FileUploadErrorType::kBrowserProcessingError;
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  task_environment().FastForwardBy(base::Seconds(30));

  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kPdf, FileUploadStatus::kProcessing, std::nullopt);
  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kPdf, FileUploadStatus::kValidationFailed, error);

  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kImage, FileUploadStatus::kProcessing, std::nullopt);
  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kImage, FileUploadStatus::kValidationFailed, error);
  std::string error_string = metrics().FileErrorToString(error);
  DestructMetricsRecorder();

  histogram_tester().ExpectUniqueSample(kContextualSearchFileUploadAttemptAll,
                                        2, 1);
  histogram_tester().ExpectUniqueSample(
      kContextualSearchFileValidationBrowserErrorAll, 2, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, AggregatedFileSizeMetrics) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().RecordFileSizeMetric(lens::MimeType::kPdf, 100);

  metrics().RecordFileSizeMetric(lens::MimeType::kImage, 200);
  DestructMetricsRecorder();

  histogram_tester().ExpectUniqueSample(kContextualSearchFileSizePdf, 100, 1);
  histogram_tester().ExpectUniqueSample(kContextualSearchFileSizeImage, 200, 1);

  histogram_tester().ExpectTotalCount(kContextualSearchFileSizeAll, 2);
  histogram_tester().ExpectBucketCount(kContextualSearchFileSizeAll, 100, 1);
  histogram_tester().ExpectBucketCount(kContextualSearchFileSizeAll, 200, 1);
}

TEST_F(ContextualSearchMetricsRecorderTest, MultiFileUpload) {
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
  histogram_tester().ExpectBucketCount(kContextualSearchFileUploadAttemptPdf, 2,
                                       1);
  histogram_tester().ExpectTotalCount(kContextualSearchFileUploadSuccessPdf, 1);
  histogram_tester().ExpectTotalCount(kContextualSearchFileUploadServerErrorPdf,
                                      1);
}

class MetricsRecorderFileTest
    : public ContextualSearchMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<FileUploadStatus, lens::MimeType>> {
 public:
  void SetUp() override {
    ContextualSearchMetricsRecorderTest::SetUp();
    metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
    metrics().OnFileUploadStatusChanged(
        mime_type_param(), FileUploadStatus::kProcessing, std::nullopt);
    mime_type_string_ = metrics().MimeTypeToString(mime_type_param());
  }
  void TestUploadSuccessMetrics() {
    histogram_tester().ExpectTotalCount(
        kContextualSearchFileUploadAttempt + mime_type_string_ +
            kContextualSearchSourceUnknownSuffix,
        1);
    histogram_tester().ExpectTotalCount(
        kContextualSearchFileUploadSuccess + mime_type_string_ +
            kContextualSearchSourceUnknownSuffix,
        1);
  }
  void TestUploadFailureMetrics() {
    histogram_tester().ExpectTotalCount(
        kContextualSearchFileUploadAttempt + mime_type_string_ +
            kContextualSearchSourceUnknownSuffix,
        1);
    histogram_tester().ExpectTotalCount(
        kContextualSearchFileUploadFailure + mime_type_string_ +
            kContextualSearchSourceUnknownSuffix,
        1);
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
                                     lens::MimeType::kAnnotatedPageContent,
                                     lens::MimeType::kUnknown)));

class MetricsRecorderFileValidationTest
    : public ContextualSearchMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<FileUploadErrorType, lens::MimeType>> {
 public:
  void SetUp() override {
    ContextualSearchMetricsRecorderTest::SetUp();
    metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
    metrics().OnFileUploadStatusChanged(
        mime_type_param(), FileUploadStatus::kProcessing, std::nullopt);
    mime_type_string_ = metrics().MimeTypeToString(mime_type_param());
    error_type_string_ = metrics().FileErrorToString(error_param());
  }
  void TestValidationFailedMetrics() {
    histogram_tester().ExpectTotalCount(
        kContextualSearchFileUploadAttempt + mime_type_string_ +
            kContextualSearchSourceUnknownSuffix,
        1);
    histogram_tester().ExpectTotalCount(
        kContextualSearchFileValidationErrorTypes + mime_type_string_ + "." +
            error_type_string_ + kContextualSearchSourceUnknownSuffix,
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
                        lens::MimeType::kAnnotatedPageContent,
                        lens::MimeType::kUnknown)));

class MetricsRecorderFileDeletionTest
    : public ContextualSearchMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<lens::MimeType, FileUploadStatus>> {
 public:
  void SetUp() override {
    ContextualSearchMetricsRecorderTest::SetUp();
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
  histogram_tester().ExpectTotalCount(kContextualSearchFileDeleted + file_type +
                                          file_status +
                                          kContextualSearchSourceUnknownSuffix,
                                      1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MetricsRecorderFileDeletionTest,
    testing::Combine(testing::Values(lens::MimeType::kPdf,
                                     lens::MimeType::kImage,
                                     lens::MimeType::kAnnotatedPageContent,
                                     lens::MimeType::kUnknown),
                     testing::Values(FileUploadStatus::kNotUploaded,
                                     FileUploadStatus::kProcessing,
                                     FileUploadStatus::kValidationFailed,
                                     FileUploadStatus::kUploadStarted,
                                     FileUploadStatus::kUploadSuccessful,
                                     FileUploadStatus::kUploadFailed,
                                     FileUploadStatus::kUploadExpired,
                                     FileUploadStatus::kUploadReplaced)));

TEST_F(ContextualSearchMetricsRecorderTest, FunnelMetrics) {
  metrics().NotifySessionStateChanged(SessionState::kSessionStarted);
  metrics().ActivateMetricsFunnel("AiMode");
  metrics().ActivateMetricsFunnel("DeepSearch");

  // Simulate file uploads.
  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kPdf, FileUploadStatus::kProcessing, std::nullopt);
  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kPdf, FileUploadStatus::kUploadSuccessful, std::nullopt);
  metrics().OnFileUploadStatusChanged(
      lens::MimeType::kImage, FileUploadStatus::kProcessing, std::nullopt);
  metrics().OnFileUploadStatusChanged(lens::MimeType::kImage,
                                      FileUploadStatus::kUploadFailed,
                                      FileUploadErrorType::kServerError);

  // Simulate query submission.
  metrics().NotifyQuerySubmitted(/*has_tab_context=*/false,
                                 /*has_non_tab_context=*/false);
  metrics().RecordQueryMetrics(/*text_length=*/100, /*file_count=*/2);
  metrics().NotifySessionStateChanged(SessionState::kNavigationOccurred);

  DestructMetricsRecorder();

  // Check funnel-specific histograms.
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Query.TextLength.FunnelMetrics.AiMode.Unknown", 100,
      1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Query.TextLength.FunnelMetrics.DeepSearch.Unknown",
      100, 1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Query.FileCount.FunnelMetrics.AiMode.Unknown", 2, 1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Query.FileCount.FunnelMetrics.DeepSearch.Unknown", 2,
      1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Session.QueryCount.FunnelMetrics.AiMode.Unknown", 1,
      1);
  histogram_tester().ExpectBucketCount(
      "ContextualSearch.Session.QueryCount.FunnelMetrics.DeepSearch.Unknown",
      1, 1);

  // File upload metrics for AiMode funnel.
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadAttemptCount.FunnelMetrics."
      "AiMode.Unknown",
      2, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadAttemptCount.FunnelMetrics."
      "Pdf.AiMode.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadAttemptCount.FunnelMetrics."
      "Image.AiMode.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadSuccessCount.FunnelMetrics."
      "AiMode.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadSuccessCount.FunnelMetrics."
      "Pdf.AiMode.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadFailureCount.FunnelMetrics."
      "AiMode.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadFailureCount.FunnelMetrics."
      "Image.AiMode.Unknown",
      1, 1);

  // File upload metrics for DeepSearch funnel.
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadAttemptCount.FunnelMetrics."
      "DeepSearch.Unknown",
      2, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadAttemptCount.FunnelMetrics."
      "Pdf.DeepSearch.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadAttemptCount.FunnelMetrics."
      "Image.DeepSearch.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadSuccessCount.FunnelMetrics."
      "DeepSearch.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadSuccessCount.FunnelMetrics."
      "Pdf.DeepSearch.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadFailureCount.FunnelMetrics."
      "DeepSearch.Unknown",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "ContextualSearch.Session.File.Browser.UploadFailureCount.FunnelMetrics."
      "Image.DeepSearch.Unknown",
      1, 1);
}

}  // namespace contextual_search

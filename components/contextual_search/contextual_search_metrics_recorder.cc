// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "components/lens/lens_overlay_mime_type.h"

namespace contextual_search {

namespace {
const char kContextualSearchFileDeleted[] =
    "ContextualSearch.Session.File.DeletedCount";
const char kContextualSearchSessionDuration[] =
    "ContextualSearch.Session.Duration.Total";
const char kContextualSearchSessionDurationQuerySubmitted[] =
    "ContextualSearch.Session.Duration.QuerySubmitted";
const char kContextualSearchSessionAbandonedDuration[] =
    "ContextualSearch.Session.Duration.Abandoned";
const char kContextualSearchQuerySubmissionTime[] =
    "ContextualSearch.Query.Time.ToSubmission";
const char kContextualSearchFileUploadAttemptPerFileType[] =
    "ContextualSearch.Session.File.Browser.UploadAttemptCount.";
const char kContextualSearchFileUploadSuccessPerFileType[] =
    "ContextualSearch.Session.File.Browser.UploadSuccessCount.";
const char kContextualSearchFileUploadFailure[] =
    "ContextualSearch.Session.File.Browser.UploadFailureCount.";
const char kContextualSearchFileValidationErrorTypes[] =
    "ContextualSearch.Session.File.Browser.ValidationFailureCount.";
const char kContextualSearchQueryTextLength[] =
    "ContextualSearch.Query.TextLength";
const char kContextualSearchQueryFileCount[] =
    "ContextualSearch.Query.FileCount";
const char kContextualSearchQueryModality[] =
    "ContextualSearch.Query.Modality.V2";
const char kContextualSearchQueryCount[] =
    "ContextualSearch.Session.QueryCount";
const char kContextualSearchFileSizePerType[] = "ContextualSearch.File.Size.";

std::string UploadStatusToString(FileUploadStatus status) {
  switch (status) {
    case FileUploadStatus::kNotUploaded:
      return "NotUploaded";
    case FileUploadStatus::kProcessing:
      return "Processing";
    case FileUploadStatus::kProcessingSuggestSignalsReady:
      return "ProcessingSuggestSignalsReady";
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

SessionMetrics::SessionMetrics() = default;
SessionMetrics::~SessionMetrics() = default;

ContextualSearchMetricsRecorder::ContextualSearchMetricsRecorder(
    ContextualSearchSource source)
    : source_(source),
      metrics_suffix_(ContextualSearchSourceToString(source)),
      session_metrics_{std::make_unique<SessionMetrics>()} {}

ContextualSearchMetricsRecorder::~ContextualSearchMetricsRecorder() {
  // Record session abandonments and completions.
  if (session_state_ == SessionState::kSessionStarted) {
    RecordSessionAbandonedMetrics();
  } else if (session_state_ == SessionState::kNavigationOccurred) {
    RecordSessionCompletedMetrics();
  }
}

void ContextualSearchMetricsRecorder::NotifySessionStateChanged(
    SessionState session_state) {
  session_state_ = session_state;
  switch (session_state) {
    case SessionState::kSessionStarted:
      NotifySessionStarted();
      break;
    case SessionState::kQuerySubmitted:
      NotifyQuerySubmitted();
      break;
    case SessionState::kSessionAbandoned:
      RecordSessionAbandonedMetrics();
      break;
    // On navigation occurrences, keep track of the session state, but do not
    // record any metrics until the end of the session, as multiple queries can
    // be submitted, such as in the case were the AIM page is opened in a new
    // tab and the ContextualSearch remains open.
    case SessionState::kNavigationOccurred:
      break;
    default:
      DCHECK(session_state_ != SessionState::kNone);
  }
}

void ContextualSearchMetricsRecorder::OnFileUploadStatusChanged(
    lens::MimeType file_mime_type,
    FileUploadStatus file_upload_status,
    const std::optional<FileUploadErrorType>& error_type) {
  switch (file_upload_status) {
    case FileUploadStatus::kProcessing:
      session_metrics_->file_upload_attempt_count_per_type[file_mime_type]++;
      break;
    case FileUploadStatus::kUploadSuccessful:
      session_metrics_->file_upload_success_count_per_type[file_mime_type]++;
      break;
    // Every validation error will have an error type, but not every file status
    // has an error, hence safeguarding the error value.
    case FileUploadStatus::kValidationFailed:
      if (error_type.has_value()) {
        session_metrics_
            ->file_validation_failure_count_per_type[file_mime_type]
                                                    [error_type.value()]++;
      }
      break;
    case FileUploadStatus::kUploadFailed:
      session_metrics_->file_upload_failure_count_per_type[file_mime_type]++;
      break;
    // The following are not file upload success or failure statuses.
    case FileUploadStatus::kNotUploaded:
    case FileUploadStatus::kUploadStarted:
    case FileUploadStatus::kUploadExpired:
    case FileUploadStatus::kProcessingSuggestSignalsReady:
      break;
  }
}
void ContextualSearchMetricsRecorder::RecordQueryMetrics(int text_length,
                                                         int file_count) {
  base::UmaHistogramCounts1M(
      base::StrCat({kContextualSearchQueryTextLength, ".", metrics_suffix_}),
      text_length);
  bool has_text = text_length != 0;
  bool has_files = file_count != 0;
  // Submission requests will always have either 1) both text and files 2) text
  // only or 3) files only.
  MultimodalState multimodal_state =
      has_text ? (has_files ? MultimodalState::kTextAndFile
                            : MultimodalState::kTextOnly)
               : MultimodalState::kFileOnly;
  base::UmaHistogramEnumeration(
      base::StrCat({kContextualSearchQueryModality, ".", metrics_suffix_}),
      multimodal_state);
  base::UmaHistogramCounts100(
      base::StrCat({kContextualSearchQueryFileCount, ".", metrics_suffix_}),
      file_count);
}

void ContextualSearchMetricsRecorder::RecordFileSizeMetric(
    lens::MimeType mime_type,
    uint64_t file_size_bytes) {
  base::UmaHistogramCounts10M(kContextualSearchFileSizePerType +
                                  MimeTypeToString(mime_type) + "." +
                                  metrics_suffix_,
                              file_size_bytes);
  base::UmaHistogramCounts10M(
      kContextualSearchFileSizePerType + metrics_suffix_, file_size_bytes);
}

void ContextualSearchMetricsRecorder::RecordFileDeletedMetrics(
    bool success,
    lens::MimeType file_type,
    FileUploadStatus file_status) {
  base::UmaHistogramBoolean(
      base::StrCat({kContextualSearchFileDeleted, ".",
                    MimeTypeToString(file_type), ".",
                    UploadStatusToString(file_status), ".", metrics_suffix_}),
      success);
}

void ContextualSearchMetricsRecorder::RecordTabClickedMetrics(
    bool has_duplicate_title,
    std::optional<int> recency_ranking) {
  base::UmaHistogramBoolean(
      "ContextualSearch.TabContextAdded." + metrics_suffix_, true);

  base::UmaHistogramBoolean(
      "ContextualSearch.TabWithDuplicateTitleClicked." + metrics_suffix_,
      has_duplicate_title);

  if (recency_ranking) {
    base::UmaHistogramCounts100(
        "ContextualSearch.AddedTabContextRecencyRanking." + metrics_suffix_,
        *recency_ranking);
  }
}

void ContextualSearchMetricsRecorder::RecordTabContextMenuMetrics(
    int total_tab_count,
    int duplicate_title_count) {
  base::UmaHistogramCounts1000(
      "ContextualSearch.ActiveTabsCountOnContextMenuOpen." + metrics_suffix_,
      total_tab_count);
  base::UmaHistogramCounts1000(
      "ContextualSearch.DuplicateTabTitlesShownCount." + metrics_suffix_,
      duplicate_title_count);
}

void ContextualSearchMetricsRecorder::RecordToolsSubmissionType(
    SubmissionType submission_type) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ContextualSearch.Tools.SubmissionType", ".", metrics_suffix_}),
      submission_type);
}

void ContextualSearchMetricsRecorder::RecordToolState(
    SubmissionType submission_type,
    AimToolState tool_state) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.Tools.",
                    SubmissionTypeToString(submission_type), ".",
                    metrics_suffix_}),
      tool_state);
}

void ContextualSearchMetricsRecorder::NotifySessionStarted() {
  session_metrics_->session_elapsed_timer =
      std::make_unique<base::ElapsedTimer>();
}

void ContextualSearchMetricsRecorder::NotifyQuerySubmitted() {
  base::TimeDelta time_to_query_submission =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kContextualSearchQuerySubmissionTime, ".", metrics_suffix_}),
      time_to_query_submission);
  session_metrics_->num_query_submissions++;
}

void ContextualSearchMetricsRecorder::RecordSessionAbandonedMetrics() {
  // In the case that the user has submitted a query in a new tab and abandons
  // the ContextualSearch session record the session as completed.
  if (session_metrics_->num_query_submissions > 0) {
    RecordSessionCompletedMetrics();
    return;
  }
  base::TimeDelta session_duration =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kContextualSearchSessionAbandonedDuration, ".", metrics_suffix_}),
      session_duration);
  RecordTotalSessionDuration(session_duration);
  FinalizeSessionMetrics();
}

void ContextualSearchMetricsRecorder::RecordSessionCompletedMetrics() {
  base::TimeDelta session_duration =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      base::StrCat({kContextualSearchSessionDurationQuerySubmitted, ".",
                    metrics_suffix_}),
      session_duration);
  base::UmaHistogramCounts100(
      base::StrCat({kContextualSearchQueryCount, ".", metrics_suffix_}),
      session_metrics_->num_query_submissions);
  RecordTotalSessionDuration(session_duration);
  FinalizeSessionMetrics();
}

void ContextualSearchMetricsRecorder::RecordTotalSessionDuration(
    base::TimeDelta session_duration) {
  base::UmaHistogramMediumTimes(
      base::StrCat({kContextualSearchSessionDuration, ".", metrics_suffix_}),
      session_duration);
}

void ContextualSearchMetricsRecorder::FinalizeSessionMetrics() {
  // Log upload attempt metrics.
  int total_attempts = 0;
  for (const auto& file_info :
       session_metrics_->file_upload_attempt_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name = kContextualSearchFileUploadAttemptPerFileType +
                                 file_type + "." + metrics_suffix_;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
    total_attempts += file_info.second;
  }

  base::UmaHistogramCounts100(
      kContextualSearchFileUploadAttemptPerFileType + metrics_suffix_,
      total_attempts);

  // Log successful uploads.
  int total_successes = 0;
  for (const auto& file_info :
       session_metrics_->file_upload_success_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name = kContextualSearchFileUploadSuccessPerFileType +
                                 file_type + "." + metrics_suffix_;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
    total_successes += file_info.second;
  }

  base::UmaHistogramCounts100(
      kContextualSearchFileUploadSuccessPerFileType + metrics_suffix_,
      total_successes);

  // Log file upload failures.
  int total_failures = 0;
  for (const auto& file_info :
       session_metrics_->file_upload_failure_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name =
        kContextualSearchFileUploadFailure + file_type + "." + metrics_suffix_;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
    total_failures += file_info.second;
  }

  base::UmaHistogramCounts100(
      kContextualSearchFileUploadFailure + metrics_suffix_, total_failures);

  // Log file validation errors.
  std::map<FileUploadErrorType, int> total_errors_by_type;
  for (const auto& file_info :
       session_metrics_->file_validation_failure_count_per_type) {
    for (const auto& error_info : file_info.second) {
      std::string file_type = MimeTypeToString(file_info.first);
      std::string error_type = FileErrorToString(error_info.first);
      std::string histogram_name = kContextualSearchFileValidationErrorTypes +
                                   file_type + "." + error_type + "." +
                                   metrics_suffix_;
      base::UmaHistogramCounts100(histogram_name, error_info.second);
      total_errors_by_type[error_info.first] += error_info.second;
    }
  }
  for (const auto& agg_error : total_errors_by_type) {
    std::string error_type = FileErrorToString(agg_error.first);
    base::UmaHistogramCounts100(kContextualSearchFileValidationErrorTypes +
                                    error_type + "." + metrics_suffix_,
                                agg_error.second);
  }
  ResetSessionMetrics();
}

void ContextualSearchMetricsRecorder::ResetSessionMetrics() {
  session_metrics_->session_elapsed_timer.reset();
  session_metrics_->file_upload_attempt_count_per_type.clear();
  session_metrics_->file_upload_success_count_per_type.clear();
  session_metrics_->file_upload_failure_count_per_type.clear();
  session_metrics_->file_validation_failure_count_per_type.clear();
  session_metrics_->num_query_submissions = 0;
}

std::string ContextualSearchMetricsRecorder::FileErrorToString(
    FileUploadErrorType error) {
  switch (error) {
    case FileUploadErrorType::kUnknown:
      return "Unknown";
    case FileUploadErrorType::kBrowserProcessingError:
      return "BrowserProcessingError";
    case FileUploadErrorType::kNetworkError:
      return "NetworkError";
    case FileUploadErrorType::kServerError:
      return "ServerError";
    case FileUploadErrorType::kServerSizeLimitExceeded:
      return "ServerLimitExceededError";
    case FileUploadErrorType::kAborted:
      return "AbortedError";
    case FileUploadErrorType::kImageProcessingError:
      return "ImageProcessingError";
  }
}

std::string ContextualSearchMetricsRecorder::MimeTypeToString(
    lens::MimeType mime_type) {
  switch (mime_type) {
    case lens::MimeType::kPdf:
      return "Pdf";
    case lens::MimeType::kImage:
      return "Image";
    case lens::MimeType::kAnnotatedPageContent:
      return "Tab";
    default:
      return "Other";
  }
}

// static
std::string ContextualSearchMetricsRecorder::ContextualSearchSourceToString(
    ContextualSearchSource source) {
  switch (source) {
    case ContextualSearchSource::kContextualTasks:
      return "ContextualTasks";
    case ContextualSearchSource::kLens:
      return "Lens";
    case ContextualSearchSource::kOmnibox:
      return "Omnibox";
    case ContextualSearchSource::kNewTabPage:
      return "NewTabPage";
    case ContextualSearchSource::kUnknown:
      return "Unknown";
  }
}

std::string ContextualSearchMetricsRecorder::SubmissionTypeToString(
    SubmissionType submission_type) {
  switch (submission_type) {
    case SubmissionType::kDefault:
      return "Default";
    case SubmissionType::kDeepSearch:
      return "DeepSearch";
    case SubmissionType::kCreateImages:
      return "CreateImages";
  }
}

// static
void ContextualSearchMetricsRecorder::RecordConfigParseSuccess(
    ContextualSearchSource source,
    bool success) {
  base::UmaHistogramBoolean(
      base::StrCat({"ContextualSearch.ConfigParseSuccess", ".",
                    ContextualSearchSourceToString(source)}),
      success);
}

}  // namespace contextual_search

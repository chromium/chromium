// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"

namespace {
const char kComposeboxFileDeleted[] = "Composebox.Session.File.DeletedCount";
const char kComposeboxSessionDuration[] = "Composebox.Session.Duration.Total";
const char kComposeboxSessionDurationQuerySubmitted[] =
    "Composebox.Session.Duration.QuerySubmitted";
const char kComposeboxSessionAbandonedDuration[] =
    "Composebox.Session.Duration.Abandoned";
const char kComposeboxQuerySubmissionTime[] =
    "Composebox.Query.Time.ToSubmission";
const char kComposeboxFileUploadAttemptPerFileType[] =
    "Composebox.Session.File.Browser.UploadAttemptCount.";
const char kComposeboxFileUploadSuccessPerFileType[] =
    "Composebox.Session.File.Browser.UploadSuccessCount.";
const char kComposeboxFileUploadFailure[] =
    "Composebox.Session.File.Browser.UploadFailureCount.";
const char kComposeboxFileValidationErrorTypes[] =
    "Composebox.Session.File.Browser.ValidationFailureCount.";
const char kComposeboxQueryTextLength[] = "Composebox.Query.TextLength";
const char kComposeboxQueryFileCount[] = "Composebox.Query.FileCount";
const char kComposeboxQueryModality[] = "Composebox.Query.Modality";
const char kComposeboxQueryCount[] = "Composebox.Session.QueryCount";
const char kComposeboxFileSizePerType[] = "Composebox.File.Size.";

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

SessionMetrics::SessionMetrics() = default;
SessionMetrics::~SessionMetrics() = default;

ComposeboxMetricsRecorder::ComposeboxMetricsRecorder(
    std::string metric_category_name)
    : metric_category_name_(metric_category_name),
      session_metrics_{std::make_unique<SessionMetrics>()} {}

ComposeboxMetricsRecorder::~ComposeboxMetricsRecorder() {
  // Record session abandonments and completions.
  if (session_state_ == SessionState::kSessionStarted) {
    RecordSessionAbandonedMetrics();
  } else if (session_state_ == SessionState::kNavigationOccurred) {
    RecordSessionCompletedMetrics();
  }
}

void ComposeboxMetricsRecorder::NotifySessionStateChanged(
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
    // tab and the composebox remains open.
    case SessionState::kNavigationOccurred:
      break;
    default:
      DCHECK(session_state_ != SessionState::kNone);
  }
}

void ComposeboxMetricsRecorder::OnFileUploadStatusChanged(
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
      break;
  }
}
void ComposeboxMetricsRecorder::RecordQueryMetrics(int text_length,
                                                   int file_count) {
  base::UmaHistogramCounts1M(metric_category_name_ + kComposeboxQueryTextLength,
                             text_length);
  bool has_text = text_length != 0;
  bool has_files = file_count != 0;
  // Submission requests will always have either 1) both text and files 2) text
  // only or 3) files only.
  NtpComposeboxMultimodalState multimodal_state =
      has_text ? (has_files ? NtpComposeboxMultimodalState::kTextAndFile
                            : NtpComposeboxMultimodalState::kTextOnly)
               : NtpComposeboxMultimodalState::kFileOnly;
  base::UmaHistogramEnumeration(
      metric_category_name_ + kComposeboxQueryModality, multimodal_state);
  base::UmaHistogramCounts100(metric_category_name_ + kComposeboxQueryFileCount,
                              file_count);
}

void ComposeboxMetricsRecorder::RecordFileSizeMetric(lens::MimeType mime_type,
                                                     uint64_t file_size_bytes) {
  base::UmaHistogramCounts10M(metric_category_name_ +
                                  kComposeboxFileSizePerType +
                                  MimeTypeToString(mime_type),
                              file_size_bytes);
}

void ComposeboxMetricsRecorder::RecordFileDeletedMetrics(
    bool success,
    lens::MimeType file_type,
    FileUploadStatus file_status) {
  base::UmaHistogramBoolean(metric_category_name_ + kComposeboxFileDeleted +
                                "." + MimeTypeToString(file_type) + "." +
                                UploadStatusToString(file_status),
                            success);
}

void ComposeboxMetricsRecorder::NotifySessionStarted() {
  session_metrics_->session_elapsed_timer =
      std::make_unique<base::ElapsedTimer>();
}

void ComposeboxMetricsRecorder::NotifyQuerySubmitted() {
  base::TimeDelta time_to_query_submission =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      metric_category_name_ + kComposeboxQuerySubmissionTime,
      time_to_query_submission);
  session_metrics_->num_query_submissions++;
}

void ComposeboxMetricsRecorder::RecordSessionAbandonedMetrics() {
  // In the case that the user has submitted a query in a new tab and abandons
  // the composebox session record the session as completed.
  if (session_metrics_->num_query_submissions > 0) {
    RecordSessionCompletedMetrics();
    return;
  }
  base::TimeDelta session_duration =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      metric_category_name_ + kComposeboxSessionAbandonedDuration,
      session_duration);
  RecordTotalSessionDuration(session_duration);
  FinalizeSessionMetrics();
}

void ComposeboxMetricsRecorder::RecordSessionCompletedMetrics() {
  base::TimeDelta session_duration =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      metric_category_name_ + kComposeboxSessionDurationQuerySubmitted,
      session_duration);
  base::UmaHistogramCounts100(metric_category_name_ + kComposeboxQueryCount,
                              session_metrics_->num_query_submissions);
  RecordTotalSessionDuration(session_duration);
  FinalizeSessionMetrics();
}

void ComposeboxMetricsRecorder::RecordTotalSessionDuration(
    base::TimeDelta session_duration) {
  base::UmaHistogramMediumTimes(
      metric_category_name_ + kComposeboxSessionDuration, session_duration);
}

void ComposeboxMetricsRecorder::FinalizeSessionMetrics() {
  // Log upload attempt metrics.
  for (const auto& file_info :
       session_metrics_->file_upload_attempt_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name = metric_category_name_ +
                                 kComposeboxFileUploadAttemptPerFileType +
                                 file_type;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
  }

  // Log successful uploads.
  for (const auto& file_info :
       session_metrics_->file_upload_success_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name = metric_category_name_ +
                                 kComposeboxFileUploadSuccessPerFileType +
                                 file_type;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
  }

  // Log file upload failures.
  for (const auto& file_info :
       session_metrics_->file_upload_failure_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name =
        metric_category_name_ + kComposeboxFileUploadFailure + file_type;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
  }

  // Log file validation errors.
  for (const auto& file_info :
       session_metrics_->file_validation_failure_count_per_type) {
    for (const auto& error_info : file_info.second) {
      std::string file_type = MimeTypeToString(file_info.first);
      std::string error_type = FileErrorToString(error_info.first);
      std::string histogram_name = metric_category_name_ +
                                   kComposeboxFileValidationErrorTypes +
                                   file_type + "." + error_type;
      base::UmaHistogramCounts100(histogram_name, error_info.second);
    }
  }
  ResetSessionMetrics();
}

void ComposeboxMetricsRecorder::ResetSessionMetrics() {
  session_metrics_->session_elapsed_timer.reset();
  session_metrics_->file_upload_attempt_count_per_type.clear();
  session_metrics_->file_upload_success_count_per_type.clear();
  session_metrics_->file_upload_failure_count_per_type.clear();
  session_metrics_->file_validation_failure_count_per_type.clear();
  session_metrics_->num_query_submissions = 0;
}

std::string ComposeboxMetricsRecorder::FileErrorToString(
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

std::string ComposeboxMetricsRecorder::MimeTypeToString(
    lens::MimeType mime_type) {
  switch (mime_type) {
    case lens::MimeType::kPdf:
      return "Pdf";
    case lens::MimeType::kImage:
      return "Image";
    default:
      return "Other";
  }
}

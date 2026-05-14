// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
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

std::string UploadStatusToString(ContextUploadStatus status) {
  switch (status) {
    case ContextUploadStatus::kNotUploaded:
      return "NotUploaded";
    case ContextUploadStatus::kProcessing:
      return "Processing";
    case ContextUploadStatus::kProcessingSuggestSignalsReady:
      return "ProcessingSuggestSignalsReady";
    case ContextUploadStatus::kValidationFailed:
      return "ValidationFailed";
    case ContextUploadStatus::kUploadStarted:
      return "UploadStarted";
    case ContextUploadStatus::kUploadSuccessful:
      return "UploadSuccessful";
    case ContextUploadStatus::kUploadFailed:
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
      session_metrics_(std::make_unique<SessionMetrics>()) {}

void ContextualSearchMetricsRecorder::UpdateContextualSearchSource(
    ContextualSearchSource source) {
  source_ = source;
  metrics_suffix_ = ContextualSearchSourceToString(source);
}

ContextualSearchMetricsRecorder::~ContextualSearchMetricsRecorder() {
  // Record session abandonments and completions.
  if (session_state_ == SessionState::kSessionStarted) {
    RecordSessionAbandonedMetrics();
  } else if (session_state_ == SessionState::kNavigationOccurred ||
             session_state_ == SessionState::kQuerySubmitted) {
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

void ContextualSearchMetricsRecorder::OnContextUploadStatusChanged(
    lens::MimeType file_mime_type,
    ContextUploadStatus context_upload_status,
    const std::optional<ContextUploadErrorType>& error_type) {
  switch (context_upload_status) {
    case ContextUploadStatus::kProcessing:
      session_metrics_->file_upload_attempt_count_per_type[file_mime_type]++;
      break;
    case ContextUploadStatus::kUploadSuccessful:
      session_metrics_->file_upload_success_count_per_type[file_mime_type]++;
      break;
    // Every validation error will have an error type, but not every file status
    // has an error, hence safeguarding the error value.
    case ContextUploadStatus::kValidationFailed:
      if (error_type.has_value()) {
        session_metrics_
            ->file_validation_failure_count_per_type[file_mime_type]
                                                    [error_type.value()]++;
      }
      break;
    case ContextUploadStatus::kUploadFailed:
      session_metrics_->file_upload_failure_count_per_type[file_mime_type]++;
      break;
    // The following are not file upload success or failure statuses.
    case ContextUploadStatus::kNotUploaded:
    case ContextUploadStatus::kUploadStarted:
    case ContextUploadStatus::kUploadExpired:
    case ContextUploadStatus::kProcessingSuggestSignalsReady:
    case ContextUploadStatus::kUploadReplaced:
      break;
  }
}
void ContextualSearchMetricsRecorder::RecordQueryMetrics(
    bool has_tab_context,
    bool has_non_tab_context,
    int text_length,
    int file_count,
    bool has_drive_context) {
  // Query text length metric.
  base::UmaHistogramCounts1M(
      base::StrCat({kContextualSearchQueryTextLength, ".", metrics_suffix_}),
      text_length);

  // Query modality metrics.
  bool has_text = text_length != 0;
  bool has_files = file_count != 0;
  // Submission requests will always have either 1) both text and files 2) text
  // only or 3) files only.
  ContextualSearchMultimodalState multimodal_state =
      has_text ? (has_files ? ContextualSearchMultimodalState::kTextAndFile
                            : ContextualSearchMultimodalState::kTextOnly)
               : ContextualSearchMultimodalState::kFileOnly;
  base::UmaHistogramEnumeration(
      base::StrCat({kContextualSearchQueryModality, ".", metrics_suffix_}),
      multimodal_state);
  base::UmaHistogramCounts100(
      base::StrCat({kContextualSearchQueryFileCount, ".", metrics_suffix_}),
      file_count);

  ContextualSearchContextState state =
      ContextualSearchContextState::kWithoutContext;
  std::string context_state = "WithoutContext";
  if (has_tab_context) {
    state = ContextualSearchContextState::kWithTabContext;
    context_state = "WithTabContext";
  } else if (has_non_tab_context) {
    state = ContextualSearchContextState::kWithNonTabContext;
    context_state = "WithNonTabContext";
  }

  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"ContextualSearch.UserAction.SubmitQueryV2.", context_state,
                    ".", metrics_suffix_})
          .c_str()));

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ContextualSearch.UserAction.SubmitQueryV2.", metrics_suffix_}),
      state);

  if (has_drive_context) {
    base::RecordAction(base::UserMetricsAction(
        base::StrCat(
            {"ContextualSearch.UserAction.SubmitQueryV2.WithDriveContext.",
             metrics_suffix_})
            .c_str()));

    base::UmaHistogramEnumeration(
        base::StrCat(
            {"ContextualSearch.UserAction.SubmitQueryV2.", metrics_suffix_}),
        ContextualSearchContextState::kWithDriveContext);
  }

  if (text_length == 0 && file_count > 0) {
    base::RecordAction(base::UserMetricsAction(
        base::StrCat(
            {"ContextualSearch.UserAction.SubmitQueryV2.WithContextNoText.",
             metrics_suffix_})
            .c_str()));

    base::UmaHistogramEnumeration(
        base::StrCat(
            {"ContextualSearch.UserAction.SubmitQueryV2.", metrics_suffix_}),
        ContextualSearchContextState::kWithContextNoText);
  }

  // Query funnel metrics.
  for (const auto& funnel : session_metrics_->active_funnels) {
    base::UmaHistogramCounts1M(
        base::StrCat({kContextualSearchQueryTextLength, ".FunnelMetrics.",
                      funnel, ".", metrics_suffix_}),
        text_length);
    base::UmaHistogramEnumeration(
        base::StrCat({kContextualSearchQueryModality, ".FunnelMetrics.", funnel,
                      ".", metrics_suffix_}),
        multimodal_state);
    base::UmaHistogramCounts100(
        base::StrCat({kContextualSearchQueryFileCount, ".FunnelMetrics.",
                      funnel, ".", metrics_suffix_}),
        file_count);
  }

  // Time to query submission metrics.
  if (!session_metrics_->session_elapsed_timer) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"ContextualSearch.Session.QuerySubmittedWithoutSessionStart", ".",
             metrics_suffix_}),
        true);
    return;
  }

  base::TimeDelta time_to_query_submission =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kContextualSearchQuerySubmissionTime, ".", metrics_suffix_}),
      time_to_query_submission);

  session_metrics_->num_query_submissions++;
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
    ContextUploadStatus file_status) {
  base::UmaHistogramBoolean(
      base::StrCat({kContextualSearchFileDeleted, ".",
                    MimeTypeToString(file_type), ".",
                    UploadStatusToString(file_status), ".", metrics_suffix_}),
      success);
}

void ContextualSearchMetricsRecorder::RecordTabAddedMetrics(
    bool has_duplicate_title,
    std::optional<int> recency_ranking,
    bool is_tab_suggestion_chip) {
  session_metrics_->tab_context_added_count++;
  if (is_tab_suggestion_chip) {
    session_metrics_->tab_context_added_from_tab_suggestion_chip_count++;
  } else {
    session_metrics_->tab_context_added_from_plus_button_count++;
  }
  if (has_duplicate_title) {
    session_metrics_->tab_with_duplicate_title_clicked_count++;
  }

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
  if (duplicate_title_count >= 0) {
    base::UmaHistogramCounts1000(
        "ContextualSearch.DuplicateTabTitlesShownCount." + metrics_suffix_,
        duplicate_title_count);
  }
}

void ContextualSearchMetricsRecorder::ActivateMetricsFunnel(
    const std::string& funnel_name) {
  if (session_state_ == SessionState::kNone) {
    // Ensure that session logging is enabled. This ensures that the session
    // is recorded for some funnels that may create a session, like the
    // plus button in the Realbox.
    NotifySessionStateChanged(SessionState::kSessionStarted);
  }
  session_metrics_->active_funnels.insert(funnel_name);
}

void ContextualSearchMetricsRecorder::NotifySessionStarted() {
  if (session_metrics_->session_elapsed_timer) {
    return;
  }
  session_metrics_->session_elapsed_timer =
      std::make_unique<base::ElapsedTimer>();
}

void ContextualSearchMetricsRecorder::NotifyQuerySubmitted(
    bool has_tab_context,
    bool has_non_tab_context,
    int query_text_length,
    int file_count,
    bool has_drive_context) {
  NotifySessionStateChanged(SessionState::kQuerySubmitted);
  RecordQueryMetrics(has_tab_context, has_non_tab_context, query_text_length,
                     file_count, has_drive_context);
}

void ContextualSearchMetricsRecorder::RecordSessionAbandonedMetrics() {
  // In the case that the user has submitted a query in a new tab and abandons
  // the ContextualSearch session record the session as completed.
  if (session_metrics_->num_query_submissions > 0) {
    RecordSessionCompletedMetrics();
    return;
  }
  if (session_metrics_->session_elapsed_timer) {
    base::TimeDelta session_duration =
        session_metrics_->session_elapsed_timer->Elapsed();
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {kContextualSearchSessionAbandonedDuration, ".", metrics_suffix_}),
        session_duration);
    RecordTotalSessionDuration(session_duration);
  }
  FinalizeSessionMetrics();
}

void ContextualSearchMetricsRecorder::RecordSessionCompletedMetrics() {
  if (session_metrics_->session_elapsed_timer) {
    base::TimeDelta session_duration =
        session_metrics_->session_elapsed_timer->Elapsed();
    base::UmaHistogramMediumTimes(
        base::StrCat({kContextualSearchSessionDurationQuerySubmitted, ".",
                      metrics_suffix_}),
        session_duration);
    RecordTotalSessionDuration(session_duration);
  }
  FinalizeSessionMetrics();
}

void ContextualSearchMetricsRecorder::RecordTotalSessionDuration(
    base::TimeDelta session_duration) {
  base::UmaHistogramMediumTimes(
      base::StrCat({kContextualSearchSessionDuration, ".", metrics_suffix_}),
      session_duration);
}

void ContextualSearchMetricsRecorder::FinalizeSessionMetrics() {
  base::UmaHistogramCounts100(
      base::StrCat({kContextualSearchQueryCount, ".", metrics_suffix_}),
      session_metrics_->num_query_submissions);
  for (const auto& funnel : session_metrics_->active_funnels) {
    base::UmaHistogramCounts100(
        base::StrCat({kContextualSearchQueryCount, ".FunnelMetrics.", funnel,
                      ".", metrics_suffix_}),
        session_metrics_->num_query_submissions);
  }
  base::UmaHistogramCounts100(
      "ContextualSearch.TabContextAdded.V2." + metrics_suffix_,
      session_metrics_->tab_context_added_count);
  base::UmaHistogramCounts100(
      "ContextualSearch.TabContextAddedFromTabSuggestionChip." +
          metrics_suffix_,
      session_metrics_->tab_context_added_from_tab_suggestion_chip_count);
  base::UmaHistogramCounts100(
      "ContextualSearch.TabContextAddedFromPlusButton." + metrics_suffix_,
      session_metrics_->tab_context_added_from_plus_button_count);
  base::UmaHistogramCounts100(
      "ContextualSearch.TabWithDuplicateTitleClicked.V2." + metrics_suffix_,
      session_metrics_->tab_with_duplicate_title_clicked_count);
  // Log upload attempt metrics.
  int total_attempts = 0;
  for (const auto& file_info :
       session_metrics_->file_upload_attempt_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name = kContextualSearchFileUploadAttemptPerFileType +
                                 file_type + "." + metrics_suffix_;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
    total_attempts += file_info.second;
    for (const auto& funnel : session_metrics_->active_funnels) {
      base::UmaHistogramCounts100(
          base::StrCat({kContextualSearchFileUploadAttemptPerFileType,
                        "FunnelMetrics.", file_type, ".", funnel, ".",
                        metrics_suffix_}),
          file_info.second);
    }
  }

  base::UmaHistogramCounts100(
      kContextualSearchFileUploadAttemptPerFileType + metrics_suffix_,
      total_attempts);

  for (const auto& funnel : session_metrics_->active_funnels) {
    base::UmaHistogramCounts100(
        base::StrCat({kContextualSearchFileUploadAttemptPerFileType,
                      "FunnelMetrics.", funnel, ".", metrics_suffix_}),
        total_attempts);
  }

  // Log successful uploads.
  int total_successes = 0;
  for (const auto& file_info :
       session_metrics_->file_upload_success_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name = kContextualSearchFileUploadSuccessPerFileType +
                                 file_type + "." + metrics_suffix_;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
    total_successes += file_info.second;
    for (const auto& funnel : session_metrics_->active_funnels) {
      base::UmaHistogramCounts100(
          base::StrCat({kContextualSearchFileUploadSuccessPerFileType,
                        "FunnelMetrics.", file_type, ".", funnel, ".",
                        metrics_suffix_}),
          file_info.second);
    }
  }

  base::UmaHistogramCounts100(
      kContextualSearchFileUploadSuccessPerFileType + metrics_suffix_,
      total_successes);

  for (const auto& funnel : session_metrics_->active_funnels) {
    base::UmaHistogramCounts100(
        base::StrCat({kContextualSearchFileUploadSuccessPerFileType,
                      "FunnelMetrics.", funnel, ".", metrics_suffix_}),
        total_successes);
  }

  // Log file upload failures.
  int total_failures = 0;
  for (const auto& file_info :
       session_metrics_->file_upload_failure_count_per_type) {
    std::string file_type = MimeTypeToString(file_info.first);
    std::string histogram_name =
        kContextualSearchFileUploadFailure + file_type + "." + metrics_suffix_;
    base::UmaHistogramCounts100(histogram_name, file_info.second);
    total_failures += file_info.second;
    for (const auto& funnel : session_metrics_->active_funnels) {
      base::UmaHistogramCounts100(
          base::StrCat({kContextualSearchFileUploadFailure, "FunnelMetrics.",
                        file_type, ".", funnel, ".", metrics_suffix_}),
          file_info.second);
    }
  }

  base::UmaHistogramCounts100(
      kContextualSearchFileUploadFailure + metrics_suffix_, total_failures);

  for (const auto& funnel : session_metrics_->active_funnels) {
    base::UmaHistogramCounts100(
        base::StrCat({kContextualSearchFileUploadFailure, "FunnelMetrics.",
                      funnel, ".", metrics_suffix_}),
        total_failures);
  }

  // Log file validation errors.
  std::map<ContextUploadErrorType, int> total_errors_by_type;
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
  session_metrics_->tab_context_added_count = 0;
  session_metrics_->tab_with_duplicate_title_clicked_count = 0;
  session_metrics_->file_upload_attempt_count_per_type.clear();
  session_metrics_->file_upload_success_count_per_type.clear();
  session_metrics_->file_upload_failure_count_per_type.clear();
  session_metrics_->file_validation_failure_count_per_type.clear();
  session_metrics_->num_query_submissions = 0;
  session_metrics_->active_funnels.clear();
}

std::string ContextualSearchMetricsRecorder::FileErrorToString(
    ContextUploadErrorType error) {
  switch (error) {
    case ContextUploadErrorType::kUnknown:
      return "Unknown";
    case ContextUploadErrorType::kBrowserProcessingError:
      return "BrowserProcessingError";
    case ContextUploadErrorType::kNetworkError:
      return "NetworkError";
    case ContextUploadErrorType::kServerError:
      return "ServerError";
    case ContextUploadErrorType::kServerSizeLimitExceeded:
      return "ServerLimitExceededError";
    case ContextUploadErrorType::kAborted:
      return "AbortedError";
    case ContextUploadErrorType::kImageProcessingError:
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

// static
void ContextualSearchMetricsRecorder::RecordConfigParseSuccess(
    ContextualSearchSource source,
    bool success) {
  base::UmaHistogramBoolean(
      base::StrCat({"ContextualSearch.ConfigParseSuccess", ".",
                    ContextualSearchSourceToString(source)}),
      success);
}

void ContextualSearchMetricsRecorder::RecordToolMode(
    omnibox::ToolMode tool_mode) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.Tools", ".", metrics_suffix_}), tool_mode,
      static_cast<omnibox::ToolMode>(omnibox::ToolMode_MAX + 1));
}

void ContextualSearchMetricsRecorder::RecordModelMode(
    omnibox::ModelMode model_mode) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.Models", ".", metrics_suffix_}),
      model_mode, static_cast<omnibox::ModelMode>(omnibox::ModelMode_MAX + 1));
}

void ContextualSearchMetricsRecorder::RecordToolModeShown(
    omnibox::ToolMode tool_mode) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.Tools.Shown", ".", metrics_suffix_}),
      tool_mode, static_cast<omnibox::ToolMode>(omnibox::ToolMode_MAX + 1));
}

void ContextualSearchMetricsRecorder::RecordModelModeShown(
    omnibox::ModelMode model_mode) {
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.Models.Shown", ".", metrics_suffix_}),
      model_mode, static_cast<omnibox::ModelMode>(omnibox::ModelMode_MAX + 1));
}

void ContextualSearchMetricsRecorder::RecordFileTypesOnSessionEnd(
    const std::vector<lens::MimeType>& types,
    bool navigated) {
  for (lens::MimeType type : types) {
    base::UmaHistogramBoolean(
        base::StrCat({"ContextualSearch.SessionEnd.NavigationResult.",
                      MimeTypeToString(type), ".", metrics_suffix_}),
        navigated);
  }
}

void ContextualSearchMetricsRecorder::RecordActiveModesOnSessionEnd(
    omnibox::ToolMode tool_mode,
    omnibox::ModelMode model_mode,
    bool navigated) {
  std::string result_str = navigated ? "Navigated" : "Abandoned";

  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.SessionEnd.", result_str, ".ToolMode",
                    ".", metrics_suffix_}),
      tool_mode, static_cast<omnibox::ToolMode>(omnibox::ToolMode_MAX + 1));

  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.SessionEnd.", result_str, ".ModelMode",
                    ".", metrics_suffix_}),
      model_mode, static_cast<omnibox::ModelMode>(omnibox::ModelMode_MAX + 1));
}

void ContextualSearchMetricsRecorder::RecordNavigationResult(bool navigated) {
  std::string result_str = navigated ? "Navigated" : "Abandoned";
  base::UmaHistogramEnumeration(
      base::StrCat({"ContextualSearch.Entrypoint.", result_str}), source_);
}

void ContextualSearchMetricsRecorder::RecordModesOnSubmission(
    omnibox::ToolMode tool_mode,
    omnibox::ModelMode model_mode,
    const std::vector<omnibox::InputType>& input_types) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ContextualSearch.Tools.ModeOnSubmission", ".", metrics_suffix_}),
      tool_mode, static_cast<omnibox::ToolMode>(omnibox::ToolMode_MAX + 1));
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ContextualSearch.Models.ModeOnSubmission", ".", metrics_suffix_}),
      model_mode, static_cast<omnibox::ModelMode>(omnibox::ModelMode_MAX + 1));

  std::set<omnibox::InputType> unique_input_types(input_types.begin(),
                                                  input_types.end());
  for (const auto& input_type : unique_input_types) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"ContextualSearch.Inputs.TypeOnSubmission", ".", metrics_suffix_}),
        input_type,
        static_cast<omnibox::InputType>(omnibox::InputType_MAX + 1));
  }
}

void ContextualSearchMetricsRecorder::RecordZeroSuggestClick(
    bool is_contextual) {
  std::string suffix = is_contextual ? "Contextual" : "NonContextual";
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"ContextualSearch.ZeroSuggestClickV2.", suffix, ".",
                    metrics_suffix_})
          .c_str()));
  base::UmaHistogramBoolean(
      base::StrCat({"ContextualSearch.ZeroSuggestClickV2.IsContextual.",
                    metrics_suffix_}),
      is_contextual);
}

void ContextualSearchMetricsRecorder::RecordTypedSuggestNavigation(
    bool is_verbatim) {
  std::string suffix = is_verbatim ? "Verbatim" : "SearchSuggest";
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"ContextualSearch.TypedSuggestNavigation.", suffix, ".",
                    metrics_suffix_})
          .c_str()));
  base::UmaHistogramBoolean(
      base::StrCat({"ContextualSearch.TypedSuggestNavigation.IsVerbatim.",
                    metrics_suffix_}),
      is_verbatim);
}

}  // namespace contextual_search

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_METRICS_RECORDER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_METRICS_RECORDER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "components/contextual_search/contextual_search_types.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace lens {
enum class MimeType;
}  // namespace lens

namespace contextual_search {

// LINT.IfChange(ContextualSearchSource)

enum class ContextualSearchSource {
  kUnknown,
  kNewTabPage,
  kOmnibox,
  kContextualTasks,
  kLens,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/histograms.xml:ContextualSearchSource,//tools/metrics/actions/actions.xml)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Describes the query submission details.
enum class MultimodalState {
  kTextOnly = 0,
  kFileOnly = 1,
  kTextAndFile = 2,
  kMaxValue = kTextAndFile,
};

enum class SessionState {
  kNone = 0,
  kSessionStarted = 1,
  kSessionAbandoned = 2,
  kQuerySubmitted = 3,
  kNavigationOccurred = 4,
};

struct SessionMetrics {
  SessionMetrics();
  ~SessionMetrics();
  // Timer to keep track of the session durations.
  std::unique_ptr<base::ElapsedTimer> session_elapsed_timer;
  // Number of file upload attempts per file type.
  std::map<lens::MimeType, int> file_upload_attempt_count_per_type;
  // Number of successful file uploads per file type.
  std::map<lens::MimeType, int> file_upload_success_count_per_type;
  // Number of file upload failures per file type.
  std::map<lens::MimeType, int> file_upload_failure_count_per_type;
  // Number of file validation errors per file type.
  std::map<lens::MimeType, std::map<FileUploadErrorType, int>>
      file_validation_failure_count_per_type;
  // In most cases `num_query_submissions` will equal 1 except in the case
  // where a user navigates to the AIM page on a new window or tab and the
  // composebox remains open.
  int num_query_submissions = 0;
};

class ContextualSearchMetricsRecorder {
 public:
  explicit ContextualSearchMetricsRecorder(ContextualSearchSource source);
  virtual ~ContextualSearchMetricsRecorder();

  // Should be called when there are session state changes to keep track of
  // session state metrics. Virtual for testing.
  virtual void NotifySessionStateChanged(SessionState session_state);

  virtual void OnFileUploadStatusChanged(
      lens::MimeType file_mime_type,
      FileUploadStatus file_upload_status,
      const std::optional<FileUploadErrorType>& error_type);

  // Maps file errors to its string version for histogram naming.
  std::string FileErrorToString(FileUploadErrorType error);
  // Maps mime types to its string version for histogram naming.
  std::string MimeTypeToString(lens::MimeType mime_type);
  // Maps contextual search sources to its string version for histogram naming.
  static std::string ContextualSearchSourceToString(
      ContextualSearchSource source);
  ContextualSearchSource source() const { return source_; }
  // Maps submission types to its string version for histogram naming.
  std::string SubmissionTypeToString(SubmissionType submission_type);

  // Records several metrics about the query, such the number of characters
  // found in the query.
  void RecordQueryMetrics(int text_length, int file_count);

  void RecordFileSizeMetric(lens::MimeType mime_type, uint64_t file_size_bytes);

  // Should be called when a file has been deleted.
  void RecordFileDeletedMetrics(bool success,
                                lens::MimeType file_type,
                                FileUploadStatus file_status);

  void RecordTabClickedMetrics(bool has_duplicate_title,
                               std::optional<int> recency_ranking);

  void RecordTabContextMenuMetrics(int total_tab_count,
                                   int duplicate_title_count);

  void RecordToolsSubmissionType(SubmissionType submission_type);

  void RecordToolState(SubmissionType submission_type, AimToolState tool_state);

  // Records whether the config was parsed successfully.
  static void RecordConfigParseSuccess(ContextualSearchSource source,
                                       bool success);

 private:
  // Called when the session starts to correctly track session
  // durations.
  void NotifySessionStarted();
  // Called when a query is submitted to correctly track the time from
  // the session starting to query submission.
  void NotifyQuerySubmitted();
  // Should only be called when a session has been abandoned.
  void RecordSessionAbandonedMetrics();
  // Should only be called if a query was submitted and navigation to the AIM
  // page occurred.
  void RecordSessionCompletedMetrics();
  // Records session durations regardless of whether the session was abandoned
  // or completed successfully.
  void RecordTotalSessionDuration(base::TimeDelta session_duration);
  // Records all file upload attempts.
  void FinalizeSessionMetrics();
  // Resets all session metrics at the end of a session.
  void ResetSessionMetrics();
  ContextualSearchSource source_;
  std::string metrics_suffix_;
  std::unique_ptr<SessionMetrics> session_metrics_;
  SessionState session_state_ = SessionState::kNone;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_METRICS_RECORDER_H_

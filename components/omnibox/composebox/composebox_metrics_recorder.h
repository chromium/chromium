// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_METRICS_RECORDER_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_METRICS_RECORDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"

enum class SessionState {
  kNone = 0,
  kSessionStarted = 1,
  kSessionAbandoned = 2,
  kQuerySubmitted = 3,
  kNavigationOccurred = 4,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Describes the query submission details.
enum class NtpComposeboxMultimodalState {
  kTextOnly = 0,
  kFileOnly = 1,
  kTextAndFile = 2,
  kMaxValue = kTextAndFile,
};

using FileUploadStatus = composebox_query::mojom::FileUploadStatus;

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

class ComposeboxMetricsRecorder {
 public:
  explicit ComposeboxMetricsRecorder(std::string metric_component_name);
  virtual ~ComposeboxMetricsRecorder();

  // Should be called when there are session state changes to keep track of
  // session state metrics. Virtual for testing.
  virtual void NotifySessionStateChanged(SessionState session_state);

  void OnFileUploadStatusChanged(
      lens::MimeType file_mime_type,
      FileUploadStatus file_upload_status,
      const std::optional<FileUploadErrorType>& error_type);

  // Maps file errors to its string version for histogram naming.
  std::string FileErrorToString(FileUploadErrorType error);
  // Maps mime types to its string version for histogram naming.
  std::string MimeTypeToString(lens::MimeType mime_type);

  // Records several metrics about the query, such the number of characters
  // found in the query.
  void RecordQueryMetrics(int text_length, int file_count);

  void RecordFileSizeMetric(lens::MimeType mime_type, uint64_t file_size_bytes);

  // Should be called when a file has been deleted.
  void RecordFileDeletedMetrics(bool success,
                                lens::MimeType file_type,
                                FileUploadStatus file_status);

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
  std::string metric_category_name_;
  std::unique_ptr<SessionMetrics> session_metrics_;
  SessionState session_state_ = SessionState::kNone;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_METRICS_RECORDER_H_

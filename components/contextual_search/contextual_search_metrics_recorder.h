// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_METRICS_RECORDER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_METRICS_RECORDER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace lens {
enum class MimeType;
}  // namespace lens

namespace contextual_search {

// LINT.IfChange(ContextualSearchSource)

enum class ContextualSearchSource {
  kUnknown = 0,
  kNewTabPage = 1,
  kOmnibox = 2,
  kContextualTasks = 3,
  kLens = 4,
  kMaxValue = kLens,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/histograms.xml:ContextualSearchSource,//tools/metrics/actions/actions.xml)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Describes the query submission details.
enum class ContextualSearchMultimodalState {
  kTextOnly = 0,
  kFileOnly = 1,
  kTextAndFile = 2,
  kMaxValue = kTextAndFile,
};

// LINT.IfChange(ContextualSearchContextState)
enum class ContextualSearchContextState {
  kWithoutContext = 0,
  kWithTabContext = 1,
  kWithNonTabContext = 2,
  kWithContextNoText = 3,
  kWithDriveContext = 4,
  kMaxValue = kWithDriveContext,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextualSearchContextState)

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
  std::map<lens::MimeType, std::map<ContextUploadErrorType, int>>
      file_validation_failure_count_per_type;
  // In most cases `num_query_submissions` will equal 1 except in the case
  // where a user navigates to the AIM page on a new window or tab and the
  // composebox remains open.
  int num_query_submissions = 0;
  // The number of times a tab is added as context to the session.
  int tab_context_added_count = 0;
  // The number of times a tab is added as context to the session via the tab
  // suggestion chip.
  int tab_context_added_from_tab_suggestion_chip_count = 0;
  // The number of time a tab is added as context to the session via the plus
  // button (i.e. not via the tab suggestion chip).
  int tab_context_added_from_plus_button_count = 0;
  // The number of times a tab with a duplicate title is added as context to the
  // session.
  int tab_with_duplicate_title_clicked_count = 0;
  // The set of active funnels for this session.
  std::set<std::string> active_funnels;
};

class ContextualSearchMetricsRecorder {
 public:
  explicit ContextualSearchMetricsRecorder(ContextualSearchSource source);
  virtual ~ContextualSearchMetricsRecorder();

  // Should be called when there are session state changes to keep track of
  // session state metrics. Virtual for testing.
  // TODO(crbug.com/458086158): Make this private and instead make
  // NotifySessionStarted, NotifyQuerySubmitted, RecordSessionAbandonedMetrics,
  // and a new NotifyNavigationOccurred method public so that the session state
  // can be managed internally by the metrics recorder.
  virtual void NotifySessionStateChanged(SessionState session_state);

  // Notifies the metrics recorder that a query was submitted.
  virtual void NotifyQuerySubmitted(bool has_tab_context,
                                    bool has_non_tab_context,
                                    int query_text_length,
                                    int file_count,
                                    bool has_drive_context);

  // Activates a funnel for metrics logging.
  virtual void ActivateMetricsFunnel(const std::string& funnel_name);

  virtual void OnContextUploadStatusChanged(
      lens::MimeType file_mime_type,
      ContextUploadStatus context_upload_status,
      const std::optional<ContextUploadErrorType>& error_type);

  // Maps file errors to its string version for histogram naming.
  std::string FileErrorToString(ContextUploadErrorType error);
  // Maps mime types to its string version for histogram naming.
  std::string MimeTypeToString(lens::MimeType mime_type);
  // Maps contextual search sources to its string version for histogram naming.
  static std::string ContextualSearchSourceToString(
      ContextualSearchSource source);
  ContextualSearchSource source() const { return source_; }

  // Records several metrics about the query, such the number of characters
  // found in the query.
  void RecordQueryMetrics(bool has_tab_context,
                          bool has_non_tab_context,
                          int text_length,
                          int file_count,
                          bool has_drive_context);

  void RecordFileSizeMetric(lens::MimeType mime_type, uint64_t file_size_bytes);

  // Should be called when a file has been deleted.
  void RecordFileDeletedMetrics(bool success,
                                lens::MimeType file_type,
                                ContextUploadStatus file_status);

  void RecordTabAddedMetrics(bool has_duplicate_title,
                               std::optional<int> recency_ranking,
                               bool is_tab_suggestion_chip);

  // If `duplicate_title_count` < 0 then it won't be recorded.
  void RecordTabContextMenuMetrics(int total_tab_count,
                                   int duplicate_title_count);

  // Records whether the config was parsed successfully.
  static void RecordConfigParseSuccess(ContextualSearchSource source,
                                       bool success);

  // Records the tool mode (i.e. Deep Search, Create Images, etc.).
  virtual void RecordToolMode(omnibox::ToolMode tool_mode);

  // Records the model mode (i.e. Gemini Pro, Gemini Pro Autoroute, etc.).
  virtual void RecordModelMode(omnibox::ModelMode model_mode);

  // Records that a specific tool mode is available for use.
  virtual void RecordToolModeShown(omnibox::ToolMode tool_mode);

  // Records that a specific model mode is available for use.
  virtual void RecordModelModeShown(omnibox::ModelMode model_mode);

  // Recorded when the user ends the contextual search session, sliced by
  // whether they navigated to the AI response or abandoned it. Records
  // independent booleans for each file type present.
  virtual void RecordFileTypesOnSessionEnd(
      const std::vector<lens::MimeType>& types,
      bool navigated);

  // Recorded when the user ends the contextual search session, sliced by
  // whether they navigated to the AI response or abandoned it. Records
  // the active tool and model mode at session end.
  virtual void RecordActiveModesOnSessionEnd(omnibox::ToolMode tool_mode,
                                             omnibox::ModelMode model_mode,
                                             bool navigated);

  // Records whether a contextual search session initiated eventually
  // resulted in a successful navigation or was abandoned.
  virtual void RecordNavigationResult(bool navigated);

  // Updates the source of the contextual search session. This is needed when
  // a session is transferred over from another source so that the metrics
  // are recorded for the correct source.
  void UpdateContextualSearchSource(ContextualSearchSource source);

  // Records tool mode, model mode, and input types on query submission.
  virtual void RecordModesOnSubmission(
      omnibox::ToolMode tool_mode,
      omnibox::ModelMode model_mode,
      const std::vector<omnibox::InputType>& input_types);

  // Records when a zero-suggest suggestion is clicked.
  virtual void RecordZeroSuggestClick(bool is_contextual);

  // Records when a typed suggestion is clicked.
  virtual void RecordTypedSuggestNavigation(bool is_verbatim);

 private:
  // Called when the session starts to correctly track session
  // durations.
  void NotifySessionStarted();
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

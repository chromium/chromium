// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "base/types/optional_ref.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/signatures.h"
#include "url/gurl.h"

namespace accessibility_annotator {
struct MemorySearchResults;
}  // namespace accessibility_annotator

namespace optimization_guide {
class ModelQualityLogEntry;
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

namespace autofill {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Logs the final execution status of search queries.
enum class AtMemoryQueryCompletedStatus {
  // The query completed successfully and returned at least one result.
  kQueryReturnedData = 0,
  // The query service did not support the query input.
  kQueryUnsupported = 1,
  // The query completed successfully but returned no results.
  kNoData = 2,
  // The query failed due to connection error.
  kNetworkError = 3,
  // The query failed due to model inference failure.
  kInferenceFailure = 4,
  // The query failed due to internal query service error.
  kInternalError = 5,
  kMaxValue = kInternalError
};

// Encapsulates the state and logging logic for the @memory search funnel.
// This class tracks the progression of a user's interaction with the @memory
// suggestions, from the initial display to the submission of a query.
class AtMemoryMetricsRecorder {
 public:
  AtMemoryMetricsRecorder(
      optimization_guide::ModelQualityLogsUploaderService* uploader_service,
      GURL url,
      std::u16string_view title,
      FormSignature form_signature,
      FieldSignature field_signature);
  AtMemoryMetricsRecorder(const AtMemoryMetricsRecorder&) = delete;
  AtMemoryMetricsRecorder& operator=(const AtMemoryMetricsRecorder&) = delete;
  ~AtMemoryMetricsRecorder();

  // Records that the popup UI was successfully displayed to the user.
  // This emits the "PopupDisplayed" metric. This method is idempotent; only
  // the first call per session will record metrics, and subsequent calls
  // with potentially different trigger sources are ignored. This is consistent
  // with the popup lifecycle, where a change in trigger mechanism would
  // typically result in the popup being hidden and a new session starting.
  void OnPopupShown(
      AutofillSuggestionTriggerSource trigger_source,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          parent_suggestion_metadata);

  // Records that a search query was submitted during this session.
  void OnQuerySubmitted(std::u16string_view query);

  // Records that a response for the pending query was received.
  void OnQueryResponseReceived(
      const accessibility_annotator::MemorySearchResults& result);

  // Records that a suggestion was accepted during this session.
  void OnSuggestionAccepted(
      accessibility_annotator::MemoryDataType memory_data_type,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          metadata = std::nullopt);

  // Records that the suggestion was successfully filled.
  void MarkFilled();

  // Records the start time of the asynchronous PII fetching process.
  void OnFetchPiiStarted();

  // Records the completion of the asynchronous PII fetching process.
  void OnFetchPiiCompleted();

 private:
  friend class AtMemoryMetricsRecorderTestApi;

  // Emits the `SuggestionAccepted` metric if `suggestion_accepted_` is not
  // `std::nullopt`.
  void MaybeLogSuggestionAccepted();

  // The unique identifier of the session. A session begins when the popup is
  // first shown to the user and ends when it is hidden. Popup updates (e.g.,
  // due to typing in the search bar) do not change the session.
  const base::Token session_id_token_;
  // The trigger source of the popup. It is `std::nullopt` until `OnPopupShown`
  // is called, serving as a signal that the popup was shown.
  std::optional<AutofillMetrics::AtMemoryTriggerSource> source_;
  bool query_submitted_ = false;
  // The pending log entry to be uploaded to MQLS for the query.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> pending_log_entry_;
  // The timer that measures the time between the query being submitted and
  // the suggestions being shown to the user. It is `std::nullopt` until
  // `OnQuerySubmitted` is called and will be reset when the query response was
  // received.
  std::optional<base::ElapsedTimer> query_to_suggestions_shown_timer_;

  // Information about whether a suggestion was accepted in response to the last
  // user query.
  struct {
    // Whether a non-empty query response has been received.
    bool suggestions_received = false;
    std::optional<accessibility_annotator::MemoryDataType> accepted_data_type;
  } suggestion_acceptance_;

  // Whether any suggestion has been accepted during the lifetime of `this`
  // recorder.
  bool suggestion_accepted_in_session_ = false;

  // Counts the number of queries submitted during this session.
  size_t query_count_ = 0;

  bool was_filled_ = false;
  // The start time of the asynchronous fetch/unmask process.
  std::optional<base::TimeTicks> fetch_pii_start_time_;
  // The duration of the successful asynchronous fetch/unmask process.
  std::optional<base::TimeDelta> fetch_pii_duration_;

  // The URL of the primary page the user triggered the @memory search on.
  const GURL url_;
  // The title of the primary page the user triggered the @memory search on.
  const std::u16string title_;

  // The form and field signature of the form the user triggered the @memory search on, or
  // 0 if no form was involved.
  const FormSignature form_signature_;
  const FieldSignature field_signature_;

  // The uploader service used to log metrics to MQLS. Not owned. Guaranteed to
  // outlive `this`.
  raw_ptr<optimization_guide::ModelQualityLogsUploaderService>
      uploader_service_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_

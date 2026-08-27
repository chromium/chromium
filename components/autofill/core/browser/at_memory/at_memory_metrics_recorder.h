// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/signatures.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace optimization_guide {
class ModelQualityLogEntry;
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

namespace ukm {
class UkmRecorder;
}  // namespace ukm

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

// LINT.IfChange(AutofillAtMemoryUiSessionOutcome)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Key terminal outcome events for an AtMemory UI session.
enum class AtMemoryUiSessionOutcome {
  // The popup was dismissed before any search query was submitted.
  kDismissedBeforeQuery = 0,
  // A query was submitted, but the popup was dismissed before receiving any
  // query response.
  kDismissedBeforeResults = 1,
  // A query completed, but the query returned empty results when the popup
  // was dismissed.
  kDismissedEmptyResults = 2,
  // The last query response returned suggestions, but the popup was dismissed
  // without accepting a suggestion.
  kDismissedResultsBeforeAcceptance = 3,
  // A suggestion was accepted, but not filled (e.g. authentication failed or
  // was cancelled).
  kSuggestionAcceptedNotFilled = 4,
  // A suggestion was accepted and successfully filled.
  kSuggestionFilled = 5,
  kMaxValue = kSuggestionFilled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAtMemoryUiSessionOutcome)

// Encapsulates the state and logging logic for the AtMemory search funnel.
// This class tracks the progression of a user's interaction with the AtMemory
// suggestions, from the initial display to the submission of a query.
class AtMemoryMetricsRecorder {
 public:
  AtMemoryMetricsRecorder(
      optimization_guide::ModelQualityLogsUploaderService* uploader_service,
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId ukm_source_id,
      GURL url,
      std::u16string_view title,
      const FieldGlobalId& field_id,
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
  void OnQueryResponseReceived(const MemorySearchResults& result);

  // Records that a suggestion was accepted during this session.
  using MemorySourcesBitmask = std::underlying_type_t<MemoryEntrySourceType>;
  void OnSuggestionAccepted(
      MemoryDataType memory_data_type,
      MemorySourcesBitmask sources_bitmask = 0,
      base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
          metadata = std::nullopt);

  // Records that the suggestion was successfully filled.
  void MarkFilled();

  // LINT.IfChange(FetchPiiSource)
  // The source of PII data fetched during filling.
  enum class FetchPiiSource {
    kAutofillAi = 0,
    kCreditCard = 1,
    kIban = 2,
    kPersonalContext = 3,
    kMaxValue = kPersonalContext,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/histograms.xml:Autofill.AtMemory.Latency.FetchPii)

  // Records the start time of the asynchronous PII fetching process.
  void OnFetchPiiStarted(FetchPiiSource source);

  // Records the completion of the asynchronous PII fetching process.
  void OnFetchPiiCompleted();

  // Records the failure reason of the asynchronous PII fetching process using
  // `PersonalContextService`.
  void OnFetchPersonalContextPiiDataFailed(
      AtMemoryQueryService::SpiiRetrievalFailureReason reason);

 private:
  friend class AtMemoryMetricsRecorderTestApi;

  bool CanLogUkm() const;

  // Emits the record in `ukm_search_query_builder_` if it exists.
  void MaybeFlushSearchQueryUkm();

  // Emits the `SuggestionAccepted` metric if `suggestion_accepted_` is not
  // `std::nullopt`.
  void MaybeLogSuggestionAccepted();

  // The unique identifier of the session. A session begins when the popup is
  // first shown to the user and ends when it is hidden. Popup updates (e.g.,
  // due to typing in the search bar) do not change the session.
  const base::Token session_id_token_;

  // The URL of the primary page the user triggered the AtMemory search on.
  const GURL url_;

  // The title of the primary page the user triggered the AtMemory search on.
  const std::u16string title_;

  // Identifiers of the field that the user triggered AtMemory on.
  const FieldGlobalId field_id_;
  const FormSignature form_signature_;
  const FieldSignature field_signature_;

  // The trigger source of the popup. It is `std::nullopt` until `OnPopupShown`
  // is called, serving as a signal that the popup was shown.
  std::optional<AutofillMetrics::AtMemoryTriggerSource> source_;

  // Counts the number of queries submitted during this session.
  size_t query_count_ = 0;

  // Counts the number of query responses received during this session.
  size_t query_response_count_ = 0;

  // Whether any suggestion has been accepted during the lifetime of `this`.
  bool suggestion_accepted_in_session_ = false;

  // Whether any suggestion has been filled during the lifetime of `this`.
  bool suggestion_filled_in_session_ = false;

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

    std::optional<MemoryDataType> accepted_data_type;

    std::optional<MemorySourcesBitmask> accepted_sources_bitmask;
  } suggestion_acceptance_;

  // Information about the asynchronous fetch/unmask process of PII.
  struct {
    std::optional<FetchPiiSource> source;

    // The start time of the asynchronous fetch/unmask process.
    std::optional<base::TimeTicks> start_time;

    // The duration of the successful asynchronous fetch/unmask process.
    std::optional<base::TimeDelta> duration;
  } fetch_pii_;

  // Members related to UKM:

  const raw_ptr<ukm::UkmRecorder> ukm_recorder_;
  const ukm::SourceId ukm_source_id_;

  // Used to assemble the data emitted in an `AtMemory.SearchQuery` record.
  std::optional<ukm::builders::AtMemory_SearchQuery> ukm_search_query_builder_;

  // Members related to MQLS:

  // The uploader service used to log metrics to MQLS. Guaranteed to outlive
  // `this`.
  const raw_ptr<optimization_guide::ModelQualityLogsUploaderService>
      uploader_service_;

  // The pending log entry to be uploaded to MQLS for the query.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> pending_log_entry_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_

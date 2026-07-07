// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/aliases.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

namespace autofill {

namespace {

std::optional<AtMemoryQueryCompletedStatus> GetQueryCompletedStatus(
    const accessibility_annotator::MemorySearchResults& result) {
  using accessibility_annotator::MemorySearchStatus;

  switch (result.status) {
    case MemorySearchStatus::kUnsupportedQuery:
      return AtMemoryQueryCompletedStatus::kQueryUnsupported;
    case MemorySearchStatus::kNoConnectionFailure:
      return AtMemoryQueryCompletedStatus::kNetworkError;
    case MemorySearchStatus::kInferenceFailure:
      return AtMemoryQueryCompletedStatus::kInferenceFailure;
    case MemorySearchStatus::kInternalFailure:
      return AtMemoryQueryCompletedStatus::kInternalError;
    case MemorySearchStatus::kFinalResponseSuccess:
      return result.entries.empty()
                 ? AtMemoryQueryCompletedStatus::kNoData
                 : AtMemoryQueryCompletedStatus::kQueryReturnedData;
    case MemorySearchStatus::kPartialResponseSuccess:
      return std::nullopt;
  }

  NOTREACHED();
}

}  // namespace

AtMemoryMetricsRecorder::AtMemoryMetricsRecorder(
    optimization_guide::ModelQualityLogsUploaderService* uploader_service,
    GURL url,
    std::u16string_view title,
    FormSignature form_signature,
    FieldSignature field_signature)
    : session_id_token_(base::Token::CreateRandom()),
      url_(std::move(url)),
      title_(title),
      form_signature_(form_signature),
      field_signature_(field_signature),
      uploader_service_(uploader_service) {}

AtMemoryMetricsRecorder::~AtMemoryMetricsRecorder() {
  // Only log summary metrics if the popup was successfully shown.
  // This avoids polluting the "No Query Submitted" data with cases where the
  // popup was hidden immediately after initialization (e.g., due to focus
  // loss) before the user could see or interact with it.
  if (source_.has_value()) {
    base::UmaHistogramBoolean("Autofill.AtMemory.QuerySubmitted",
                              query_submitted_);
    MaybeLogSuggestionAccepted();
    if (suggestion_acceptance_.accepted_data_type.has_value()) {
      // TODO(crbug.com/530438524): Fix and rename.
      base::UmaHistogramBoolean("Autofill.AtMemory.Funnel.SuggestionFilled",
                                was_filled_);
      if (fetch_pii_duration_) {
        base::UmaHistogramTimes("Autofill.AtMemory.Funnel.TimeToFetchUnmasked",
                                *fetch_pii_duration_);
      }
    }
  }
}

void AtMemoryMetricsRecorder::OnPopupShown(
    AutofillSuggestionTriggerSource trigger_source) {
  if (source_.has_value()) {
    return;
  }

  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kAtMemory:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger;
      break;
    case AutofillSuggestionTriggerSource::kAtMemoryContextMenu:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kContextMenu;
      break;
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldValueChanged:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
    case AutofillSuggestionTriggerSource::kProactivePasswordRecovery:
    case AutofillSuggestionTriggerSource::kGlic:
    case AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge:
      // This class should only be used for @memory searches.
      NOTREACHED();
  }

  // `source_` is set only when the popup is successfully displayed. This
  // serves as a signal that the user has actually seen the suggestions.
  base::UmaHistogramEnumeration("Autofill.AtMemory.SearchBarDisplayed",
                                *source_);
}

void AtMemoryMetricsRecorder::OnQuerySubmitted(std::u16string_view query) {
  ++query_count_;

  query_to_suggestions_shown_timer_.emplace();

  if (uploader_service_) {
    pending_log_entry_ =
        std::make_unique<optimization_guide::ModelQualityLogEntry>(
            uploader_service_->GetWeakPtr());
    optimization_guide::proto::AtMemoryQuality* quality =
        pending_log_entry_->log_ai_data_request()
            ->mutable_at_memory()
            ->mutable_quality();
    quality->set_session_id(session_id_token_.ToString());
    quality->set_query(base::UTF16ToUTF8(query));
    quality->set_url(url_.spec());
    quality->set_title(base::UTF16ToUTF8(title_));
    quality->set_form_signature(form_signature_.value());
    quality->set_field_signature(field_signature_.value());
    // Rely on log entry destructor to upload the log entry, so this will flush
    // when a new query comes in or the funnel metrics object gets destroyed.
  }

  query_submitted_ = true;
}

void AtMemoryMetricsRecorder::OnSuggestionAccepted(
    accessibility_annotator::MemoryDataType memory_data_type,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        metadata) {
  suggestion_acceptance_.accepted_data_type = memory_data_type;

  if (metadata.has_value() && !metadata->multi_index.empty()) {
    base::UmaHistogramSparse("Autofill.AtMemory.AcceptedSuggestionIndex",
                             static_cast<int>(metadata->multi_index[0]));
    const int secondary_index = metadata->multi_index.size() > 1
                                    ? static_cast<int>(metadata->multi_index[1])
                                    : -1;
    base::UmaHistogramSparse(
        "Autofill.AtMemory.AcceptedSuggestionSecondaryIndex", secondary_index);

    if (pending_log_entry_) {
      optimization_guide::proto::AtMemoryQuality* quality =
          pending_log_entry_->log_ai_data_request()
              ->mutable_at_memory()
              ->mutable_quality();
      if (metadata->multi_index[0] >=
          static_cast<size_t>(quality->suggestions_size())) {
        // This should never happen, but if it does, we should not crash.
        return;
      }
      auto* accepted_suggestion =
          quality->mutable_suggestions(metadata->multi_index[0]);
      accepted_suggestion->set_action(
          secondary_index == -1
              ? optimization_guide::proto::AT_MEMORY_SUGGESTION_ACTION_ACCEPTED
              : optimization_guide::proto::
                    AT_MEMORY_SUGGESTION_ACTION_FLYOUT_MENU_ATTRIBUTE_ACCEPTED);
    }
  }
}

void AtMemoryMetricsRecorder::OnQueryResponseReceived(
    const accessibility_annotator::MemorySearchResults& result) {
  if (std::optional<AtMemoryQueryCompletedStatus> status =
          GetQueryCompletedStatus(result)) {
    base::UmaHistogramEnumeration("Autofill.AtMemory.QueryCompleted", *status);
  }

  MaybeLogSuggestionAccepted();
  suggestion_acceptance_ = {.suggestions_received = !result.entries.empty()};

  if (!query_to_suggestions_shown_timer_) {
    return;
  }

  base::TimeDelta time_since_query_submitted =
      query_to_suggestions_shown_timer_->Elapsed();
  base::UmaHistogramTimes("Autofill.AtMemory.Latency.Query",
                          time_since_query_submitted);
  query_to_suggestions_shown_timer_.reset();

  if (!pending_log_entry_) {
    return;
  }
  auto* quality = pending_log_entry_->log_ai_data_request()
                      ->mutable_at_memory()
                      ->mutable_quality();
  quality->set_query_submitted_to_suggestions_shown_ms(
      time_since_query_submitted.InMilliseconds());

  for (const auto& suggestion : result.entries) {
    auto* quality_suggestion = quality->add_suggestions();
    bool has_autofill_source = std::ranges::contains(
        suggestion.sources,
        accessibility_annotator::MemoryEntrySourceType::kAutofill,
        &accessibility_annotator::MemoryEntrySource::type);
    quality_suggestion->set_source(
        has_autofill_source
            ? optimization_guide::proto::AT_MEMORY_SUGGESTION_SOURCE_AUTOFILL
            : optimization_guide::proto::
                  AT_MEMORY_SUGGESTION_SOURCE_CONTEXT_MEMORY_SERVICE);
    quality_suggestion->set_response_results_index(
        suggestion.remote_response_index.value_or(-1));
  }
}

void AtMemoryMetricsRecorder::OnFetchPiiStarted() {
  fetch_pii_start_time_ = base::TimeTicks::Now();
  fetch_pii_duration_.reset();
}

void AtMemoryMetricsRecorder::OnFetchPiiCompleted() {
  CHECK(fetch_pii_start_time_);
  fetch_pii_duration_.emplace(base::TimeTicks::Now() - *fetch_pii_start_time_);
}

void AtMemoryMetricsRecorder::MarkFilled() {
  was_filled_ = true;
}

void AtMemoryMetricsRecorder::MaybeLogSuggestionAccepted() {
  if (suggestion_acceptance_.suggestions_received) {
    base::UmaHistogramBoolean(
        "Autofill.AtMemory.SuggestionAccepted",
        suggestion_acceptance_.accepted_data_type.has_value());
  }
  if (suggestion_acceptance_.accepted_data_type.has_value()) {
    base::UmaHistogramCounts100("Autofill.AtMemory.QueryCountBeforeAcceptance",
                                query_count_);
  }
}

}  // namespace autofill

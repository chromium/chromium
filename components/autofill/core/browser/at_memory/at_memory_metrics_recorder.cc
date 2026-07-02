// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/aliases.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

namespace autofill {

AtMemoryMetricsRecorder::AtMemoryMetricsRecorder(
    optimization_guide::ModelQualityLogsUploaderService* uploader_service,
    GURL url,
    std::u16string_view title)
    : url_(std::move(url)),
      title_(title),
      uploader_service_(uploader_service) {}

AtMemoryMetricsRecorder::~AtMemoryMetricsRecorder() {
  // Only log summary metrics if the popup was successfully shown.
  // This avoids polluting the "No Query Submitted" data with cases where the
  // popup was hidden immediately after initialization (e.g., due to focus
  // loss) before the user could see or interact with it.
  if (source_.has_value()) {
    base::UmaHistogramBoolean("Autofill.AtMemory.Funnel.QuerySubmitted",
                              query_submitted_);
    base::UmaHistogramBoolean("Autofill.AtMemory.Funnel.SuggestionAccepted",
                              suggestion_accepted_);
    if (suggestion_accepted_) {
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
  base::UmaHistogramEnumeration("Autofill.AtMemory.Funnel.PopupDisplayed",
                                *source_);
}

void AtMemoryMetricsRecorder::OnQuerySubmitted(std::u16string query) {
  if (uploader_service_) {
    pending_log_entry_ =
        std::make_unique<optimization_guide::ModelQualityLogEntry>(
            uploader_service_->GetWeakPtr());
    optimization_guide::proto::AtMemoryQuality* quality =
        pending_log_entry_->log_ai_data_request()
            ->mutable_at_memory()
            ->mutable_quality();
    quality->set_query(base::UTF16ToUTF8(query));
    quality->set_url(url_.spec());
    quality->set_title(base::UTF16ToUTF8(title_));
    // Rely on log entry destructor to upload the log entry, so this will flush
    // when a new query comes in or the funnel metrics object gets destroyed.
  }

  query_submitted_ = true;
}

void AtMemoryMetricsRecorder::OnSuggestionAccepted() {
  suggestion_accepted_ = true;
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

}  // namespace autofill

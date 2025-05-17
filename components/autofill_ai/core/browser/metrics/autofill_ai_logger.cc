// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/metrics/autofill_ai_logger.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/metrics/autofill_ai_ukm_logger.h"

namespace autofill_ai {

namespace {

constexpr char funnel_histogram_prefix[] = "Autofill.Ai.Funnel.";

void LogFunnelMetric(std::string_view funnel_metric_name,
                     bool submission_state,
                     bool metric_value) {
  const std::string specific_histogram_name = base::StrCat(
      {funnel_histogram_prefix, submission_state ? "Submitted." : "Abandoned.",
       funnel_metric_name});
  const std::string aggregate_histogram_name =
      base::StrCat({funnel_histogram_prefix, "Aggregate.", funnel_metric_name});
  base::UmaHistogramBoolean(specific_histogram_name, metric_value);
  base::UmaHistogramBoolean(aggregate_histogram_name, metric_value);
}

}  // namespace

AutofillAiLogger::AutofillAiLogger(AutofillAiClient* client)
    : ukm_logger_(client) {}
AutofillAiLogger::~AutofillAiLogger() = default;

void AutofillAiLogger::OnFormEligibilityAvailable(
    autofill::FormGlobalId form_id,
    bool is_eligible) {
  form_states_[form_id].is_eligible = is_eligible;
}

void AutofillAiLogger::OnFormHasDataToFill(autofill::FormGlobalId form_id) {
  form_states_[form_id].has_data_to_fill = true;
}

void AutofillAiLogger::OnSuggestionsShown(const autofill::FormStructure& form,
                                          const autofill::AutofillField& field,
                                          ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()].suggestions_shown = true;
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field,
                            AutofillAiUkmLogger::EventType::kSuggestionShown);
}

void AutofillAiLogger::OnDidFillSuggestion(const autofill::FormStructure& form,
                                           const autofill::AutofillField& field,
                                           ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()].did_fill_suggestions = true;
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field,
                            AutofillAiUkmLogger::EventType::kSuggestionFilled);
}

void AutofillAiLogger::OnEditedAutofilledField(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field,
    ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()].edited_autofilled_field = true;
  ukm_logger_.LogFieldEvent(
      ukm_source_id, form, field,
      AutofillAiUkmLogger::EventType::kEditedAutofilledValue);
}

void AutofillAiLogger::OnDidFillField(const autofill::FormStructure& form,
                                      const autofill::AutofillField& field,
                                      ukm::SourceId ukm_source_id) {
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field,
                            AutofillAiUkmLogger::EventType::kFieldFilled);
}

void AutofillAiLogger::RecordFormMetrics(const autofill::FormStructure& form,
                                         ukm::SourceId ukm_source_id,
                                         bool submission_state,
                                         bool opt_in_status) {
  FunnelState state = form_states_[form.global_id()];
  if (submission_state) {
    base::UmaHistogramBoolean("Autofill.Ai.OptInStatus", opt_in_status);
    ukm_logger_.LogKeyMetrics(
        ukm_source_id, form, /*data_to_fill_available=*/state.has_data_to_fill,
        /*suggestions_shown=*/state.suggestions_shown,
        /*suggestion_filled=*/state.did_fill_suggestions,
        /*edited_autofilled_field=*/state.edited_autofilled_field,
        /*opt_in_status=*/opt_in_status);
  }

  LogFunnelMetric("Eligibility", submission_state, state.is_eligible);
  if (!state.is_eligible) {
    return;
  }
  LogFunnelMetric("ReadinessAfterEligibility", submission_state,
                  state.has_data_to_fill);
  if (!state.has_data_to_fill) {
    return;
  }
  LogFunnelMetric("FillAfterSuggestion", submission_state,
                  state.did_fill_suggestions);
  if (!state.did_fill_suggestions) {
    return;
  }
  LogFunnelMetric("CorrectionAfterFill", submission_state,
                  state.edited_autofilled_field);
}

}  // namespace autofill_ai

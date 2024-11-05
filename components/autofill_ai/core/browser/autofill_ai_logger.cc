// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_logger.h"

#include <string_view>

#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill_ai {

namespace {

constexpr char funnel_histogram_prefix[] = "Autofill.FormsAI.Funnel.";

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

AutofillAiLogger::AutofillAiLogger() = default;
AutofillAiLogger::~AutofillAiLogger() = default;

void AutofillAiLogger::OnFormEligibilityAvailable(
    autofill::FormGlobalId form_id,
    bool is_eligible) {
  form_states_[form_id].is_eligible = is_eligible;
}

void AutofillAiLogger::OnFormHasDataToFill(autofill::FormGlobalId form_id) {
  form_states_[form_id].has_data_to_fill = true;
}

void AutofillAiLogger::OnSuggestionsShown(autofill::FormGlobalId form_id) {
  form_states_[form_id].did_show_suggestions = true;
}

void AutofillAiLogger::OnTriggeredFillingSuggestions(
    autofill::FormGlobalId form_id) {
  form_states_[form_id].did_start_loading_suggestions = true;
}

void AutofillAiLogger::OnFillingSuggestionsShown(
    autofill::FormGlobalId form_id) {
  form_states_[form_id].did_show_filling_suggestions = true;
}

void AutofillAiLogger::OnDidFillSuggestion(autofill::FormGlobalId form_id) {
  form_states_[form_id].did_fill_suggestions = true;
}

void AutofillAiLogger::OnDidCorrectFillingSuggestion(
    autofill::FormGlobalId form_id) {
  form_states_[form_id].did_correct_filling = true;
}

void AutofillAiLogger::RecordMetricsForForm(autofill::FormGlobalId form_id,
                                            bool submission_state) {
  LogFunnelMetric("Eligibility", submission_state,
                  form_states_[form_id].is_eligible);
  if (!form_states_[form_id].is_eligible) {
    return;
  }
  LogFunnelMetric("ReadinessAfterEligibility", submission_state,
                  form_states_[form_id].has_data_to_fill);
  if (!form_states_[form_id].has_data_to_fill) {
    return;
  }
  LogFunnelMetric("SuggestionAfterReadiness", submission_state,
                  form_states_[form_id].did_show_suggestions);
  if (!form_states_[form_id].did_show_suggestions) {
    return;
  }
  LogFunnelMetric("LoadingAfterSuggestion", submission_state,
                  form_states_[form_id].did_start_loading_suggestions);
  if (!form_states_[form_id].did_start_loading_suggestions) {
    return;
  }
  LogFunnelMetric("FillingSuggestionAfterLoading", submission_state,
                  form_states_[form_id].did_show_filling_suggestions);
  if (!form_states_[form_id].did_show_filling_suggestions) {
    return;
  }
  LogFunnelMetric("FillAfterSuggestion", submission_state,
                  form_states_[form_id].did_fill_suggestions);
  if (!form_states_[form_id].did_fill_suggestions) {
    return;
  }
  LogFunnelMetric("CorrectionAfterFill", submission_state,
                  form_states_[form_id].did_correct_filling);
}

}  // namespace autofill_ai

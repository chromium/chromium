// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_logger.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_ukm_logger.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

constexpr std::string_view funnel_histogram_prefix = "Autofill.Ai.Funnel.";
constexpr std::string_view key_metric_histogram_prefix =
    "Autofill.Ai.KeyMetrics.";

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

void LogKeyMetric(std::string_view key_metric_name,
                  std::string_view entity_type,
                  bool metric_value) {
  const std::string generic_histogram_name =
      base::StrCat({key_metric_histogram_prefix, key_metric_name});
  base::UmaHistogramBoolean(generic_histogram_name, metric_value);
  if (!entity_type.empty()) {
    const std::string entity_specific_histogram =
        base::StrCat({generic_histogram_name, ".", entity_type});
    base::UmaHistogramBoolean(entity_specific_histogram, metric_value);
  }
}

}  // namespace

AutofillAiLogger::AutofillAiLogger(AutofillClient* client)
    : ukm_logger_(client) {}
AutofillAiLogger::~AutofillAiLogger() = default;

void AutofillAiLogger::OnFormEligibilityAvailable(FormGlobalId form_id,
                                                  bool is_eligible) {
  form_states_[form_id].is_eligible = is_eligible;
}

void AutofillAiLogger::OnFormHasDataToFill(FormGlobalId form_id) {
  form_states_[form_id].has_data_to_fill = true;
}

void AutofillAiLogger::OnSuggestionsShown(const FormStructure& form,
                                          const AutofillField& field,
                                          ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()].suggestions_shown = true;
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field,
                            AutofillAiUkmLogger::EventType::kSuggestionShown);
}

void AutofillAiLogger::OnDidFillSuggestion(const FormStructure& form,
                                           const AutofillField& field,
                                           ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()].did_fill_suggestions = true;
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field,
                            AutofillAiUkmLogger::EventType::kSuggestionFilled);
}

void AutofillAiLogger::OnEditedAutofilledField(const FormStructure& form,
                                               const AutofillField& field,
                                               ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()].edited_autofilled_field = true;
  ukm_logger_.LogFieldEvent(
      ukm_source_id, form, field,
      AutofillAiUkmLogger::EventType::kEditedAutofilledValue);
}

void AutofillAiLogger::OnDidFillField(const FormStructure& form,
                                      const AutofillField& field,
                                      ukm::SourceId ukm_source_id) {
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field,
                            AutofillAiUkmLogger::EventType::kFieldFilled);
}

void AutofillAiLogger::RecordFormMetrics(const FormStructure& form,
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
    if (opt_in_status) {
      RecordKeyMetrics(form, state);
    }
  }
  RecordFunnelMetrics(state, submission_state);
  RecordNumberOfFieldsFilled(form, state, opt_in_status);
}

void AutofillAiLogger::RecordFunnelMetrics(const FunnelState& funnel_state,
                                           bool submission_state) const {
  LogFunnelMetric("Eligibility", submission_state, funnel_state.is_eligible);
  if (!funnel_state.is_eligible) {
    return;
  }
  LogFunnelMetric("ReadinessAfterEligibility", submission_state,
                  funnel_state.has_data_to_fill);
  if (!funnel_state.has_data_to_fill) {
    return;
  }
  LogFunnelMetric("FillAfterSuggestion", submission_state,
                  funnel_state.did_fill_suggestions);
  if (!funnel_state.did_fill_suggestions) {
    return;
  }
  LogFunnelMetric("CorrectionAfterFill", submission_state,
                  funnel_state.edited_autofilled_field);
}

void AutofillAiLogger::RecordKeyMetrics(const FormStructure& form,
                                        const FunnelState& funnel_state) const {
  const std::string_view entity_type = [&] {
    for (const auto& [section, entities_and_fields] :
         DetermineAttributeTypes(form.fields())) {
      for (const auto& [entity, fields_and_types] : entities_and_fields) {
        switch (entity.name()) {
          case EntityTypeName::kPassport:
            return "Passport";
          case EntityTypeName::kDriversLicense:
            return "DriversLicense";
          case EntityTypeName::kVehicle:
            return "Vehicle";
        }
      }
    }
    return "";
  }();

  LogKeyMetric("FillingReadiness", entity_type, funnel_state.has_data_to_fill);
  LogKeyMetric("FillingAssistance", entity_type,
               funnel_state.did_fill_suggestions);
  if (funnel_state.suggestions_shown) {
    LogKeyMetric("FillingAcceptance", entity_type,
                 funnel_state.did_fill_suggestions);
  }
  if (funnel_state.did_fill_suggestions) {
    LogKeyMetric("FillingCorrectness", entity_type,
                 !funnel_state.edited_autofilled_field);
  }
}

void AutofillAiLogger::RecordNumberOfFieldsFilled(const FormStructure& form,
                                                  const FunnelState& state,
                                                  bool opt_in_status) const {
  const int num_filled_fields = std::ranges::count_if(
      form, [&](const std::unique_ptr<AutofillField>& field) {
        switch (field->filling_product()) {
          case FillingProduct::kAddress:
          case FillingProduct::kCreditCard:
          case FillingProduct::kMerchantPromoCode:
          case FillingProduct::kIban:
          case FillingProduct::kPassword:
          case FillingProduct::kPlusAddresses:
          case FillingProduct::kAutofillAi:
          case FillingProduct::kLoyaltyCard:
          case FillingProduct::kIdentityCredential:
            return true;
          case FillingProduct::kAutocomplete:
          case FillingProduct::kCompose:
          case FillingProduct::kDataList:
          case FillingProduct::kNone:
            return false;
        }
      });
  const int num_autofill_ai_filled_fields = std::ranges::count(
      form, FillingProduct::kAutofillAi, &AutofillField::filling_product);
  const std::string total_opt_in_histogram_name =
      base::StrCat({"Autofill.Ai.NumberOfFilledFields.Total.",
                    opt_in_status ? "OptedIn" : "OptedOut"});
  const std::string total_readiness_histogram_name =
      base::StrCat({"Autofill.Ai.NumberOfFilledFields.Total.",
                    state.has_data_to_fill ? "HasDataToFill" : "NoDataToFill"});
  base::UmaHistogramCounts100(total_opt_in_histogram_name, num_filled_fields);
  base::UmaHistogramCounts100(total_readiness_histogram_name,
                              num_filled_fields);

  if (opt_in_status) {
    base::UmaHistogramCounts100(
        "Autofill.Ai.NumberOfFilledFields.AutofillAi.OptedIn",
        num_autofill_ai_filled_fields);
  }
  if (state.has_data_to_fill) {
    base::UmaHistogramCounts100(
        "Autofill.Ai.NumberOfFilledFields.AutofillAi.HasDataToFill",
        num_autofill_ai_filled_fields);
  }
}

}  // namespace autofill

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_logger.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_ukm_logger.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

constexpr std::string_view funnel_histogram_prefix = "Autofill.Ai.Funnel.";
constexpr std::string_view key_metric_histogram_prefix =
    "Autofill.Ai.KeyMetrics.";

// LINT.IfChange(HistogramSuffixForEntityType)
std::string_view HistogramSuffixForEntityType(EntityType type) {
  switch (type.name()) {
    case EntityTypeName::kDriversLicense:
      return "DriversLicense";
    case EntityTypeName::kKnownTravelerNumber:
      return "KnownTravelerNumber";
    case EntityTypeName::kNationalIdCard:
      return "NationalIdCard";
    case EntityTypeName::kPassport:
      return "Passport";
    case EntityTypeName::kRedressNumber:
      return "RedressNumber";
    case EntityTypeName::kVehicle:
      return "Vehicle";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiEntityType)

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
  const std::string histogram_name = base::StrCat(
      {key_metric_histogram_prefix, key_metric_name, ".", entity_type});
  base::UmaHistogramBoolean(histogram_name, metric_value);
}

}  // namespace

AutofillAiLogger::AutofillAiLogger(AutofillClient* client)
    : ukm_logger_(client) {}
AutofillAiLogger::~AutofillAiLogger() {
  for (const auto& [form_id, states] : form_states_) {
    if (!submitted_forms_.contains(form_id)) {
      DenseSet<EntityType> relevant_entities(
          states, &std::pair<const EntityType, FunnelState>::first);
      RecordFunnelMetrics(states, relevant_entities,
                          /*submission_state=*/false);
    }
  }
}

void AutofillAiLogger::OnFormEligibilityAvailable(
    FormGlobalId form_id,
    DenseSet<EntityType> relevant_entities) {
  for (EntityType entity_type : relevant_entities) {
    form_states_[form_id][entity_type].is_eligible = true;
  }
}

void AutofillAiLogger::OnFormHasDataToFill(
    FormGlobalId form_id,
    DenseSet<EntityType> entities_to_fill) {
  for (EntityType entity_type : entities_to_fill) {
    form_states_[form_id][entity_type].has_data_to_fill = true;
  }
}

void AutofillAiLogger::OnSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    DenseSet<EntityType> suggested_entity_types,
    ukm::SourceId ukm_source_id) {
  for (EntityType type : suggested_entity_types) {
    form_states_[form.global_id()][type].suggestions_shown = true;
    ukm_logger_.LogFieldEvent(ukm_source_id, form, field, type,
                              AutofillAiUkmLogger::EventType::kSuggestionShown);
  }
}

void AutofillAiLogger::OnDidFillSuggestion(const FormStructure& form,
                                           const AutofillField& field,
                                           EntityType entity_type,
                                           ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()][entity_type].did_fill_suggestions = true;
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity_type,
                            AutofillAiUkmLogger::EventType::kSuggestionFilled);
}

void AutofillAiLogger::OnEditedAutofilledField(const FormStructure& form,
                                               const AutofillField& field,
                                               ukm::SourceId ukm_source_id) {
  auto it = last_filled_entity_.find(field.global_id());
  if (it == last_filled_entity_.end()) {
    return;
  }
  EntityType entity_type = it->second;
  form_states_[form.global_id()][entity_type].edited_autofilled_field = true;
  ukm_logger_.LogFieldEvent(
      ukm_source_id, form, field, entity_type,
      AutofillAiUkmLogger::EventType::kEditedAutofilledValue);
}

void AutofillAiLogger::OnDidFillField(const FormStructure& form,
                                      const AutofillField& field,
                                      EntityType entity_type,
                                      ukm::SourceId ukm_source_id) {
  last_filled_entity_.insert({field.global_id(), entity_type});
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity_type,
                            AutofillAiUkmLogger::EventType::kFieldFilled);
}

void AutofillAiLogger::RecordFormMetrics(const FormStructure& form,
                                         ukm::SourceId ukm_source_id,
                                         bool submission_state,
                                         bool opt_in_status) {
  if (submission_state) {
    submitted_forms_.insert(form.global_id());
  }
  const DenseSet<EntityType> relevant_entities =
      GetRelevantEntityTypesForFields(form.fields());
  if (relevant_entities.empty()) {
    return;
  }
  std::map<EntityType, FunnelState> states = form_states_[form.global_id()];
  if (submission_state) {
    using enum AutofillAiOptInStatus;
    base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Status",
                                  opt_in_status ? kOptedIn : kOptedOut);
    // TODO(crbug.com/408380915): Remove after M141.
    base::UmaHistogramBoolean("Autofill.Ai.OptInStatus", opt_in_status);

    for (const auto& [entity_type, state] : states) {
      ukm_logger_.LogKeyMetrics(ukm_source_id, form, entity_type,
                                state.has_data_to_fill, state.suggestions_shown,
                                state.did_fill_suggestions,
                                state.edited_autofilled_field, opt_in_status);
    }
    if (opt_in_status) {
      RecordKeyMetrics(relevant_entities, states);
    }
  }
  RecordFunnelMetrics(states, relevant_entities, submission_state);
  RecordNumberOfFieldsFilled(form, states, opt_in_status);
}

void AutofillAiLogger::RecordFunnelMetrics(
    const std::map<EntityType, FunnelState>& states,
    DenseSet<EntityType> relevant_entities,
    bool submission_state) const {
  for (EntityType entity_type : relevant_entities) {
    const std::string_view type_str = HistogramSuffixForEntityType(entity_type);
    base::UmaHistogramEnumeration(
        base::StrCat({"Autofill.Ai.Funnel.",
                      submission_state ? "Submitted" : "Abandoned",
                      ".Eligibility2"}),
        entity_type.name());
    base::UmaHistogramEnumeration("Autofill.Ai.Funnel.Aggregate.Eligibility2",
                                  entity_type.name());
    auto it = states.find(entity_type);
    if (it == states.end()) {
      continue;
    }
    const FunnelState& funnel_state = it->second;
    LogFunnelMetric(base::StrCat({"ReadinessAfterEligibility.", type_str}),
                    submission_state, funnel_state.has_data_to_fill);
    if (!funnel_state.has_data_to_fill) {
      continue;
    }
    LogFunnelMetric(base::StrCat({"SuggestionAfterReadiness.", type_str}),
                    submission_state, funnel_state.suggestions_shown);
    if (!funnel_state.suggestions_shown) {
      continue;
    }
    LogFunnelMetric(base::StrCat({"FillAfterSuggestion.", type_str}),
                    submission_state, funnel_state.did_fill_suggestions);
    if (!funnel_state.did_fill_suggestions) {
      continue;
    }
    LogFunnelMetric(base::StrCat({"CorrectionAfterFill.", type_str}),
                    submission_state, funnel_state.edited_autofilled_field);
  }
}

void AutofillAiLogger::RecordKeyMetrics(
    DenseSet<EntityType> relevant_entities,
    const std::map<EntityType, FunnelState>& states) const {
  for (EntityType entity_type : relevant_entities) {
    auto it = states.find(entity_type);
    if (it == states.end()) {
      // This means that the form mutated in a way such that it used to have
      // fields fillable with a certain `EntityType` and it now does not. Those
      // cases are gracefully ignored and not logged.
      continue;
    }
    const FunnelState& funnel_state = it->second;
    const std::string_view type_str = HistogramSuffixForEntityType(entity_type);
    LogKeyMetric("FillingReadiness", type_str, funnel_state.has_data_to_fill);
    LogKeyMetric("FillingAssistance", type_str,
                 funnel_state.did_fill_suggestions);
    if (funnel_state.suggestions_shown) {
      LogKeyMetric("FillingAcceptance", type_str,
                   funnel_state.did_fill_suggestions);
    }
    if (funnel_state.did_fill_suggestions) {
      LogKeyMetric("FillingCorrectness", type_str,
                   !funnel_state.edited_autofilled_field);
    }
  }
}

void AutofillAiLogger::RecordNumberOfFieldsFilled(
    const FormStructure& form,
    const std::map<EntityType, FunnelState>& states,
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
          case FillingProduct::kOneTimePassword:
            return true;
          case FillingProduct::kAutocomplete:
          case FillingProduct::kCompose:
          case FillingProduct::kDataList:
          case FillingProduct::kNone:
            return false;
        }
      });
  const bool has_data_to_fill =
      std::ranges::any_of(states, [&](const auto& type_and_state) {
        const auto& [type, state] = type_and_state;
        return state.has_data_to_fill;
      });
  const int num_autofill_ai_filled_fields = std::ranges::count(
      form, FillingProduct::kAutofillAi, &AutofillField::filling_product);
  const std::string total_opt_in_histogram_name =
      base::StrCat({"Autofill.Ai.NumberOfFilledFields.Total.",
                    opt_in_status ? "OptedIn" : "OptedOut"});
  const std::string total_readiness_histogram_name =
      base::StrCat({"Autofill.Ai.NumberOfFilledFields.Total.",
                    has_data_to_fill ? "HasDataToFill" : "NoDataToFill"});
  base::UmaHistogramCounts100(total_opt_in_histogram_name, num_filled_fields);
  base::UmaHistogramCounts100(total_readiness_histogram_name,
                              num_filled_fields);

  if (opt_in_status) {
    base::UmaHistogramCounts100(
        "Autofill.Ai.NumberOfFilledFields.AutofillAi.OptedIn",
        num_autofill_ai_filled_fields);
  }
  if (has_data_to_fill) {
    base::UmaHistogramCounts100(
        "Autofill.Ai.NumberOfFilledFields.AutofillAi.HasDataToFill",
        num_autofill_ai_filled_fields);
  }
}

}  // namespace autofill

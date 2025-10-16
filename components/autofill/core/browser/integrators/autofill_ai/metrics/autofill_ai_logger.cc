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
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_ukm_logger.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

void LogFunnelMetric(std::string_view funnel_metric_name,
                     std::string_view entity_type_name,
                     bool submission_state,
                     bool metric_value) {
  static constexpr std::string_view kFunnelHistogramMask =
      "Autofill.Ai.Funnel.%s.%s%s";
  // Emit both the `Aggregate` variant of the metric and the one corresponding
  // to the `submission_state`.
  for (std::string_view submission_state_str :
       {(submission_state ? "Submitted" : "Abandoned"), "Aggregate"}) {
    // Emit both the variant of the metric that corresponds to
    // `entity_type_name` and the one that is typeless in that sense.
    for (std::string entity_type_str :
         {base::StrCat({".", entity_type_name}), std::string()}) {
      base::UmaHistogramBoolean(
          base::StringPrintf(kFunnelHistogramMask, submission_state_str,
                             funnel_metric_name, entity_type_str),
          metric_value);
    }
  }
}

void LogKeyMetric(std::string_view key_metric_name,
                  std::string_view entity_type_name,
                  bool metric_value) {
  static constexpr std::string_view kKeyMetricsHistogramMask =
      "Autofill.Ai.KeyMetrics.%s%s";
  // Emit both the variant of the metric that corresponds to `entity_type_name`
  // and the one that is typeless in that sense.
  for (std::string entity_type_str :
       {base::StrCat({".", entity_type_name}), std::string()}) {
    base::UmaHistogramBoolean(
        base::StringPrintf(kKeyMetricsHistogramMask, key_metric_name,
                           entity_type_str),
        metric_value);
  }
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

void AutofillAiLogger::OnFormHasDataToFill(
    FormGlobalId form_id,
    DenseSet<EntityType> form_relevant_entity_types,
    base::span<const EntityInstance> stored_entities) {
  DenseSet<EntityType> stored_entity_types(stored_entities,
                                           &EntityInstance::type);
  for (EntityType type : form_relevant_entity_types) {
    form_states_[form_id][type].has_data_to_fill =
        stored_entity_types.contains(type);
  }
}

void AutofillAiLogger::OnSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    base::span<const EntityInstance* const> entities_suggested,
    ukm::SourceId ukm_source_id) {
  for (const EntityInstance* const entity : entities_suggested) {
    form_states_[form.global_id()][entity->type()].suggestions_shown = true;
    ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity->type(),
                              AutofillAiUkmLogger::EventType::kSuggestionShown);
  }
}

void AutofillAiLogger::OnDidFillSuggestion(const FormStructure& form,
                                           const AutofillField& field,
                                           const EntityInstance& entity_filled,
                                           ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()][entity_filled.type()].did_fill_suggestions =
      true;
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity_filled.type(),
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
                                      const EntityInstance& entity_filled,
                                      ukm::SourceId ukm_source_id) {
  last_filled_entity_.insert({field.global_id(), entity_filled.type()});
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity_filled.type(),
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
    base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Status.Submission",
                                  opt_in_status ? kOptedIn : kOptedOut);
    // TODO(crbug.com/408380915): Remove after M142.
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
    const std::string_view type_str = EntityTypeToMetricsString(entity_type);
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
    LogFunnelMetric("ReadinessAfterEligibility", type_str, submission_state,
                    funnel_state.has_data_to_fill);
    if (!funnel_state.has_data_to_fill) {
      continue;
    }
    LogFunnelMetric("SuggestionAfterReadiness", type_str, submission_state,
                    funnel_state.suggestions_shown);
    if (!funnel_state.suggestions_shown) {
      continue;
    }
    LogFunnelMetric("FillAfterSuggestion", type_str, submission_state,
                    funnel_state.did_fill_suggestions);
    if (!funnel_state.did_fill_suggestions) {
      continue;
    }
    LogFunnelMetric("CorrectionAfterFill", type_str, submission_state,
                    funnel_state.edited_autofilled_field);
  }
}

void AutofillAiLogger::OnSaveOrUpdatePromptResult(
    AutofillClient::AutofillAiPromptTypes prompt_type,
    EntityType entity_type,
    EntityInstance::RecordType record_type,
    uint64_t form_session_id,
    const std::string& domain,
    AutofillClient::EntitySaveOrUpdatePromptResult result,
    ukm::SourceId ukm_source_id) {
  ukm_logger_.LogSaveOrUpdatePromptResult(prompt_type, entity_type, record_type,
                                          form_session_id, domain, result,
                                          ukm_source_id);
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
    const std::string_view type_str = EntityTypeToMetricsString(entity_type);
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
          case FillingProduct::kPasskey:
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

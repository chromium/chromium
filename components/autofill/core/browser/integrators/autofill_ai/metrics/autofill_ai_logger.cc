// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_logger.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/map_util.h"
#include "base/metrics/histogram_functions.h"
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
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

void LogFunnelMetric(std::string_view funnel_metric_name,
                     std::optional<EntityType> entity_type,
                     std::optional<EntityInstance::RecordType> record_type,
                     bool submission_state,
                     bool metric_value) {
  // Only entity-type-specific histograms are split by record type.
  CHECK(!record_type || entity_type);

  static constexpr std::string_view kFunnelHistogramMask =
      "Autofill.Ai.Funnel.%s.%s%s%s";

  const std::string entity_type_str =
      entity_type ? base::StrCat({".", EntityTypeToMetricsString(*entity_type)})
                  : "";
  const std::string record_type_str =
      record_type
          ? base::StrCat({".", EntityRecordTypeToMetricsString(*record_type)})
          : "";

  // Emit both the `Aggregate` variant of the metric and the one corresponding
  // to the `submission_state`.
  for (std::string_view submission_state_str :
       {(submission_state ? "Submitted" : "Abandoned"), "Aggregate"}) {
    base::UmaHistogramBoolean(
        base::StringPrintf(kFunnelHistogramMask, submission_state_str,
                           funnel_metric_name, entity_type_str,
                           record_type_str),
        metric_value);
  }
}

void LogKeyMetric(std::string_view key_metric_name,
                  std::optional<EntityType> entity_type,
                  std::optional<EntityInstance::RecordType> record_type,
                  bool metric_value) {
  // Only entity-type-specific histograms are split by record type.
  CHECK(!record_type || entity_type);

  static constexpr std::string_view kKeyMetricsHistogramMask =
      "Autofill.Ai.KeyMetrics.%s%s%s";

  const std::string entity_type_str =
      entity_type ? base::StrCat({".", EntityTypeToMetricsString(*entity_type)})
                  : "";
  const std::string record_type_str =
      record_type
          ? base::StrCat({".", EntityRecordTypeToMetricsString(*record_type)})
          : "";

  base::UmaHistogramBoolean(
      base::StringPrintf(kKeyMetricsHistogramMask, key_metric_name,
                         entity_type_str, record_type_str),
      metric_value);
}

}  // namespace

AutofillAiLogger::AutofillAiLogger(AutofillClient* client)
    : ukm_logger_(client) {}
AutofillAiLogger::~AutofillAiLogger() {
  for (const auto& [form_id, states] : form_states_) {
    if (!submitted_forms_.contains(form_id)) {
      RecordFunnelMetrics(states, /*submission_state=*/false);
    }
  }
}

void AutofillAiLogger::OnFormHasDataToFill(
    FormGlobalId form_id,
    DenseSet<EntityType> form_relevant_entity_types,
    base::span<const EntityInstance> stored_entities) {
  std::map<EntityInstance::RecordType, DenseSet<EntityType>>
      entity_types_by_record_type;
  for (const EntityInstance& entity : stored_entities) {
    entity_types_by_record_type[entity.record_type()].insert(entity.type());
  }
  for (EntityType entity_type : form_relevant_entity_types) {
    for (EntityInstance::RecordType record_type :
         DenseSet<EntityInstance::RecordType>::all()) {
      form_states_[form_id][entity_type][record_type].has_data_to_fill =
          entity_types_by_record_type[record_type].contains(entity_type);
    }
  }
}

void AutofillAiLogger::OnSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    base::span<const EntityInstance* const> entities_suggested,
    ukm::SourceId ukm_source_id) {
  auto suggested_entity_types =
      base::MakeFlatSet<std::pair<EntityType, EntityInstance::RecordType>>(
          entities_suggested, /*comp=*/{},
          [](const EntityInstance* const entity) {
            return std::pair(entity->type(), entity->record_type());
          });
  for (const auto& [entity_type, record_type] : suggested_entity_types) {
    form_states_[form.global_id()][entity_type][record_type].suggestions_shown =
        true;
    ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity_type,
                              record_type,
                              AutofillAiUkmLogger::EventType::kSuggestionShown);
  }
}

void AutofillAiLogger::OnDidFillSuggestion(const FormStructure& form,
                                           const AutofillField& field,
                                           const EntityInstance& entity_filled,
                                           ukm::SourceId ukm_source_id) {
  form_states_[form.global_id()][entity_filled.type()]
              [entity_filled.record_type()]
                  .did_fill_suggestions = true;
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity_filled.type(),
                            entity_filled.record_type(),
                            AutofillAiUkmLogger::EventType::kSuggestionFilled);
}

void AutofillAiLogger::OnEditedAutofilledField(const FormStructure& form,
                                               const AutofillField& field,
                                               ukm::SourceId ukm_source_id) {
  const std::pair<EntityType, EntityInstance::RecordType>* last_filled_entity =
      base::FindOrNull(last_filled_entity_, field.global_id());
  if (!last_filled_entity) {
    return;
  }
  const auto& [entity_type, entity_record_type] = *last_filled_entity;
  form_states_[form.global_id()][entity_type][entity_record_type]
      .edited_autofilled_field = true;
  ukm_logger_.LogFieldEvent(
      ukm_source_id, form, field, entity_type, entity_record_type,
      AutofillAiUkmLogger::EventType::kEditedAutofilledValue);
}

void AutofillAiLogger::OnDidFillField(const FormStructure& form,
                                      const AutofillField& field,
                                      const EntityInstance& entity_filled,
                                      ukm::SourceId ukm_source_id) {
  last_filled_entity_.insert_or_assign(
      field.global_id(),
      std::pair(entity_filled.type(), entity_filled.record_type()));
  ukm_logger_.LogFieldEvent(ukm_source_id, form, field, entity_filled.type(),
                            entity_filled.record_type(),
                            AutofillAiUkmLogger::EventType::kFieldFilled);
}

void AutofillAiLogger::OnImportPromptResult(
    const FormData& form,
    AutofillClient::AutofillAiImportPromptType prompt_type,
    EntityType entity_type,
    EntityInstance::RecordType record_type,
    AutofillClient::AutofillAiBubbleClosedReason close_reason,
    ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.Ai.",
                    EntityPromptTypeToMetricsString(prompt_type), ".",
                    EntityTypeToMetricsString(entity_type), ".",
                    EntityRecordTypeToMetricsString(record_type)}),
      close_reason);
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.Ai.",
                    EntityPromptTypeToMetricsString(prompt_type), ".",
                    EntityTypeToMetricsString(entity_type)}),
      close_reason);
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.Ai.",
                    EntityPromptTypeToMetricsString(prompt_type),
                    ".AllEntities"}),
      close_reason);
  ukm_logger_.LogImportPromptResult(form, prompt_type, entity_type, record_type,
                                    close_reason, ukm_source_id);
}

void AutofillAiLogger::RecordFormMetrics(const FormStructure& form,
                                         ukm::SourceId ukm_source_id,
                                         bool submission_state,
                                         bool opt_in_status) {
  if (submission_state) {
    submitted_forms_.insert(form.global_id());
  }
  std::map<EntityType, std::map<EntityInstance::RecordType, FunnelState>>
      funnel_states = form_states_[form.global_id()];
  if (funnel_states.empty()) {
    return;
  }
  if (submission_state) {
    using enum AutofillAiOptInStatus;
    base::UmaHistogramEnumeration("Autofill.Ai.OptIn.Status.Submission",
                                  opt_in_status ? kOptedIn : kOptedOut);

    for (const auto& [entity_type, states] : funnel_states) {
      FunnelState combined_state = CombineStates(states);
      ukm_logger_.LogKeyMetrics(
          ukm_source_id, form, entity_type, combined_state.has_data_to_fill,
          combined_state.suggestions_shown, combined_state.did_fill_suggestions,
          combined_state.edited_autofilled_field, opt_in_status);
    }
    if (opt_in_status) {
      RecordKeyMetrics(funnel_states);
    }
  }
  RecordFunnelMetrics(funnel_states, submission_state);
  RecordNumberOfFieldsFilled(form, funnel_states, opt_in_status);
}

void AutofillAiLogger::RecordFunnelMetrics(
    const FormFunnelStateMap& funnel_states,
    bool submission_state) const {
  std::map<EntityType, FunnelState> record_type_agnostic_states;
  // `funnel_states` is a map of EntityType -> RecordType -> FunnelState.
  // Compress the RecordType dimension into an EntityType -> FunnelState map.
  for (const auto& [type, states] : funnel_states) {
    record_type_agnostic_states.insert({type, CombineStates(states)});
  }
  FunnelState combined_state = CombineStates(record_type_agnostic_states);

  RecordFunnelMetricsForState(combined_state, /*entity_type=*/std::nullopt,
                              /*record_type=*/std::nullopt, submission_state);

  for (const auto& [entity_type, states] : funnel_states) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Autofill.Ai.Funnel.",
                      submission_state ? "Submitted" : "Abandoned",
                      ".Eligibility2"}),
        entity_type.name());
    base::UmaHistogramEnumeration("Autofill.Ai.Funnel.Aggregate.Eligibility2",
                                  entity_type.name());

    RecordFunnelMetricsForState(record_type_agnostic_states[entity_type],
                                entity_type,
                                /*record_type=*/std::nullopt, submission_state);
    for (const auto& [record_type, state] : states) {
      RecordFunnelMetricsForState(state, entity_type, record_type,
                                  submission_state);
    }
  }
}

void AutofillAiLogger::RecordFunnelMetricsForState(
    FunnelState funnel_state,
    std::optional<EntityType> entity_type,
    std::optional<EntityInstance::RecordType> record_type,
    bool submission_state) const {
  LogFunnelMetric("ReadinessAfterEligibility", entity_type, record_type,
                  submission_state, funnel_state.has_data_to_fill);
  if (!funnel_state.has_data_to_fill) {
    return;
  }
  LogFunnelMetric("SuggestionAfterReadiness", entity_type, record_type,
                  submission_state, funnel_state.suggestions_shown);
  if (!funnel_state.suggestions_shown) {
    return;
  }
  LogFunnelMetric("FillAfterSuggestion", entity_type, record_type,
                  submission_state, funnel_state.did_fill_suggestions);
  if (!funnel_state.did_fill_suggestions) {
    return;
  }
  LogFunnelMetric("CorrectionAfterFill", entity_type, record_type,
                  submission_state, funnel_state.edited_autofilled_field);
}

void AutofillAiLogger::RecordKeyMetrics(
    const FormFunnelStateMap& funnel_states) const {
  std::map<EntityType, FunnelState> record_type_agnostic_states;
  // `funnel_states` is a map of EntityType -> RecordType -> FunnelState.
  // Compress the RecordType dimension into an EntityType -> FunnelState map.
  for (const auto& [type, states] : funnel_states) {
    record_type_agnostic_states.insert({type, CombineStates(states)});
  }
  FunnelState combined_state = CombineStates(record_type_agnostic_states);
  RecordKeyMetricsForState(combined_state, /*entity_type=*/std::nullopt,
                           /*record_type=*/std::nullopt);
  for (const auto& [entity_type, states] : funnel_states) {
    RecordKeyMetricsForState(record_type_agnostic_states.at(entity_type),
                             entity_type, /*record_type=*/std::nullopt);
    for (const auto& [record_type, state] : states) {
      RecordKeyMetricsForState(state, entity_type, record_type);
    }
  }
}

void AutofillAiLogger::RecordKeyMetricsForState(
    FunnelState funnel_state,
    std::optional<EntityType> entity_type,
    std::optional<EntityInstance::RecordType> record_type) const {
  LogKeyMetric("FillingReadiness", entity_type, record_type,
               funnel_state.has_data_to_fill);
  LogKeyMetric("FillingAssistance", entity_type, record_type,
               funnel_state.did_fill_suggestions);
  if (funnel_state.suggestions_shown) {
    LogKeyMetric("FillingAcceptance", entity_type, record_type,
                 funnel_state.did_fill_suggestions);
  }
  if (funnel_state.did_fill_suggestions) {
    LogKeyMetric("FillingCorrectness", entity_type, record_type,
                 !funnel_state.edited_autofilled_field);
  }
}

void AutofillAiLogger::RecordNumberOfFieldsFilled(
    const FormStructure& form,
    const FormFunnelStateMap& funnel_states,
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
      std::ranges::any_of(funnel_states, [&](const auto& type_and_states) {
        const auto& [type, states] = type_and_states;
        return CombineStates(states).has_data_to_fill;
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/studies/hats_surveys_util.h"

#include <memory>
#include <ranges>
#include <string>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// Converts `filling_stats` to a key-value representation, where the key
// is the "stats category" and the value is the number of fields that match
// such category. This is used to show users a survey that will measure the
// perception of Autofill.
HatsSurveyStringData FormFillingStatsToSurveyStringData(
    const autofill_metrics::FormGroupFillingStats& filling_stats) {
  return {
      {"Accepted fields", base::NumberToString(filling_stats.num_accepted)},
      {"Corrected to same type",
       base::NumberToString(filling_stats.num_corrected_to_same_type)},
      {"Corrected to a different type",
       base::NumberToString(filling_stats.num_corrected_to_different_type)},
      {"Corrected to an unknown type",
       base::NumberToString(filling_stats.num_corrected_to_unknown_type)},
      {"Corrected to empty",
       base::NumberToString(filling_stats.num_corrected_to_empty)},
      {"Manually filled to same type",
       base::NumberToString(filling_stats.num_manually_filled_to_same_type)},
      {"Manually filled to a different type",
       base::NumberToString(
           filling_stats.num_manually_filled_to_different_type)},
      {"Manually filled to an unknown type",
       base::NumberToString(filling_stats.num_manually_filled_to_unknown_type)},
      {"Total corrected", base::NumberToString(filling_stats.TotalCorrected())},
      {"Total filled", base::NumberToString(filling_stats.TotalFilled())},
      {"Total unfilled", base::NumberToString(filling_stats.TotalUnfilled())},
      {"Total manually filled",
       base::NumberToString(filling_stats.TotalManuallyFilled())},
      {"Total number of fields", base::NumberToString(filling_stats.Total())}};
}

size_t CountFormType(const FormStructure& form, FormType type) {
  return std::ranges::count_if(
      form.fields(), [type](const std::unique_ptr<AutofillField>& field) {
        return field->Type().GetFormTypes().contains(type);
      });
}

// Returns the product specific data (PSD) of the survey. If the conditions of
// the survey are not satisfied, returns `std::nullopt`.
std::optional<HatsSurveyStringData> GetUserPerceptionSurveyData(
    const FormStructure& submitted_form,
    FormType type,
    size_t min_number_of_fields_of_type,
    const base::Feature& feature) {
  const autofill_metrics::FormGroupFillingStats filling_stats =
      autofill_metrics::GetFormFillingStatsForFormType(type, submitted_form);

  if (CountFormType(submitted_form, type) >= min_number_of_fields_of_type &&
      filling_stats.TotalFilled() > 0 &&
      base::FeatureList::IsEnabled(feature)) {
    return FormFillingStatsToSurveyStringData(filling_stats);
  }
  return std::nullopt;
}

}  // namespace

void MaybeTriggerFormSubmissionHatsSurveys(
    AutofillClient& client,
    const FormStructure& submitted_form) {
  // Use the same minimum required number of fields for a user perception survey
  // that is required for running local heuristics. This makes the survey more
  // consistent with recorded UMA metrics that rely on a type prediction.
  if (std::optional<HatsSurveyStringData> survey_data =
          GetUserPerceptionSurveyData(
              submitted_form, FormType::kAddressForm,
              /*min_number_of_fields_of_type=*/kMinRequiredFieldsForHeuristics,
              features::kAutofillAddressUserPerceptionSurvey)) {
    client.TriggerUserPerceptionOfAutofillSurvey(FillingProduct::kAddress,
                                                 *survey_data);
    return;
  }

  if (std::optional<HatsSurveyStringData> survey_data =
          GetUserPerceptionSurveyData(
              submitted_form, FormType::kCreditCardForm,
              /*min_number_of_fields_of_type=*/1,
              features::kAutofillCreditCardUserPerceptionSurvey)) {
    client.TriggerUserPerceptionOfAutofillSurvey(FillingProduct::kCreditCard,
                                                 *survey_data);
    return;
  }
}

RecentUserAutofillAiInteractionsForHats::
    RecentUserAutofillAiInteractionsForHats() = default;
RecentUserAutofillAiInteractionsForHats::
    ~RecentUserAutofillAiInteractionsForHats() = default;

void RecentUserAutofillAiInteractionsForHats::SuggestionsShown(
    const FormStructure& form,
    const AutofillField& field) {
  auto it = user_suggestion_interactions_per_form_.Get(form.global_id());
  // Do not overwrite cases in which a suggestion was previously accepted.
  if (it == user_suggestion_interactions_per_form_.end() ||
      !it->second.entity_type_accepted) {
    user_suggestion_interactions_per_form_.Put(
        form.global_id(),
        InteractionDetails{
            .entity_type_accepted = std::nullopt,
            .accepted_entity_record_type = std::nullopt,
            .autofill_ai_field_types = field.Type().GetAutofillAiTypes(),
        });
  }
}

void RecentUserAutofillAiInteractionsForHats::SuggestionAccepted(
    const FormStructure& form,
    const EntityInstance& entity) {
  auto it = user_suggestion_interactions_per_form_.Get(form.global_id());
  if (it != user_suggestion_interactions_per_form_.end()) {
    it->second.entity_type_accepted = entity.type();
    it->second.accepted_entity_record_type = entity.record_type();
  }
}

std::optional<RecentUserAutofillAiInteractionsForHats::InteractionDetails>
RecentUserAutofillAiInteractionsForHats::GetRecentUserInteraction(
    FormGlobalId form_id) const {
  if (auto it = user_suggestion_interactions_per_form_.Peek(form_id);
      it != user_suggestion_interactions_per_form_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void MaybeTriggerAutofillAiSubmissionHatsSurveys(
    AutofillClient& client,
    const FormStructure& submitted_form,
    const RecentUserAutofillAiInteractionsForHats& suggestion_interactions) {
  std::optional<RecentUserAutofillAiInteractionsForHats::InteractionDetails>
      interaction_details = suggestion_interactions.GetRecentUserInteraction(
          submitted_form.global_id());
  if (!interaction_details) {
    return;
  }
  const EntityDataManager* entity_manager = client.GetEntityDataManager();
  if (!entity_manager) {
    return;
  }
  if (interaction_details->entity_type_accepted &&
      interaction_details->accepted_entity_record_type ==
          EntityInstance::RecordType::kPersonalContext) {
    auto saved_entity_type_names = base::MakeFlatSet<EntityTypeName>(
        entity_manager->GetEntityInstances(), std::less(),
        [](const EntityInstance& entity) { return entity.type().name(); });

    client.TriggerAutofillAiFillingJourneySurvey(
        /*suggestion_accepted=*/true,
        *interaction_details->entity_type_accepted, saved_entity_type_names,
        interaction_details->autofill_ai_field_types);
  }
}

}  // namespace autofill

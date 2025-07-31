// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autofill_ai/autofill_ai_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_suggestions.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"

namespace autofill {

AutofillAiSuggestionGenerator::AutofillAiSuggestionGenerator() = default;
AutofillAiSuggestionGenerator::~AutofillAiSuggestionGenerator() = default;

void AutofillAiSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form_data, field_data, form, field, client,
      [&callback](std::pair<FillingProduct,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void AutofillAiSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form_data, field_data, form, field, all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void AutofillAiSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  if (!MayPerformAutofillAiAction(client, AutofillAiAction::kFilling)) {
    callback({FillingProduct::kAutofillAi, {}});
    return;
  }

  const EntityDataManager* entity_manager = client.GetEntityDataManager();
  if (!entity_manager) {
    callback({FillingProduct::kAutofillAi, {}});
    return;
  }
  if (!field) {
    callback({FillingProduct::kAutofillAi, {}});
    return;
  }

  base::span<const EntityInstance> entities =
      entity_manager->GetEntityInstances();
  if (entities.empty()) {
    callback({FillingProduct::kAutofillAi, {}});
    return;
  }
  app_locale_ = client.GetAppLocale();

  // Sort entities based on their frecency.
  std::vector<const EntityInstance*> sorted_entities = base::ToVector(
      entities, [](const EntityInstance& entity) { return &entity; });
  std::ranges::sort(sorted_entities,
                    [comp = EntityInstance::FrecencyOrder(base::Time::Now())](
                        const EntityInstance* lhs, const EntityInstance* rhs) {
                      return comp(*lhs, *rhs);
                    });

  std::vector<SuggestionData> suggestion_data = base::ToVector(
      std::move(sorted_entities), [](const EntityInstance* entity) {
        return SuggestionData(std::move(*entity));
      });

  callback({FillingProduct::kAutofillAi, std::move(suggestion_data)});
}

void AutofillAiSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  if (!form || !field) {
    callback({FillingProduct::kAutofillAi, {}});
    return;
  }

  std::vector<SuggestionData> autofill_ai_suggestion_data =
      ExtractSuggestionDataForFillingProduct(all_suggestion_data,
                                             FillingProduct::kAutofillAi);

  std::vector<EntityInstance> entities = base::ToVector(
      std::move(autofill_ai_suggestion_data),
      [](SuggestionData& suggestion_data) {
        return std::get<autofill::EntityInstance>(std::move(suggestion_data));
      });

  std::vector<Suggestion> suggestions =
      CreateAutofillAiFillingSuggestions(*form, *field, entities, app_locale_);
  callback({FillingProduct::kAutofillAi, std::move(suggestions)});
}

}  // namespace autofill

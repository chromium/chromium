// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/entities/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/data_model/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/autofill_ai/core/browser/autofill_ai_logger.h"
#include "components/autofill_ai/core/browser/autofill_ai_utils.h"
#include "components/autofill_ai/core/browser/autofill_ai_value_filter.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_ai {

namespace {

using autofill::LogBuffer;
using autofill::LoggingScope;
using autofill::LogMessage;
using autofill::SuggestionType;

bool CheckIfEntitySatisfiesConstraints(const autofill::EntityInstance& entity) {
  autofill::DenseSet<autofill::AttributeType> attribute_types;
  for (const autofill::AttributeInstance& attribute_instance :
       entity.attributes()) {
    attribute_types.insert(attribute_instance.type());
  }

  return std::ranges::any_of(
      entity.type().import_constraints(),
      [&](const autofill::DenseSet<autofill::AttributeType>& constraint) {
        return attribute_types.contains_all(constraint);
      });
}

std::vector<autofill::EntityInstance> GetPossibleEntitiesFromSubmittedForm(
    const autofill::FormStructure& submitted_form) {
  std::map<
      autofill::Section,
      std::map<autofill::EntityType, std::vector<autofill::AttributeInstance>>>
      section_to_entity_types_attributes;
  for (const std::unique_ptr<autofill::AutofillField>& field :
       submitted_form.fields()) {
    std::optional<autofill::FieldType> autofill_ai_server_prediction =
        field->GetAutofillAiServerTypePredictions();
    if (!autofill_ai_server_prediction) {
      continue;
    }
    std::optional<autofill::AttributeType> field_attribute_type =
        autofill::AttributeType::FromFieldType(*autofill_ai_server_prediction);
    CHECK(field_attribute_type);
    // TODO(crbug.com/389629676): Save data format.
    std::u16string value = field->value(autofill::ValueSemantics::kCurrent);
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
    if (value.empty()) {
      continue;
    }

    section_to_entity_types_attributes[field->section()][field_attribute_type
                                                             ->entity_type()]
        .emplace_back(*field_attribute_type, value,
                      autofill::AttributeInstance::Context{});
  }

  std::vector<autofill::EntityInstance> entities_found_in_form;
  for (auto& [section, entity_to_attributes] :
       section_to_entity_types_attributes) {
    for (auto& [entity_name, attributes] : entity_to_attributes) {
      autofill::EntityInstance entity = autofill::EntityInstance(
          autofill::EntityType(entity_name), std::move(attributes),
          base::Uuid::GenerateRandomV4(), /*nickname=*/std::string(""),
          base::Time::Now());
      if (!CheckIfEntitySatisfiesConstraints(entity)) {
        continue;
      }
      entities_found_in_form.push_back(std::move(entity));
    }
  }

  return entities_found_in_form;
}

// Returns true if `entity` cannot be merged in any of the `current_entities`
// nor is a subset of any of them. This means that a save prompt should be
// displayed.
bool ShouldShowNewEntitySavePrompt(
    const autofill::EntityInstance& entity,
    base::span<const autofill::EntityInstance> current_entities) {
  return std::ranges::none_of(
      current_entities, [&](const autofill::EntityInstance& existing_entity) {
        // Entities of different type should not be merged.
        if (entity.type() != existing_entity.type()) {
          return false;
        }
        autofill::EntityInstance::EntityMergeability mergeability =
            existing_entity.GetEntityMergeability(entity);
        // If `entity` can be merged into `existing_entity`, a save prompt
        // should not be shown.
        if (!mergeability.mergeable_attributes.empty()) {
          return true;
        }
        // If `entity` is a subset of another entity, we should also not show a
        // save prompt.
        if (mergeability.is_subset) {
          return true;
        }
        return false;
      });
}

// Finds an entity in `current_entities` which `entity` can be merged into.
// Returns both the updated entity and the original entity.
// Returns `std::nullopt` if no suitable entity is found.
std::optional<std::pair<autofill::EntityInstance, autofill::EntityInstance>>
MaybeUpdateEntity(const autofill::EntityInstance& entity,
                  base::span<const autofill::EntityInstance> current_entities) {
  for (const autofill::EntityInstance& existing_entity : current_entities) {
    // Entities of different type should not be merged.
    if (entity.type() != existing_entity.type()) {
      continue;
    }
    autofill::EntityInstance::EntityMergeability mergeability =
        existing_entity.GetEntityMergeability(entity);
    if (mergeability.mergeable_attributes.empty()) {
      continue;
    }

    // Merges attributes into `existing_entity` and returns an updated entity
    // that contains both existing and new attributes.
    std::vector<autofill::AttributeInstance> new_attributes =
        base::ToVector(mergeability.mergeable_attributes);
    for (autofill::AttributeInstance curr_attribute :
         existing_entity.attributes()) {
      new_attributes.emplace_back(std::move(curr_attribute));
    }
    return std::make_pair(autofill::EntityInstance(
                              existing_entity.type(), std::move(new_attributes),
                              existing_entity.guid(),
                              existing_entity.nickname(), base::Time::Now()),
                          existing_entity);
  }
  return std::nullopt;
}

}  // namespace

AutofillAiManager::AutofillAiManager(AutofillAiClient* client,
                                     autofill::StrikeDatabase* strike_database)
    : client_(CHECK_DEREF(client)) {
  user_annotation_prompt_strike_database_ =
      strike_database
          ? std::make_unique<
                AutofillPrectionImprovementsAnnotationPromptStrikeDatabase>(
                strike_database)
          : nullptr;
}

bool AutofillAiManager::IsFormBlockedForImport(
    const autofill::FormStructure& form) const {
  if (!user_annotation_prompt_strike_database_) {
    return true;
  }

  return user_annotation_prompt_strike_database_->ShouldBlockFeature(
      AutofillAiAnnotationPromptStrikeDatabaseTraits::GetId(
          form.form_signature()));
}
void AutofillAiManager::AddStrikeForImportFromForm(
    const autofill::FormStructure& form) {
  if (!user_annotation_prompt_strike_database_) {
    return;
  }

  user_annotation_prompt_strike_database_->AddStrike(
      AutofillAiAnnotationPromptStrikeDatabaseTraits::GetId(
          form.form_signature()));
}

void AutofillAiManager::RemoveStrikesForImportFromForm(
    const autofill::FormStructure& form) {
  if (!user_annotation_prompt_strike_database_) {
    return;
  }

  user_annotation_prompt_strike_database_->ClearStrikes(
      AutofillAiAnnotationPromptStrikeDatabaseTraits::GetId(
          form.form_signature()));
}

base::flat_map<autofill::FieldGlobalId, bool>
AutofillAiManager::GetFieldValueSensitivityMap(
    const autofill::FormData& form_data) {
  autofill::FormStructure* form_structure =
      client_->GetCachedFormStructure(form_data.global_id());

  if (!form_structure) {
    return base::flat_map<autofill::FieldGlobalId, bool>();
  }

  FilterSensitiveValues(*form_structure);

  return base::MakeFlatMap<autofill::FieldGlobalId, bool>(
      form_structure->fields(), {}, [](const auto& field) {
        return std::make_pair(
            field->global_id(),
            field->value_identified_as_potentially_sensitive());
      });
}

AutofillAiManager::~AutofillAiManager() = default;

bool AutofillAiManager::IsEligibleForAutofillAi(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field) const {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWithDataSchema)) {
    // TODO(crbug.com/389629573): If triggering via manual fallback, the check
    // `field.GetAutofillAiServerTypePredictions()` does not apply.
    return field.GetAutofillAiServerTypePredictions() &&
           client_->IsAutofillAiEnabledPref() && IsUserEligible();
  }
  return false;
}

bool AutofillAiManager::IsUserEligible() const {
  return client_->IsUserEligible();
}

void AutofillAiManager::OnReceivedAXTree(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  client_->GetModelExecutor()->GetPredictions(
      form, /*field_eligibility_map*/ {}, GetFieldValueSensitivityMap(form),
      std::move(ax_tree_update), base::DoNothing());
}

void AutofillAiManager::OnSuggestionsShown(
    const autofill::DenseSet<SuggestionType>& shown_suggestion_types,
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    UpdateSuggestionsCallback update_suggestions_callback) {
  if (shown_suggestion_types.contains(SuggestionType::kFillAutofillAi)) {
    logger_.OnFillingSuggestionsShown(form.global_id());
  }
}

void AutofillAiManager::OnFormSeen(const autofill::FormStructure& form) {
  bool is_eligible = IsFormEligibleForFilling(form);
  logger_.OnFormEligibilityAvailable(form.global_id(), is_eligible);
  if (!is_eligible) {
    return;
  }

  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }
  if (entity_manager->GetEntityInstances().empty()) {
    return;
  }
  // TODO(crbug.com/389629573): We should check whether any of `entities`
  // can actually fill a field in the `form`, not only whether entities
  // exist.
  logger_.OnFormHasDataToFill(form.global_id());
}

void AutofillAiManager::OnDidFillSuggestion(autofill::FormGlobalId form_id) {
  logger_.OnDidFillSuggestion(form_id);
}

void AutofillAiManager::OnEditedAutofilledField(
    autofill::FormGlobalId form_id) {
  logger_.OnDidCorrectFillingSuggestion(form_id);
}

void AutofillAiManager::MaybeImportForm(
    std::unique_ptr<autofill::FormStructure> form,
    base::OnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                            bool autofill_ai_shows_bubble)> autofill_callback) {
  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    // The import is skipped because the entity data manager service is not
    // available.
    LOG_AF(GetCurrentLogManager())
        << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
        << "Entity data manager is not available";

    std::move(autofill_callback).Run(std::move(form), false);
    return;
  }
  std::vector<autofill::EntityInstance> entity_instances_from_form =
      GetPossibleEntitiesFromSubmittedForm(*form);
  if (entity_instances_from_form.empty()) {
    std::move(autofill_callback).Run(std::move(form), false);
    return;
  }

  std::vector<autofill::EntityInstance> current_entities =
      entity_manager->GetEntityInstances();
  std::ranges::sort(entity_instances_from_form,
                    autofill::EntityInstance::ImportOrder);

  for (autofill::EntityInstance& entity : entity_instances_from_form) {
    if (ShouldShowNewEntitySavePrompt(entity, current_entities)) {
      client_->ShowSaveAutofillAiBubble(
          std::move(entity),
          /*old_entity=*/std::nullopt,
          BindOnce(&AutofillAiManager::OnSavePromptAcceptance, GetWeakPtr()));
      std::move(autofill_callback).Run(std::move(form), true);
      return;
    } else if (std::optional<std::pair<autofill::EntityInstance,
                                       autofill::EntityInstance>>
                   maybe_entity_to_update =
                       MaybeUpdateEntity(entity, current_entities)) {
      client_->ShowSaveAutofillAiBubble(
          std::move(maybe_entity_to_update->first),
          std::move(maybe_entity_to_update->second),
          BindOnce(&AutofillAiManager::OnSavePromptAcceptance, GetWeakPtr()));
      std::move(autofill_callback).Run(std::move(form), true);
      return;
    }
  }
  std::move(autofill_callback).Run(std::move(form), false);
}

void AutofillAiManager::OnSavePromptAcceptance(
    AutofillAiClient::SavePromptAcceptanceResult result) {
  if (!result.entity) {
    return;
  }
  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }
  entity_manager->AddOrUpdateEntityInstance(*std::move(result.entity));
}

void AutofillAiManager::GetSuggestions(autofill::FormGlobalId form_global_id,
                                       autofill::FieldGlobalId field_global_id,
                                       bool is_manual_fallback,
                                       GetSuggestionsCallback callback) {
  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return std::move(callback).Run({});
  }

  const std::vector<autofill::EntityInstance>& entities =
      entity_manager->GetEntityInstances();
  if (entities.empty()) {
    std::move(callback).Run({});
    return;
  }

  autofill::FormStructure* form_structure =
      client_->GetCachedFormStructure(form_global_id);
  if (!form_structure) {
    std::move(callback).Run({});
    return;
  }

  const autofill::AutofillField* autofill_field =
      form_structure->GetFieldById(field_global_id);
  if (!autofill_field) {
    std::move(callback).Run({});
    return;
  }

  if (is_manual_fallback &&
      !autofill_field->GetAutofillAiServerTypePredictions()) {
    // TODO(crbug.com/389629573): Store `form`, `field` and trigger LLM.
    // Once we have LLM responses we need to rebuild the suggestions and
    // reshow the popup, either via `AutofillClient` or exposing the
    // update method from `AutofillExternalDelegate.
    std::move(callback).Run({CreateLoadingSuggestions()});
    return;
  }

  CHECK(autofill_field->GetAutofillAiServerTypePredictions());
  std::move(callback).Run(
      CreateFillingSuggestions(*form_structure, field_global_id, entities));
}

bool AutofillAiManager::ShouldDisplayIph(
    const autofill::AutofillField& field) const {
  // Iph can be shown if:
  // 1. The pref is off.
  // 2. The user can access the feature (for example the experiment flag is on).
  // 3. The focused form can trigger the feature.
  return !client_->IsAutofillAiEnabledPref() && IsUserEligible() &&
         field.GetAutofillAiServerTypePredictions();
}

autofill::LogManager* AutofillAiManager::GetCurrentLogManager() {
  return client_->GetAutofillClient().GetCurrentLogManager();
}

base::WeakPtr<AutofillAiManager> AutofillAiManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill_ai

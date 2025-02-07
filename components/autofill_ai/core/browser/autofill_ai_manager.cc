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
#include "components/optimization_guide/core/optimization_guide_decider.h"
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

// TODO(crbug.com/389629676): Remove this in favour of an implementation that
// lives in EntityType (or similar), as a way to avoid missing new entities.
static constexpr auto kEntitiesOrderedByImportPreference =
    std::array{autofill::EntityType(autofill::EntityTypeName::kPassport),
               autofill::EntityType(autofill::EntityTypeName::kLoyaltyCard)};

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
        .emplace_back(*field_attribute_type, base::UTF16ToUTF8(value),
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
// Returns `std::nullopt` if no suitable entity is found.
std::optional<autofill::EntityInstance> MaybeUpdateEntity(
    const autofill::EntityInstance& entity,
    base::span<const autofill::EntityInstance> current_entities) {
  for (const autofill::EntityInstance& existing_entity : current_entities) {
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
    return autofill::EntityInstance(
        existing_entity.type(), std::move(new_attributes),
        existing_entity.guid(), existing_entity.nickname(), base::Time::Now());
  }
  return std::nullopt;
}

}  // namespace

AutofillAiManager::AutofillAiManager(
    AutofillAiClient* client,
    optimization_guide::OptimizationGuideDecider* decider,
    autofill::StrikeDatabase* strike_database)
    : client_(CHECK_DEREF(client)), decider_(decider) {
  if (decider_) {
    decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::
             AUTOFILL_PREDICTION_IMPROVEMENTS_ALLOWLIST});
  }

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

bool AutofillAiManager::HasAutofillAiDataForField(
    const autofill::FormFieldData& field) {
  return cache_ && cache_->contains(field.global_id());
}

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
      std::move(ax_tree_update),
      base::BindOnce(&AutofillAiManager::OnReceivedPredictions,
                     weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
}

void AutofillAiManager::OnReceivedPredictions(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    AutofillAiModelExecutor::PredictionsOrError predictions_or_error,
    std::optional<std::string> model_execution_id) {
  LOG_AF(GetCurrentLogManager())
      << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
      << "Received predictions:" <<
      [&] {
        LogBuffer buffer;
        if (!predictions_or_error.has_value()) {
          buffer << "Error";
          return buffer;
        }
        buffer << autofill::Tag{"table"};
        for (const auto& [field_id, prediction] :
             predictions_or_error.value()) {
          buffer << autofill::Tr{} << field_id << prediction.value;
        }
        buffer << autofill::CTag{"table"};
        return buffer;
      }();

  if (predictions_or_error.has_value()) {
    prediction_retrieval_state_ = PredictionRetrievalState::kDoneSuccess;
    cache_ = std::move(predictions_or_error.value());
  } else {
    prediction_retrieval_state_ = PredictionRetrievalState::kDoneError;
  }

  // Depending on whether predictions where retrieved or not, we need to show
  // the corresponding suggestions. This is delayed a little bit so that we
  // don't see a flickering UI.
  loading_suggestion_timer_.Start(
      FROM_HERE, kMinTimeToShowLoading,
      base::BindRepeating(
          &AutofillAiManager::UpdateSuggestionsAfterReceivedPredictions,
          weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
}

void AutofillAiManager::UpdateSuggestionsAfterReceivedPredictions(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field) {
  switch (prediction_retrieval_state_) {
    case PredictionRetrievalState::kDoneSuccess:
      if (HasAutofillAiDataForField(trigger_field)) {
        UpdateSuggestions(CreateFillingSuggestions(
            *client_, *cache_, form, trigger_field, autofill_suggestions_));
      } else {
        OnFailedToGenerateSuggestions();
      }
      break;
    case PredictionRetrievalState::kDoneError:
      OnFailedToGenerateSuggestions();
      break;
    case PredictionRetrievalState::kReady:
    case PredictionRetrievalState::kIsLoadingPredictions:
      NOTREACHED();
  }
}

// TODO(crbug.com/362468426): Rename this method to
// `UserClickedManagePredictionsImprovements()`.
void AutofillAiManager::UserClickedLearnMore() {
  client_->OpenAutofillAiSettings();
}

void AutofillAiManager::OnSuggestionsShown(
    const autofill::DenseSet<SuggestionType>& shown_suggestion_types,
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    UpdateSuggestionsCallback update_suggestions_callback) {
  logger_.OnSuggestionsShown(form.global_id());
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
  entity_manager->LoadEntityInstances(base::BindOnce(
      [](base::WeakPtr<AutofillAiManager> self, autofill::FormGlobalId form_id,
         std::vector<autofill::EntityInstance> entities) {
        if (entities.empty()) {
          return;
        }
        // TODO(crbug.com/389629573): We should check whether any of `entities`
        // can actually fill a field in the `form`, not only whether entities
        // exist.
        self->logger_.OnFormHasDataToFill(form_id);
      },
      GetWeakPtr(), form.global_id()));
}

void AutofillAiManager::OnDidFillSuggestion(autofill::FormGlobalId form_id) {
  logger_.OnDidFillSuggestion(form_id);
}

void AutofillAiManager::OnEditedAutofilledField(
    autofill::FormGlobalId form_id) {
  logger_.OnDidCorrectFillingSuggestion(form_id);
}

void AutofillAiManager::Reset() {
  cache_ = std::nullopt;
  last_queried_form_global_id_ = std::nullopt;
  update_suggestions_callback_ = base::NullCallback();
  loading_suggestion_timer_.Stop();
  prediction_retrieval_state_ = PredictionRetrievalState::kReady;
}

void AutofillAiManager::UpdateSuggestions(
    const std::vector<autofill::Suggestion>& suggestions) {
  loading_suggestion_timer_.Stop();
  if (update_suggestions_callback_.is_null()) {
    return;
  }
  update_suggestions_callback_.Run(
      suggestions, autofill::AutofillSuggestionTriggerSource::kAutofillAi);
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

  entity_manager->LoadEntityInstances(base::BindOnce(
      [](base::WeakPtr<AutofillAiManager> self,
         std::unique_ptr<autofill::FormStructure> form,
         std::vector<autofill::EntityInstance> entity_instances_from_form,
         base::OnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                                 bool autofill_ai_shows_bubble)>
             autofill_callback,
         std::vector<autofill::EntityInstance> current_entities) {
        if (!self) {
          std::move(autofill_callback).Run(std::move(form), false);
          return;
        }
        for (const autofill::EntityType& entity_type :
             kEntitiesOrderedByImportPreference) {
          for (autofill::EntityInstance& entity :
               entity_instances_from_form) {
            if (entity.type() != entity_type) {
              continue;
            }

            if (ShouldShowNewEntitySavePrompt(entity, current_entities)) {
              self->client_->ShowSaveAutofillAiBubble(
                  std::move(entity),
                  BindOnce(&AutofillAiManager::OnSavePromptAcceptance, self,
                           AutofillAiManager::EntityUpdateType::kSave));
              std::move(autofill_callback).Run(std::move(form), true);
              return;
            } else if (std::optional<autofill::EntityInstance>
                           maybe_entity_to_update =
                               MaybeUpdateEntity(entity, current_entities)) {
              self->client_->ShowSaveAutofillAiBubble(
                  std::move(*maybe_entity_to_update),
                  BindOnce(&AutofillAiManager::OnSavePromptAcceptance, self,
                           AutofillAiManager::EntityUpdateType::kUpdate));
              std::move(autofill_callback).Run(std::move(form), true);
              return;
            }
          }
        }
        std::move(autofill_callback).Run(std::move(form), false);
      },
      GetWeakPtr(), std::move(form), std::move(entity_instances_from_form),
      std::move(autofill_callback)));
}

void AutofillAiManager::OnSavePromptAcceptance(
    AutofillAiManager::EntityUpdateType update_type,
    AutofillAiClient::SavePromptAcceptanceResult result) {
  if (!result.prompt_was_accepted) {
    return;
  }

  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }
  CHECK(result.entity);
  if (update_type == AutofillAiManager::EntityUpdateType::kSave) {
    entity_manager->AddEntityInstance(*result.entity);
  } else if (update_type == AutofillAiManager::EntityUpdateType::kUpdate) {
    entity_manager->UpdateEntityInstance(*result.entity);
  }
}

void AutofillAiManager::GetSuggestionsV2(
    autofill::FormGlobalId form_global_id,
    autofill::FieldGlobalId field_global_id,
    bool is_manual_fallback,
    GetSuggestionsCallback callback) {
  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return std::move(callback).Run({});
  }

  entity_manager->LoadEntityInstances(base::BindOnce(
      [](base::WeakPtr<AutofillAiManager> self,
         autofill::FormGlobalId form_global_id,
         autofill::FieldGlobalId field_global_id, bool is_manual_fallback,
         GetSuggestionsCallback callback,
         std::vector<autofill::EntityInstance> entities) {
        if (entities.empty()) {
          std::move(callback).Run({});
          return;
        }

        autofill::FormStructure* form_structure =
            self->client_->GetCachedFormStructure(form_global_id);
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
        std::move(callback).Run(CreateFillingSuggestionsV2(
            *form_structure, field_global_id, entities));
      },
      GetWeakPtr(), form_global_id, field_global_id, is_manual_fallback,
      std::move(callback)));
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

void AutofillAiManager::GoToSettings() const {
  client_->OpenAutofillAiSettings();
}

void AutofillAiManager::OnFailedToGenerateSuggestions() {
  if (!autofill_suggestions_.empty()) {
    // Fallback to regular autofill suggestions if any instead of showing an
    // error directly.
    UpdateSuggestions(autofill_suggestions_);
    return;
  }
  // TODO(crbug.com/370693653): Also add logic to fallback to autocomplete
  // suggestions if possible.
  switch (prediction_retrieval_state_) {
    case PredictionRetrievalState::kReady:
    case PredictionRetrievalState::kIsLoadingPredictions:
      NOTREACHED();
    case PredictionRetrievalState::kDoneSuccess:
      UpdateSuggestions(CreateNoInfoSuggestions());
      break;
    case PredictionRetrievalState::kDoneError:
      UpdateSuggestions(CreateErrorSuggestions());
      break;
  }
}

autofill::LogManager* AutofillAiManager::GetCurrentLogManager() {
  return client_->GetAutofillClient().GetCurrentLogManager();
}

base::WeakPtr<AutofillAiManager> AutofillAiManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill_ai

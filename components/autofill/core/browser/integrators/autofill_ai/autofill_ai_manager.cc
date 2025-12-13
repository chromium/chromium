// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/types/zip.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_import_utils.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_logger.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_attribute.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_update_strike_database.h"
#include "components/autofill/core/browser/suggestions/autofill_ai/autofill_ai_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strike_database/strike_database.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

bool DidUserExplicitlyDeclineImportPrompt(
    AutofillClient::AutofillAiBubbleClosedReason close_reason) {
  switch (close_reason) {
    case AutofillClient::AutofillAiBubbleClosedReason::kCancelled:
    case AutofillClient::AutofillAiBubbleClosedReason::kClosed:
      return true;
    case AutofillClient::AutofillAiBubbleClosedReason::kUnknown:
    case AutofillClient::AutofillAiBubbleClosedReason::kAccepted:
    case AutofillClient::AutofillAiBubbleClosedReason::kNotInteracted:
    case AutofillClient::AutofillAiBubbleClosedReason::kLostFocus:
      return false;
  }
}

// Given an `entity`, returns the string to use as a strike key for each entry
// in `entity.type().strike_keys()`.
std::vector<std::string> GetAttributeStrikeKeys(const EntityInstance& entity,
                                                const std::string& app_locale) {
  auto value_for_strike_key = [&](DenseSet<AttributeType> types) {
    // A list of (attribute_type_name, attribute_value) pairs.
    std::vector<std::pair<std::string, std::string>> key_value_pairs =
        base::ToVector(types, [&](AttributeType attribute_type) {
          base::optional_ref<const AttributeInstance> attribute =
              entity.attribute(attribute_type);
          return std::pair(
              std::string(attribute_type.name_as_string()),
              attribute
                  ? base::UTF16ToUTF8(attribute->GetCompleteInfo(app_locale))
                  : std::string());
        });

    // We sort the keys to ensure they remain stable even if the ordering in
    // the DenseSet changes.
    std::ranges::sort(key_value_pairs);

    // Now join them to create a strike key of the following format:
    // "attribute_type_name1;attribute_value1;attribute_type_name2;..."
    std::vector<std::string> string_pieces;
    string_pieces.reserve(2 * key_value_pairs.size());
    for (auto& [key, value] : key_value_pairs) {
      string_pieces.emplace_back(std::move(key));
      string_pieces.emplace_back(std::move(value));
    }
    // Hash the result to avoid storing potentially sensitive data unencrypted
    // on the disk.
    return base::NumberToString(
        StrToHash64Bit(base::JoinString(string_pieces, ";")));
  };

  return base::ToVector(entity.type().strike_keys(), value_for_strike_key);
}

base::flat_set<EntityTypeName> GetSaveEntitiesTypesNames(
    base::span<const EntityInstance> saved_entities) {
  base::flat_set<EntityTypeName> entity_types;
  for (const EntityInstance& entity : saved_entities) {
    entity_types.insert(entity.type().name());
  }
  return entity_types;
}

}  // namespace

AutofillAiManager::EntityImportPromptCandidate::EntityImportPromptCandidate(
    AutofillClient::AutofillAiImportPromptType prompt_type,
    const EntityInstance& candidate_entity)
    : prompt_type(prompt_type), candidate_entity(candidate_entity) {}

AutofillAiManager::EntityImportPromptCandidate::EntityImportPromptCandidate(
    const EntityImportPromptCandidate&) = default;

AutofillAiManager::EntityImportPromptCandidate::EntityImportPromptCandidate(
    EntityImportPromptCandidate&&) = default;

AutofillAiManager::EntityImportPromptCandidate&
AutofillAiManager::EntityImportPromptCandidate::operator=(
    const EntityImportPromptCandidate&) = default;

AutofillAiManager::EntityImportPromptCandidate&
AutofillAiManager::EntityImportPromptCandidate::operator=(
    EntityImportPromptCandidate&&) = default;

AutofillAiManager::EntityImportPromptCandidate::~EntityImportPromptCandidate() =
    default;

AutofillAiManager::AutofillAiManager(
    AutofillClient* client,
    strike_database::StrikeDatabaseBase* strike_database)
    : client_(CHECK_DEREF(client)) {
  if (strike_database) {
    save_strike_db_by_attribute_ =
        std::make_unique<AutofillAiSaveStrikeDatabaseByAttribute>(
            strike_database);
    save_strike_db_by_host_ =
        std::make_unique<AutofillAiSaveStrikeDatabaseByHost>(strike_database);
    update_strike_db_ =
        std::make_unique<AutofillAiUpdateStrikeDatabase>(strike_database);
  }
}

AutofillAiManager::~AutofillAiManager() = default;

void AutofillAiManager::OnSuggestionsShown(
    const FormStructure& form,
    const AutofillField& field,
    base::span<const Suggestion> shown_suggestions,
    ukm::SourceId ukm_source_id) {
  std::vector<const EntityInstance*> entities_suggested;
  for (const Suggestion& suggestion : shown_suggestions) {
    if (const auto* payload =
            std::get_if<Suggestion::AutofillAiPayload>(&suggestion.payload)) {
      if (base::optional_ref<const EntityInstance> entity =
              client_->GetEntityDataManager()->GetEntityInstance(
                  payload->guid)) {
        entities_suggested.push_back(entity.as_ptr());
      }
    }
  }
  logger_.OnSuggestionsShown(form, field, entities_suggested, ukm_source_id);

  auto it = user_suggestion_interactions_per_form_.Get(form.global_id());
  // Do not overwrite cases in which a suggestion was previously accepted.
  if (it == user_suggestion_interactions_per_form_.end() ||
      !it->second.entity_type_accepted) {
    user_suggestion_interactions_per_form_.Put(
        {form.global_id(),
         {.suggested_entity_types =
              DenseSet<EntityType>(entities_suggested, &EntityInstance::type),
          .entity_type_accepted = std::nullopt,
          .autofill_ai_field_types = field.Type().GetAutofillAiTypes()}});
  }
}

void AutofillAiManager::OnFormSeen(const FormStructure& form) {
  const DenseSet<EntityType> relevant_entities =
      GetRelevantEntityTypesForFields(form.fields());
  const EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (relevant_entities.empty() || !entity_manager) {
    return;
  }
  logger_.OnFormHasDataToFill(form.global_id(), relevant_entities,
                              entity_manager->GetEntityInstances());
}

void AutofillAiManager::OnDidFillSuggestion(
    const EntityInstance& entity,
    const FormStructure& form,
    const AutofillField& trigger_field,
    base::span<const AutofillField* const> filled_fields,
    ukm::SourceId ukm_source_id) {
  logger_.OnDidFillSuggestion(form, trigger_field, entity, ukm_source_id);
  for (const AutofillField* const field : filled_fields) {
    logger_.OnDidFillField(form, CHECK_DEREF(field), entity, ukm_source_id);
  }
  EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }
  entity_manager->RecordEntityUsed(entity.guid(), base::Time::Now());
  auto it = user_suggestion_interactions_per_form_.Get(form.global_id());
  if (it != user_suggestion_interactions_per_form_.end()) {
    it->second.entity_type_accepted = entity.type();
  }
}

void AutofillAiManager::OnEditedAutofilledField(const FormStructure& form,
                                                const AutofillField& field,
                                                ukm::SourceId ukm_source_id) {
  logger_.OnEditedAutofilledField(form, field, ukm_source_id);
}

bool AutofillAiManager::OnFormSubmitted(const FormStructure& form,
                                        ukm::SourceId ukm_source_id) {
  logger_.RecordFormMetrics(form, ukm_source_id, /*submission_state=*/true,
                            GetAutofillAiOptInStatus(*client_));
  // There are a few prompt/import scenarios a user can find, depending on
  // whether they are have 1p availability and the data entered in `form`.
  //
  // 1. The user submits a form with an entity that cannot be deduplicated with
  //    saved data. If the user has Wallet enabled, they will be offered to save
  //    to Wallet. Otherwise, they will be offered to save the data locally.
  // 2. The user submits a form that either contains a superset of a saved
  //    entity or fulfills matching criteria with a saved entity. In this case,
  //    the user will see an update prompt. If the user has Wallet enabled, the
  //    data is written into Wallet and, the original is deleted it if was
  //    local. Otherwise, the update is written to local data.
  // 3. The user submits a form that contains a subset of a locally saved
  //    entity. If the user has Wallet enabled and the resulting entity is not a
  //    duplicate of data saved in Wallet, a save prompt to Wallet is shown. On
  //    acceptance, the local entity is removed.
  const bool form_imported = MaybeImportForm(form, ukm_source_id);
  // Importing a form can already lead to a survey, therefore only show the
  // filling hats survey if no prompt was shown.
  if (!form_imported) {
    auto it = user_suggestion_interactions_per_form_.Get(form.global_id());
    if (it == user_suggestion_interactions_per_form_.end()) {
      return false;
    }
    const EntityDataManager* entity_manager = client_->GetEntityDataManager();
    if (!entity_manager) {
      LOG_AF(GetCurrentLogManager())
          << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
          << "Entity data manager is not available";
      return {};
    }
    if (it->second.entity_type_accepted) {
      client_->TriggerAutofillAiFillingJourneySurvey(
          /*suggestion_accepted=*/true, it->second.entity_type_accepted.value(),
          GetSaveEntitiesTypesNames(entity_manager->GetEntityInstances()),
          it->second.autofill_ai_field_types);
    } else {
      CHECK(!it->second.suggested_entity_types.empty());
      // Normally only one entity type is shown to users. However, in the case
      // where more than one type is shown and the user did not accept the
      // suggestion, use the first type as the survey type.
      client_->TriggerAutofillAiFillingJourneySurvey(
          /*suggestion_accepted=*/false,
          *(it->second.suggested_entity_types.begin()),
          GetSaveEntitiesTypesNames(entity_manager->GetEntityInstances()),
          it->second.autofill_ai_field_types);
    }
  }
  return form_imported;
}

bool AutofillAiManager::MaybeImportForm(const FormStructure& form,
                                        ukm::SourceId ukm_source_id) {
  std::vector<EntityImportPromptCandidate> prompt_candidates =
      GetEntityPromptCandidates(form);

  bool prompt_shown = false;
  for (const auto& [prompt_type, prompt_candidate] : prompt_candidates) {
    base::UmaHistogramBoolean(
        base::StringPrintf("Autofill.Ai.PromptSuppression.%s.%s",
                           EntityPromptTypeToMetricsString(prompt_type),
                           EntityTypeToMetricsString(prompt_candidate.type())),
        prompt_shown);

    if (prompt_shown) {
      continue;
    }

    prompt_shown = true;
    AutofillClient::EntityImportPromptResultCallback prompt_result_callback =
        BindOnce(&AutofillAiManager::HandlePromptResult, GetWeakPtr(),
                 form.ToFormData(), prompt_candidate, ukm_source_id,
                 prompt_type);

    client_->ShowEntityImportBubble(
        std::move(prompt_candidate),
        prompt_type == AutofillClient::AutofillAiImportPromptType::kUpdate
            ? std::optional(*client_->GetEntityDataManager()->GetEntityInstance(
                  prompt_candidate.guid()))
            : std::nullopt,
        std::move(prompt_result_callback));
  }
  return prompt_shown;
}

void AutofillAiManager::HandlePromptResult(
    const FormData& form,
    EntityInstance entity,
    ukm::SourceId ukm_source_id,
    AutofillClient::AutofillAiImportPromptType prompt_type,
    AutofillClient::AutofillAiBubbleClosedReason close_reason) {
  logger_.OnImportPromptResult(form, prompt_type, entity.type(),
                               entity.record_type(), close_reason,
                               ukm_source_id);
  EntityDataManager& entity_manager =
      CHECK_DEREF(client_->GetEntityDataManager());

  AddOrClearImportPromptStrikes(prompt_type, close_reason, form.url(), entity);

  const bool prompt_accepted =
      close_reason == AutofillClient::AutofillAiBubbleClosedReason::kAccepted;

  // This switch should handle prompt-type-specific logic.
  switch (prompt_type) {
    case AutofillClient::AutofillAiImportPromptType::kSave:
      client_->TriggerAutofillAiSavePromptSurvey(
          prompt_accepted, entity.type(),
          GetSaveEntitiesTypesNames(entity_manager.GetEntityInstances()));
      break;
    case AutofillClient::AutofillAiImportPromptType::kUpdate:
    case AutofillClient::AutofillAiImportPromptType::kMigrate:
      break;
  }

  // Only add logic that is common across all prompt types below this line.
  // Otherwise use the switch above.

  if (prompt_accepted) {
    entity_manager.AddOrUpdateEntityInstance(std::move(entity));
  }
}

std::vector<Suggestion> AutofillAiManager::GetSuggestions(
    const FormStructure& form,
    const FormFieldData& trigger_field) {
  AutofillAiSuggestionGenerator suggestion_generator;
  std::vector<Suggestion> suggestions;
  const AutofillField* autofill_field =
      form.GetFieldById(trigger_field.global_id());

  auto on_suggestion_data_returned =
      [&form, &autofill_field, &trigger_field, &suggestions, this,
       &suggestion_generator](
          std::pair<SuggestionGenerator::SuggestionDataSource,
                    std::vector<SuggestionGenerator::SuggestionData>>
              suggestion_data) {
        suggestion_generator.GenerateSuggestions(
            form.ToFormData(), trigger_field, &form, autofill_field, *client_,
            {std::move(suggestion_data)},
            [&suggestions](
                SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
              suggestions = std::move(returned_suggestions.second);
            });
      };

  // Since the `on_suggestion_data_returned` callback is called synchronously,
  // we can assume that `suggestions` will hold correct value.
  suggestion_generator.FetchSuggestionData(form.ToFormData(), trigger_field,
                                           &form, autofill_field, *client_,
                                           on_suggestion_data_returned);
  return suggestions;
}

bool AutofillAiManager::ShouldDisplayIph(const FormStructure& form,
                                         FieldGlobalId field_id) const {
  // This early return is just a performance optimization:
  // AutofillAiAction::kIphForOptIn requires an EntityType, which we don't know
  // at this point yet. Since kIphForOptIn is a stronger requirement than
  // kOptIn, we can check kOptIn first.
  if (!MayPerformAutofillAiAction(*client_, AutofillAiAction::kOptIn)) {
    return false;
  }

  // The user must have at least one address or payments instrument to indicate
  // that they are an active Autofill user.
  const AddressDataManager& adm =
      client_->GetPersonalDataManager().address_data_manager();
  const PaymentsDataManager& paydm =
      client_->GetPersonalDataManager().payments_data_manager();
  if (adm.GetProfiles().empty() && paydm.GetCreditCards().empty() &&
      paydm.GetIbans().empty() && !paydm.HasEwalletAccounts() &&
      !paydm.HasMaskedBankAccounts() &&
      !base::FeatureList::IsEnabled(
          features::
              kAutofillAiIgnoreWhetherUserHasAddressOrPaymentsDataForIph)) {
    return false;
  }
  const AutofillField* const focused_field = form.GetFieldById(field_id);
  if (!focused_field) {
    return false;
  }

  // We want to show IPH if filling the `focused_field` and fields that belong
  // to the same entity leads to an import.
  std::map<EntityType, DenseSet<AttributeType>> attributes_in_form;
  for (auto [entity, fields_and_types] : RationalizeAndDetermineAttributeTypes(
           form.fields(), focused_field->section())) {
    if (base::Contains(fields_and_types, focused_field->global_id(),
                       [](const AutofillFieldWithAttributeType& f) {
                         return f.field->global_id();
                       }) &&
        MayPerformAutofillAiAction(*client_, AutofillAiAction::kIphForOptIn,
                                   entity)) {
      attributes_in_form[entity].insert_all(
          DenseSet(fields_and_types, &AutofillFieldWithAttributeType::type));
    }
  }

  return std::ranges::any_of(attributes_in_form, [](const auto& p) {
    return AttributesMeetImportConstraints(p.first, p.second);
  });
}

LogManager* AutofillAiManager::GetCurrentLogManager() {
  return client_->GetCurrentLogManager();
}

void AutofillAiManager::AddOrClearImportPromptStrikes(
    AutofillClient::AutofillAiImportPromptType prompt_type,
    AutofillClient::AutofillAiBubbleClosedReason close_reason,
    const GURL& url,
    const EntityInstance& entity) {
  switch (prompt_type) {
    case AutofillClient::AutofillAiImportPromptType::kSave:
    case AutofillClient::AutofillAiImportPromptType::kMigrate:
      if (close_reason ==
          AutofillClient::AutofillAiBubbleClosedReason::kAccepted) {
        ClearStrikesForSave(url, entity);
      } else if (DidUserExplicitlyDeclineImportPrompt(close_reason)) {
        AddStrikeForSaveAttempt(url, entity);
      }
      break;
    case AutofillClient::AutofillAiImportPromptType::kUpdate:
      if (close_reason ==
          AutofillClient::AutofillAiBubbleClosedReason::kAccepted) {
        ClearStrikesForUpdate(entity.guid());
      } else if (DidUserExplicitlyDeclineImportPrompt(close_reason)) {
        AddStrikeForUpdateAttempt(entity.guid());
      }
      break;
  }
}

void AutofillAiManager::AddStrikeForSaveAttempt(const GURL& url,
                                                const EntityInstance& entity) {
  if (save_strike_db_by_host_ && url.is_valid() && url.has_host()) {
    save_strike_db_by_host_->AddStrike(
        AutofillAiSaveStrikeDatabaseByHost::GetId(
            entity.type().name_as_string(), url.GetHost()));
  }
  if (save_strike_db_by_attribute_) {
    for (const std::string& key :
         GetAttributeStrikeKeys(entity, client_->GetAppLocale())) {
      save_strike_db_by_attribute_->AddStrike(key);
    }
  }
}

void AutofillAiManager::AddStrikeForUpdateAttempt(
    const EntityInstance::EntityId& entity_uuid) {
  if (update_strike_db_) {
    update_strike_db_->AddStrike(*entity_uuid);
  }
}

void AutofillAiManager::ClearStrikesForSave(const GURL& url,
                                            const EntityInstance& entity) {
  if (save_strike_db_by_host_ && url.is_valid() && url.has_host()) {
    save_strike_db_by_host_->ClearStrikes(
        AutofillAiSaveStrikeDatabaseByHost::GetId(
            entity.type().name_as_string(), url.GetHost()));
  }
  if (save_strike_db_by_attribute_) {
    for (const std::string& key :
         GetAttributeStrikeKeys(entity, client_->GetAppLocale())) {
      save_strike_db_by_attribute_->ClearStrikes(key);
    }
  }
}

void AutofillAiManager::ClearStrikesForUpdate(
    const EntityInstance::EntityId& entity_uuid) {
  if (update_strike_db_) {
    update_strike_db_->ClearStrikes(*entity_uuid);
  }
}

bool AutofillAiManager::IsSaveBlockedByStrikeDatabase(
    const GURL& url,
    const EntityInstance& entity) const {
  if (!save_strike_db_by_host_ ||
      save_strike_db_by_host_->ShouldBlockFeature(
          AutofillAiSaveStrikeDatabaseByHost::GetId(
              entity.type().name_as_string(), url.GetHost()))) {
    return true;
  }

  if (!save_strike_db_by_attribute_ ||
      std::ranges::any_of(
          GetAttributeStrikeKeys(entity, client_->GetAppLocale()),
          [&](const std::string& key) {
            return save_strike_db_by_attribute_->ShouldBlockFeature(key);
          })) {
    return true;
  }

  return false;
}

bool AutofillAiManager::IsUpdateBlockedByStrikeDatabase(
    const EntityInstance::EntityId& entity_uuid) const {
  return !update_strike_db_ ||
         update_strike_db_->ShouldBlockFeature(*entity_uuid);
}

base::WeakPtr<AutofillAiManager> AutofillAiManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::vector<AutofillAiManager::EntityImportPromptCandidate>
AutofillAiManager::GetEntityPromptCandidates(const FormStructure& form) {
  const EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    LOG_AF(GetCurrentLogManager())
        << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
        << "Entity data manager is not available";
    return {};
  }
  base::span<const EntityInstance> saved_entities =
      entity_manager->GetEntityInstances();
  std::vector<EntityInstance> observed_entities =
      GetPossibleEntitiesFromSubmittedForm(form.fields(), *client_);
  std::erase_if(observed_entities, [&](const EntityInstance& entity) {
    return !MayPerformAutofillAiAction(*client_, AutofillAiAction::kImport,
                                       entity.type());
  });
  std::ranges::sort(observed_entities, EntityInstance::ImportOrder);

  std::vector<std::vector<std::optional<EntityInstance::EntityMergeability>>>
      mergeabilities = base::ToVector(
          observed_entities, [&](const EntityInstance& observed_entity) {
            return base::ToVector(
                saved_entities, [&](const EntityInstance& saved_entity) {
                  return saved_entity.type() == observed_entity.type()
                             ? std::optional(saved_entity.GetEntityMergeability(
                                   observed_entity))
                             : std::nullopt;
                });
          });

  std::vector<EntityImportPromptCandidate> prompt_candidates;

  // Populate the list of candidates, adding save, update and migrate candidates
  // in that order, given that this is the order of priority of those prompts
  // relative to each other.
  base::Extend(prompt_candidates,
               GetSavePromptCandidates(observed_entities, saved_entities, form,
                                       mergeabilities));
  base::Extend(prompt_candidates,
               GetUpdatePromptCandidates(observed_entities, saved_entities,
                                         mergeabilities));
  base::Extend(prompt_candidates, GetMigratePromptCandidates(
                                      observed_entities, saved_entities, form));

  return prompt_candidates;
}

std::vector<AutofillAiManager::EntityImportPromptCandidate>
AutofillAiManager::GetSavePromptCandidates(
    base::span<const EntityInstance> observed_entities,
    base::span<const EntityInstance> saved_entities,
    const FormStructure& form,
    const std::vector<
        std::vector<std::optional<EntityInstance::EntityMergeability>>>&
        entities_mergeabilities) const {
  std::vector<EntityImportPromptCandidate> save_candidates;
  for (const auto [observed_entity, mergeabilities] :
       base::zip(observed_entities, entities_mergeabilities)) {
    // Discard `observed_entity` if it is a subset of some saved entity.
    if (std::ranges::any_of(
            mergeabilities,
            [](const std::optional<EntityInstance::EntityMergeability>&
                   mergeability) {
              return mergeability && mergeability->is_subset;
            })) {
      continue;
    }
    // If `observed_entity` is not mergeable with any saved entity, we should
    // show a save prompt for it.
    if (std::ranges::all_of(
            mergeabilities,
            [](const std::optional<EntityInstance::EntityMergeability>&
                   mergeability) {
              return !mergeability ||
                     mergeability->mergeable_attributes.empty();
            }) &&
        !IsSaveBlockedByStrikeDatabase(form.source_url(), observed_entity)) {
      save_candidates.emplace_back(
          AutofillClient::AutofillAiImportPromptType::kSave, observed_entity);
    }
  }
  return save_candidates;
}

std::vector<AutofillAiManager::EntityImportPromptCandidate>
AutofillAiManager::GetUpdatePromptCandidates(
    base::span<const EntityInstance> observed_entities,
    base::span<const EntityInstance> saved_entities,
    const std::vector<
        std::vector<std::optional<EntityInstance::EntityMergeability>>>&
        entities_mergeabilities) const {
  std::vector<EntityImportPromptCandidate> update_candidates;
  for (const auto [observed_entity, mergeabilities] :
       base::zip(observed_entities, entities_mergeabilities)) {
    // Discard `observed_entity` if it is a subset of some saved entity.
    if (std::ranges::any_of(
            mergeabilities,
            [](const std::optional<EntityInstance::EntityMergeability>&
                   mergeability) {
              return mergeability && mergeability->is_subset;
            })) {
      continue;
    }

    // For each saved entity that is mergeable with `observed_entity`, we should
    // add an update prompt candidate.
    for (auto [mergeability, saved_entity] :
         base::zip(mergeabilities, saved_entities)) {
      if (!mergeability || mergeability->mergeable_attributes.empty() ||
          saved_entity.are_attributes_read_only() ||
          IsUpdateBlockedByStrikeDatabase(saved_entity.guid())) {
        continue;
      }
      // Do not update a server entity into a local entity.
      if (saved_entity.record_type() ==
              EntityInstance::RecordType::kServerWallet &&
          observed_entity.record_type() == EntityInstance::RecordType::kLocal) {
        continue;
      }
      // This will contain the attributes of the new to-be-updated entity.
      base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
          new_attributes = std::move(mergeability->mergeable_attributes);
      for (const AttributeInstance& curr_attribute :
           saved_entity.attributes()) {
        // Only add the attributes of the saved entity that weren't mergeable
        // with the observed entity. The other attributes were added by
        // `mergeable_attributes`.
        // Note that `base::flat_set::insert` does exactly that.
        new_attributes.insert(curr_attribute);
      }
      update_candidates.emplace_back(
          AutofillClient::AutofillAiImportPromptType::kUpdate,
          EntityInstance(saved_entity.type(), std::move(new_attributes),
                         saved_entity.guid(), saved_entity.nickname(),
                         base::Time::Now(), saved_entity.use_count(),
                         base::Time::Now(), observed_entity.record_type(),
                         EntityInstance::AreAttributesReadOnly(false),
                         /*frecency_override=*/""));
    }
  }
  return update_candidates;
}

std::vector<AutofillAiManager::EntityImportPromptCandidate>
AutofillAiManager::GetMigratePromptCandidates(
    base::span<const EntityInstance> observed_entities,
    base::span<const EntityInstance> saved_entities,
    const FormStructure& form) const {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Autofill.Ai.Timing.GetEntityUpstreamCandidateFromSubmittedForm");

  std::vector<const EntityInstance*> saved_local_entities;
  std::vector<const EntityInstance*> saved_server_entities;
  for (const EntityInstance& entity : saved_entities) {
    switch (entity.record_type()) {
      case EntityInstance::RecordType::kLocal:
        //  Do not add entity types that cannot be upstreamed.
        if (MayPerformAutofillAiAction(
                *client_, AutofillAiAction::kImportToWallet, entity.type())) {
          saved_local_entities.push_back(&entity);
        }
        break;
      case EntityInstance::RecordType::kServerWallet:
        saved_server_entities.push_back(&entity);
        break;
    }
  }

  // Keep only local entities that are not a subset of a server entity,
  // otherwise they would be duplicated on the server.
  std::erase_if(saved_local_entities, [&](const EntityInstance* local_entity) {
    return std::ranges::any_of(
        saved_server_entities, [&](const EntityInstance* server_entity) {
          return local_entity->IsSubsetOf(*server_entity);
        });
  });
  // Prioritize recently used entities.
  std::ranges::sort(saved_local_entities,
                    [](const EntityInstance* lhs, const EntityInstance* rhs) {
                      return EntityInstance::MigrationOrder(*lhs, *rhs);
                    });

  std::vector<EntityImportPromptCandidate> migrate_candidates;
  for (const EntityInstance& observed_entity : observed_entities) {
    // Note that the migration prompt uses the regular save prompt strike
    // database.
    if (IsSaveBlockedByStrikeDatabase(form.source_url(), observed_entity)) {
      continue;
    }

    for (const EntityInstance* local_entity : saved_local_entities) {
      if (local_entity->type() == observed_entity.type() &&
          observed_entity.IsSubsetOf(*local_entity)) {
        migrate_candidates.emplace_back(
            AutofillClient::AutofillAiImportPromptType::kMigrate,
            EntityInstance(
                local_entity->type(),
                base::ToVector(local_entity->attributes()),
                local_entity->guid(), local_entity->nickname(),
                local_entity->date_modified(), local_entity->use_count(),
                local_entity->use_date(),
                EntityInstance::RecordType::kServerWallet,
                // Entities that are migrated from local to server are never
                // read-only, since local entities can always be edited by the
                // users, so can their server counterpart.
                EntityInstance::AreAttributesReadOnly(false),
                /*frecency_override=*/""));
      }
    }
  }

  return migrate_candidates;
}
}  // namespace autofill

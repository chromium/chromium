// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

#include <algorithm>
#include <memory>
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
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_attribute.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_update_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
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
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_import_utils.h"
#include "components/autofill_ai/core/browser/autofill_ai_utils.h"
#include "components/autofill_ai/core/browser/metrics/autofill_ai_logger.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_ai {

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AutofillAiSaveStrikeDatabaseByHost;
using autofill::AutofillClient;
using autofill::AutofillField;
using autofill::DenseSet;
using autofill::EntityInstance;
using autofill::EntityType;
using autofill::FieldType;
using autofill::FormStructure;
using autofill::LogBuffer;
using autofill::LoggingScope;
using autofill::LogMessage;
using autofill::SuggestionType;

// Returns true if `entity` cannot be merged in any of the `current_entities`
// nor is a subset of any of them. This means that a save prompt should be
// displayed.
bool ShouldShowNewEntitySavePrompt(
    const EntityInstance& entity,
    base::span<const EntityInstance> current_entities) {
  return std::ranges::none_of(
      current_entities, [&](const EntityInstance& existing_entity) {
        // Entities of different type should not be merged.
        if (entity.type() != existing_entity.type()) {
          return false;
        }
        EntityInstance::EntityMergeability mergeability =
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
std::optional<std::pair<EntityInstance, EntityInstance>> MaybeUpdateEntity(
    const EntityInstance& entity,
    base::span<const EntityInstance> current_entities) {
  for (const EntityInstance& existing_entity : current_entities) {
    // Entities of different type should not be merged.
    if (entity.type() != existing_entity.type()) {
      continue;
    }
    EntityInstance::EntityMergeability mergeability =
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
    return std::make_pair(
        EntityInstance(existing_entity.type(), std::move(new_attributes),
                       existing_entity.guid(), existing_entity.nickname(),
                       base::Time::Now(), existing_entity.use_count(),
                       base::Time::Now()),
        existing_entity);
  }
  return std::nullopt;
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
        autofill::StrToHash64Bit(base::JoinString(string_pieces, ";")));
  };

  return base::ToVector(entity.type().strike_keys(), value_for_strike_key);
}

}  // namespace

AutofillAiManager::AutofillAiManager(AutofillAiClient* client,
                                     autofill::StrikeDatabase* strike_database)
    : client_(CHECK_DEREF(client)) {
  if (strike_database) {
    save_strike_db_by_attribute_ =
        std::make_unique<autofill::AutofillAiSaveStrikeDatabaseByAttribute>(
            strike_database);
    save_strike_db_by_host_ =
        std::make_unique<AutofillAiSaveStrikeDatabaseByHost>(strike_database);
    update_strike_db_ =
        std::make_unique<autofill::AutofillAiUpdateStrikeDatabase>(
            strike_database);
  }
}

AutofillAiManager::~AutofillAiManager() = default;

void AutofillAiManager::OnSuggestionsShown(const autofill::FormStructure& form,
                                           const autofill::AutofillField& field,
                                           ukm::SourceId ukm_source_id) {
  logger_.OnSuggestionsShown(form, field, ukm_source_id);
}

void AutofillAiManager::OnFormSeen(const FormStructure& form) {
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

void AutofillAiManager::OnDidFillSuggestion(
    const base::Uuid& guid,
    const autofill::FormStructure& form,
    const autofill::AutofillField& trigger_field,
    base::span<const autofill::AutofillField* const> filled_fields,
    ukm::SourceId ukm_source_id) {
  logger_.OnDidFillSuggestion(form, trigger_field, ukm_source_id);
  for (const autofill::AutofillField* const field : filled_fields) {
    logger_.OnDidFillField(form, CHECK_DEREF(field), ukm_source_id);
  }
  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }
  entity_manager->RecordEntityUsed(guid, base::Time::Now());
}

void AutofillAiManager::OnEditedAutofilledField(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field,
    ukm::SourceId ukm_source_id) {
  logger_.OnEditedAutofilledField(form, field, ukm_source_id);
}

bool AutofillAiManager::OnFormSubmitted(const FormStructure& form,
                                        ukm::SourceId ukm_source_id) {
  if (std::ranges::any_of(
          form.fields(), [](const std::unique_ptr<AutofillField>& field) {
            return field->GetAutofillAiServerTypePredictions() != std::nullopt;
          })) {
    logger_.RecordFormMetrics(
        form, ukm_source_id, /*submission_state=*/true,
        autofill::GetAutofillAiOptInStatus(client_->GetAutofillClient()));
  }
  return MaybeImportForm(form);
}

bool AutofillAiManager::MaybeImportForm(const FormStructure& form) {
  if (!autofill::MayPerformAutofillAiAction(
          client_->GetAutofillClient(), autofill::AutofillAiAction::kImport)) {
    return false;
  }

  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    LOG_AF(GetCurrentLogManager())
        << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
        << "Entity data manager is not available";
    return false;
  }
  std::vector<EntityInstance> entity_instances_from_form =
      GetPossibleEntitiesFromSubmittedForm(
          form.fields(), client_->GetAutofillClient().GetAppLocale());
  if (entity_instances_from_form.empty()) {
    return false;
  }

  base::span<const EntityInstance> current_entities =
      entity_manager->GetEntityInstances();
  std::ranges::sort(entity_instances_from_form, EntityInstance::ImportOrder);

  for (EntityInstance& entity : entity_instances_from_form) {
    if (ShouldShowNewEntitySavePrompt(entity, current_entities)) {
      if (IsSaveBlockedByStrikeDatabase(form.source_url(), entity)) {
        continue;
      }
      auto prompt_result_callback =
          BindOnce(&AutofillAiManager::HandleSavePromptResult, GetWeakPtr(),
                   form.source_url(), entity);
      client_->ShowSaveOrUpdateBubble(std::move(entity),
                                      /*old_entity=*/std::nullopt,
                                      std::move(prompt_result_callback));
      return true;
    }
    if (std::optional<std::pair<EntityInstance, EntityInstance>>
            entity_to_update = MaybeUpdateEntity(entity, current_entities)) {
      auto& [new_entity, old_entity] = *entity_to_update;
      if (IsUpdateBlockedByStrikeDatabase(old_entity.guid())) {
        continue;
      }
      auto prompt_result_callback =
          BindOnce(&AutofillAiManager::HandleUpdatePromptResult, GetWeakPtr(),
                   old_entity.guid());
      client_->ShowSaveOrUpdateBubble(std::move(new_entity),
                                      std::move(old_entity),
                                      std::move(prompt_result_callback));
      return true;
    }
  }
  return false;
}

void AutofillAiManager::HandleSavePromptResult(
    const GURL& form_url,
    const autofill::EntityInstance& entity,
    AutofillAiClient::SaveOrUpdatePromptResult result) {
  if (!result.entity) {
    if (result.did_user_decline) {
      AddStrikeForSaveAttempt(form_url, entity);
    }
    return;
  }

  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }

  ClearStrikesForSave(form_url, entity);
  entity_manager->AddOrUpdateEntityInstance(*std::move(result.entity));
}

void AutofillAiManager::HandleUpdatePromptResult(
    const base::Uuid& entity_uuid,
    AutofillAiClient::SaveOrUpdatePromptResult result) {
  if (!result.entity) {
    if (result.did_user_decline) {
      AddStrikeForUpdateAttempt(entity_uuid);
    }
    return;
  }

  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }

  ClearStrikesForUpdate(entity_uuid);
  entity_manager->AddOrUpdateEntityInstance(*std::move(result.entity));
}

std::vector<autofill::Suggestion> AutofillAiManager::GetSuggestions(
    autofill::FormGlobalId form_global_id,
    autofill::FieldGlobalId field_global_id) {
  const AutofillClient& autofill_client = client_->GetAutofillClient();
  if (!autofill::MayPerformAutofillAiAction(
          autofill_client, autofill::AutofillAiAction::kFilling)) {
    return {};
  }

  autofill::EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return {};
  }

  base::span<const EntityInstance> entities =
      entity_manager->GetEntityInstances();
  if (entities.empty()) {
    return {};
  }

  FormStructure* form_structure =
      client_->GetCachedFormStructure(form_global_id);
  if (!form_structure) {
    return {};
  }

  const AutofillField* autofill_field =
      form_structure->GetFieldById(field_global_id);
  if (!autofill_field ||
      !autofill_field->GetAutofillAiServerTypePredictions()) {
    return {};
  }

  return CreateFillingSuggestions(*form_structure, field_global_id, entities,
                                  autofill_client.GetAppLocale());
}

bool AutofillAiManager::ShouldDisplayIph(autofill::FormGlobalId form,
                                         autofill::FieldGlobalId field) const {
  const autofill::AutofillClient& autofill_client =
      client_->GetAutofillClient();
  if (!autofill::MayPerformAutofillAiAction(
          autofill_client, autofill::AutofillAiAction::kIphForOptIn)) {
    return false;
  }

  // The user must have at least one address or payments instrument to indicate
  // that they are an active Autofill user.
  const autofill::AddressDataManager& adm =
      autofill_client.GetPersonalDataManager().address_data_manager();
  const autofill::PaymentsDataManager& paydm =
      autofill_client.GetPersonalDataManager().payments_data_manager();
  if (adm.GetProfiles().empty() && paydm.GetCreditCards().empty() &&
      paydm.GetIbans().empty() && !paydm.HasEwalletAccounts() &&
      !paydm.HasMaskedBankAccounts()) {
    return false;
  }

  const FormStructure* const form_structure =
      client_->GetCachedFormStructure(form);
  if (!form_structure) {
    return false;
  }
  const AutofillField* const focused_field =
      form_structure->GetFieldById(field);
  if (!focused_field) {
    return false;
  }

  // The focused field needs to be AutofillAI-related.
  if (!focused_field->GetAutofillAiServerTypePredictions()) {
    return false;
  }

  // Submitting the form should lead to an import if the user fills all of the
  // currently focused section's fields that are related to AutofillAI
  std::map<EntityType, DenseSet<AttributeType>> attributes_in_form;
  for (const std::unique_ptr<AutofillField>& current_field :
       form_structure->fields()) {
    if (current_field->section() != focused_field->section()) {
      continue;
    }
    const std::optional<FieldType> autofill_ai_type =
        current_field->GetAutofillAiServerTypePredictions();
    if (!autofill_ai_type) {
      continue;
    }
    const std::optional<AttributeType> attribute_type =
        AttributeType::FromFieldType(*autofill_ai_type);
    if (!attribute_type) {
      continue;
    }

    const EntityType entity_type = attribute_type->entity_type();
    DenseSet<AttributeType>& attributes_found_so_far =
        attributes_in_form[entity_type];
    attributes_found_so_far.insert(*attribute_type);
    if (AttributesMeetImportConstraints(entity_type, attributes_found_so_far)) {
      return true;
    }
  }

  return false;
}

autofill::LogManager* AutofillAiManager::GetCurrentLogManager() {
  return client_->GetAutofillClient().GetCurrentLogManager();
}

void AutofillAiManager::AddStrikeForSaveAttempt(const GURL& url,
                                                const EntityInstance& entity) {
  if (save_strike_db_by_host_ && url.is_valid() && url.has_host()) {
    save_strike_db_by_host_->AddStrike(
        AutofillAiSaveStrikeDatabaseByHost::GetId(
            entity.type().name_as_string(), url.host()));
  }
  if (save_strike_db_by_attribute_) {
    for (const std::string& key : GetAttributeStrikeKeys(
             entity, client_->GetAutofillClient().GetAppLocale())) {
      save_strike_db_by_attribute_->AddStrike(key);
    }
  }
}

void AutofillAiManager::AddStrikeForUpdateAttempt(
    const base::Uuid& entity_uuid) {
  if (update_strike_db_) {
    update_strike_db_->AddStrike(entity_uuid.AsLowercaseString());
  }
}

void AutofillAiManager::ClearStrikesForSave(
    const GURL& url,
    const autofill::EntityInstance& entity) {
  if (save_strike_db_by_host_ && url.is_valid() && url.has_host()) {
    save_strike_db_by_host_->ClearStrikes(
        AutofillAiSaveStrikeDatabaseByHost::GetId(
            entity.type().name_as_string(), url.host()));
  }
  if (save_strike_db_by_attribute_) {
    for (const std::string& key : GetAttributeStrikeKeys(
             entity, client_->GetAutofillClient().GetAppLocale())) {
      save_strike_db_by_attribute_->ClearStrikes(key);
    }
  }
}

void AutofillAiManager::ClearStrikesForUpdate(const base::Uuid& entity_uuid) {
  if (update_strike_db_) {
    update_strike_db_->ClearStrikes(entity_uuid.AsLowercaseString());
  }
}

bool AutofillAiManager::IsSaveBlockedByStrikeDatabase(
    const GURL& url,
    const EntityInstance& entity) const {
  if (!save_strike_db_by_host_ ||
      save_strike_db_by_host_->ShouldBlockFeature(
          AutofillAiSaveStrikeDatabaseByHost::GetId(
              entity.type().name_as_string(), url.host()))) {
    return true;
  }

  if (!save_strike_db_by_attribute_ ||
      std::ranges::any_of(
          GetAttributeStrikeKeys(entity,
                                 client_->GetAutofillClient().GetAppLocale()),
          [&](const std::string& key) {
            return save_strike_db_by_attribute_->ShouldBlockFeature(key);
          })) {
    return true;
  }

  return false;
}

bool AutofillAiManager::IsUpdateBlockedByStrikeDatabase(
    const base::Uuid& entity_uuid) const {
  return !update_strike_db_ ||
         update_strike_db_->ShouldBlockFeature(entity_uuid.AsLowercaseString());
}

base::WeakPtr<AutofillAiManager> AutofillAiManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill_ai

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
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
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

}  // namespace

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
    DenseSet<EntityType> suggested_entity_types,
    ukm::SourceId ukm_source_id) {
  logger_.OnSuggestionsShown(form, field, suggested_entity_types,
                             ukm_source_id);
}

void AutofillAiManager::OnFormSeen(const FormStructure& form) {
  const DenseSet<EntityType> relevant_entities =
      GetRelevantEntityTypesForFields(form.fields());
  logger_.OnFormEligibilityAvailable(form.global_id(), relevant_entities);
  if (relevant_entities.empty()) {
    return;
  }

  EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }

  auto entities_to_fill = DenseSet<EntityType>(
      entity_manager->GetEntityInstances(), &EntityInstance::type);
  entities_to_fill.intersect(relevant_entities);
  if (entities_to_fill.empty()) {
    return;
  }

  logger_.OnFormHasDataToFill(form.global_id(), entities_to_fill);
}

void AutofillAiManager::OnDidFillSuggestion(
    const EntityInstance& entity,
    const FormStructure& form,
    const AutofillField& trigger_field,
    base::span<const AutofillField* const> filled_fields,
    ukm::SourceId ukm_source_id) {
  logger_.OnDidFillSuggestion(form, trigger_field, entity.type(),
                              ukm_source_id);
  for (const AutofillField* const field : filled_fields) {
    logger_.OnDidFillField(form, CHECK_DEREF(field), entity.type(),
                           ukm_source_id);
  }
  EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }
  entity_manager->RecordEntityUsed(entity.guid(), base::Time::Now());
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
  return MaybeImportForm(form, ukm_source_id);
}

bool AutofillAiManager::MaybeImportForm(const FormStructure& form,
                                        ukm::SourceId ukm_source_id) {
  if (!MayPerformAutofillAiAction(*client_, AutofillAiAction::kImport)) {
    return false;
  }

  std::vector<std::pair<EntityInstance, std::optional<EntityInstance>>>
      save_update_candidates = GetEntitySaveAndUpdatePromptCandidates(form);
  for (const std::pair<EntityInstance, std::optional<EntityInstance>>&
           save_update_candidate : save_update_candidates) {
    const auto& [new_entity, old_entity] = save_update_candidate;
    const bool show_prompt =
        save_update_candidate == save_update_candidates.front();

    base::UmaHistogramBoolean(
        base::StringPrintf("Autofill.Ai.PromptSuppression.%s.%s",
                           old_entity ? "UpdatePrompt" : "SavePrompt",
                           EntityTypeToMetricsString(new_entity.type())),
        !show_prompt);

    if (show_prompt) {
      auto prompt_result_callback =
          old_entity
              ? BindOnce(
                    &AutofillAiManager::HandleUpdatePromptResult, GetWeakPtr(),
                    autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()),
                    net::registry_controlled_domains::GetDomainAndRegistry(
                        form.main_frame_origin(),
                        net::registry_controlled_domains::
                            EXCLUDE_PRIVATE_REGISTRIES),
                    ukm_source_id, old_entity->guid())
              : BindOnce(
                    &AutofillAiManager::HandleSavePromptResult, GetWeakPtr(),
                    form.source_url(),
                    autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()),
                    net::registry_controlled_domains::GetDomainAndRegistry(
                        form.main_frame_origin(),
                        net::registry_controlled_domains::
                            EXCLUDE_PRIVATE_REGISTRIES),
                    ukm_source_id, new_entity);
      client_->ShowEntitySaveOrUpdateBubble(std::move(new_entity),
                                            std::move(old_entity),
                                            std::move(prompt_result_callback));
    }
  }
  return !save_update_candidates.empty();
}

void AutofillAiManager::HandleSavePromptResult(
    const GURL& form_url,
    uint64_t form_session_id,
    const std::string& domain,
    ukm::SourceId ukm_source_id,
    const EntityInstance& entity,
    AutofillClient::EntitySaveOrUpdatePromptResult result) {
  logger_.OnSaveOrUpdatePromptResult(
      AutofillClient::AutofillAiPromptTypes::kSave, entity.type(),
      entity.record_type(), form_session_id, domain, result, ukm_source_id);
  client_->TriggerAutofillAiSavePromptSurvey(
      /*prompt_accepted=*/result.entity.has_value());
  if (!result.entity) {
    if (result.did_user_decline) {
      AddStrikeForSaveAttempt(form_url, entity);
    }
    return;
  }

  EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }

  ClearStrikesForSave(form_url, entity);
  entity_manager->AddOrUpdateEntityInstance(*std::move(result.entity));
}

void AutofillAiManager::HandleUpdatePromptResult(
    uint64_t form_session_id,
    const std::string& domain,
    ukm::SourceId ukm_source_id,
    const EntityInstance::EntityId& entity_uuid,
    AutofillClient::EntitySaveOrUpdatePromptResult result) {
  if (const EntityDataManager* entity_manager =
          client_->GetEntityDataManager()) {
    if (base::optional_ref<const EntityInstance> entity =
            entity_manager->GetEntityInstance(entity_uuid)) {
      logger_.OnSaveOrUpdatePromptResult(
          AutofillClient::AutofillAiPromptTypes::kUpdate, entity->type(),
          entity->record_type(), form_session_id, domain, result,
          ukm_source_id);
    }
  }

  if (!result.entity) {
    if (result.did_user_decline) {
      AddStrikeForUpdateAttempt(entity_uuid);
    }
    return;
  }

  EntityDataManager* entity_manager = client_->GetEntityDataManager();
  if (!entity_manager) {
    return;
  }

  ClearStrikesForUpdate(entity_uuid);
  entity_manager->AddOrUpdateEntityInstance(*std::move(result.entity));
}

std::vector<Suggestion> AutofillAiManager::GetSuggestions(
    const FormStructure& form,
    const FormFieldData& trigger_field) {
  AutofillAiSuggestionGenerator suggestion_generator(*client_);
  std::vector<Suggestion> suggestions;
  const AutofillField* autofill_field =
      form.GetFieldById(trigger_field.global_id());

  auto on_suggestion_data_returned =
      [&form, &autofill_field, &trigger_field, &suggestions,
       &suggestion_generator](
          std::pair<SuggestionGenerator::SuggestionDataSource,
                    std::vector<SuggestionGenerator::SuggestionData>>
              suggestion_data) {
        suggestion_generator.GenerateSuggestions(
            form.ToFormData(), trigger_field, &form, autofill_field,
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
  if (!MayPerformAutofillAiAction(*client_, AutofillAiAction::kIphForOptIn)) {
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
                       })) {
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

void AutofillAiManager::AddStrikeForSaveAttempt(const GURL& url,
                                                const EntityInstance& entity) {
  if (save_strike_db_by_host_ && url.is_valid() && url.has_host()) {
    save_strike_db_by_host_->AddStrike(
        AutofillAiSaveStrikeDatabaseByHost::GetId(
            entity.type().name_as_string(), url.host()));
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
            entity.type().name_as_string(), url.host()));
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
              entity.type().name_as_string(), url.host()))) {
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

std::vector<std::pair<EntityInstance, std::optional<EntityInstance>>>
AutofillAiManager::GetEntitySaveAndUpdatePromptCandidates(
    const FormStructure& form) {
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
      GetPossibleEntitiesFromSubmittedForm(
          form.fields(), client_->GetAppLocale(),
          client_->GetVariationConfigCountryCode());
  std::ranges::sort(observed_entities, EntityInstance::ImportOrder);

  std::vector<std::pair<EntityInstance, std::optional<EntityInstance>>>
      save_candidates;
  std::vector<std::pair<EntityInstance, std::optional<EntityInstance>>>
      update_candidates;
  for (const EntityInstance& observed_entity : observed_entities) {
    std::vector<std::optional<EntityInstance::EntityMergeability>>
        mergeabilities =
            base::ToVector(saved_entities, [&](const EntityInstance& entity) {
              return entity.type() == observed_entity.type()
                         ? std::optional(
                               entity.GetEntityMergeability(observed_entity))
                         : std::nullopt;
            });

    // If `observed_entity` is a subset of some saved entity, we should not show
    // any prompt for it.
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
      save_candidates.emplace_back(observed_entity, std::nullopt);
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
          EntityInstance(saved_entity.type(), std::move(new_attributes),
                         saved_entity.guid(), saved_entity.nickname(),
                         base::Time::Now(), saved_entity.use_count(),
                         base::Time::Now(), saved_entity.record_type()),
          saved_entity);
    }
  }

  // Return a list containing save candidates before update candidates so that
  // the first candidate has always the highest priority among all candidates.
  std::vector<std::pair<EntityInstance, std::optional<EntityInstance>>>
      candidates = std::move(save_candidates);
  base::Extend(candidates, std::move(update_candidates));
  return candidates;
}

}  // namespace autofill

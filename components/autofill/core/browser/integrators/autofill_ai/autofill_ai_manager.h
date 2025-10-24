// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_MANAGER_H_

#include "base/containers/lru_cache.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_logger.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_attribute.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_update_strike_database.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace autofill {

class AutofillField;
class EntityInstance;
class FormFieldData;
class FormStructure;
struct Suggestion;

// The class for embedder-independent, tab-specific Autofill AI logic. This
// class is an interface.
class AutofillAiManager {
 public:
  AutofillAiManager(AutofillClient* client,
                    strike_database::StrikeDatabaseBase* strike_database);
  AutofillAiManager(const AutofillAiManager&) = delete;
  AutofillAiManager& operator=(const AutofillAiManager&) = delete;
  virtual ~AutofillAiManager();

  // Generates AutofillAi suggestions.
  virtual std::vector<Suggestion> GetSuggestions(
      const FormStructure& form,
      const FormFieldData& trigger_field);

  // Attempts to display an import bubble for `form` if Autofill AI is
  // interested in the form. Returns whether an import bubble will be shown.
  // Also contains metric logging logic.
  virtual bool OnFormSubmitted(const FormStructure& form,
                               ukm::SourceId ukm_source_id);

  // Indicates whether to try to display IPH for opting into AutofillAI. It
  // checks that all of the following is true:
  // - The user is eligible for AutofillAI and has not already opted in.
  // - The user has at least one address or payments instrument saved.
  // - `field` has AutofillAI predictions.
  // - If `form` is submitted (with appropriate values), there is at least one
  //   entity that meets the criteria for import.
  virtual bool ShouldDisplayIph(const FormStructure& form,
                                FieldGlobalId field) const;

  // TODO(crbug.com/389629573): The "On*" methods below are used only for
  // logging purposes. Explore different approaches.

  virtual void OnSuggestionsShown(
      const FormStructure& form,
      const AutofillField& field,
      base::span<const Suggestion> shown_suggestions,
      ukm::SourceId ukm_source_id);
  virtual void OnFormSeen(const FormStructure& form);
  virtual void OnDidFillSuggestion(
      const EntityInstance& entity,
      const FormStructure& form,
      const AutofillField& field,
      base::span<const AutofillField* const> filled_fields,
      ukm::SourceId ukm_source_id);
  virtual void OnEditedAutofilledField(const FormStructure& form,
                                       const AutofillField& field,
                                       ukm::SourceId ukm_source_id);

  base::WeakPtr<AutofillAiManager> GetWeakPtr();

 private:
  friend class AutofillAiManagerTestApi;
  struct UserSuggestionInteractionDetails {
    // Upon clicking a field, stores the different entity types used to
    // generate the suggestions shown.
    DenseSet<EntityType> suggested_entity_types;
    std::optional<EntityType> entity_type_accepted;
    // The types of the field where the suggestion was shown or accepted.
    FieldTypeSet autofill_ai_field_types;
  };
  const size_t kSuggestionInteractionCacheMaxSize = 5;

  // Strike database related methods:
  void AddOrClearImportPromptStrikes(
      AutofillClient::AutofillAiImportPromptType prompt_type,
      AutofillClient::AutofillAiBubbleClosedReason close_reason,
      const GURL& url,
      const EntityInstance& entity);
  void AddStrikeForSaveAttempt(const GURL& url, const EntityInstance& entity);
  void AddStrikeForUpdateAttempt(const EntityInstance::EntityId& entity_uuid);
  void ClearStrikesForSave(const GURL& url, const EntityInstance& entity);
  void ClearStrikesForUpdate(const EntityInstance::EntityId& entity_uuid);
  bool IsSaveBlockedByStrikeDatabase(const GURL& url,
                                     const EntityInstance& entity) const;
  bool IsUpdateBlockedByStrikeDatabase(
      const EntityInstance::EntityId& entity_uuid) const;

  struct EntityImportPromptCandidate {
    EntityImportPromptCandidate() = delete;
    EntityImportPromptCandidate(
        AutofillClient::AutofillAiImportPromptType prompt_type,
        const EntityInstance& candidate_entity);
    EntityImportPromptCandidate(const EntityImportPromptCandidate&);
    EntityImportPromptCandidate(EntityImportPromptCandidate&&);
    EntityImportPromptCandidate& operator=(const EntityImportPromptCandidate&);
    EntityImportPromptCandidate& operator=(EntityImportPromptCandidate&&);
    ~EntityImportPromptCandidate();

    // The type of prompt that this candidate is meant for.
    AutofillClient::AutofillAiImportPromptType prompt_type;
    // The entity that the user would get if they accept the possibly shown
    // save/update/migrate prompt.
    EntityInstance candidate_entity;
  };

  // Given `form` that is observed at submission, returns candidates for showing
  // either save or update prompts. The function returns a map keyed by the type
  // of prompt to be shown and where the value is a list of candidates that
  // qualifies for showing a prompt of the corresponding type. The candidates
  // are ordered in decreasing priority of the corresponding prompt being shown.
  std::vector<EntityImportPromptCandidate> GetEntityPromptCandidates(
      const FormStructure& form);

  // Finds the entities for which AutofillAi should offer a save prompt.
  // - `observed_entities` are the EntityInstances that were extracted from
  //   `form` at form submission.
  // - `saved_entities` are the EntityInstance's already stored for the user.
  // - For each pair (observed_entity[i], saved_entity[j]),
  //   `entity_mergeabilities[i][j]` contains the `EntityMergeability`
  //   information for that pair if the two entities share the same
  //   `EntityType`, and std::nullopt otherwise.
  std::vector<EntityImportPromptCandidate> GetSavePromptCandidates(
      base::span<const EntityInstance> observed_entities,
      base::span<const EntityInstance> saved_entities,
      const FormStructure& form,
      const std::vector<
          std::vector<std::optional<EntityInstance::EntityMergeability>>>&
          entities_mergeabilities) const;

  // Finds the entities for which AutofillAi should offer an update prompt. An
  // update candidate consists of the new entity that would be stored after
  // update, as well as the ID of the old entity to be updated.
  // - `observed_entities` are the EntityInstances extracted at form submission.
  // - `saved_entities` are the EntityInstances already stored for the user.
  // - For each pair (observed_entity[i], saved_entity[j]),
  //   `entity_mergeabilities[i][j]` contains the `EntityMergeability`
  //   information for that pair if the two entities share the same
  //   `EntityType`, and std::nullopt otherwise.
  std::vector<EntityImportPromptCandidate> GetUpdatePromptCandidates(
      base::span<const EntityInstance> observed_entities,
      base::span<const EntityInstance> saved_entities,
      const std::vector<
          std::vector<std::optional<EntityInstance::EntityMergeability>>>&
          entities_mergeabilities) const;

  // Finds the entities for which AutofillAi should offer a migration prompt.
  // A migration candidate consists of the new entity that would be stored after
  // migration, as well as the ID of the old entity to be migrated.
  // - `observed_entities` are the EntityInstances extracted at form submission.
  // - `saved_entities` are the EntityInstance's already stored for the user.
  std::vector<EntityImportPromptCandidate> GetMigratePromptCandidates(
      base::span<const EntityInstance> observed_entities,
      base::span<const EntityInstance> saved_entities,
      const FormStructure& form) const;

  // Attempts to display an import bubble for `form` if Autofill AI is
  // interested in the form. Returns whether an import bubble will be shown.
  bool MaybeImportForm(const FormStructure& form, ukm::SourceId ukm_source_id);

  // Handles the logic that needs to run when an import prompt is closed.
  void HandlePromptResult(
      const FormData& form,
      EntityInstance entity,
      ukm::SourceId ukm_source_id,
      AutofillClient::AutofillAiImportPromptType prompt_type,
      AutofillClient::AutofillAiBubbleClosedReason close_reason);

  LogManager* GetCurrentLogManager();

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<AutofillClient> client_;

  // Logger that records various Autofill AI metrics.
  AutofillAiLogger logger_{&*client_};

  // A strike database for save prompts keyed by (entity_type_name, host).
  std::unique_ptr<AutofillAiSaveStrikeDatabaseByHost> save_strike_db_by_host_;

  // A strike database for save prompts keyed by (entity_type_name,
  // attribute_type_name_1, attribute_value_1, ...).
  std::unique_ptr<AutofillAiSaveStrikeDatabaseByAttribute>
      save_strike_db_by_attribute_;

  // A strike database for update prompts keyed by the guid of the entity that
  // is to be updated.
  std::unique_ptr<AutofillAiUpdateStrikeDatabase> update_strike_db_;

  // Keeps suggestions details about the five most recent forms the user has
  // interacted with.
  base::LRUCache<FormGlobalId, UserSuggestionInteractionDetails>
      user_suggestion_interactions_per_form_{
          kSuggestionInteractionCacheMaxSize};

  base::WeakPtrFactory<AutofillAiManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_MANAGER_H_

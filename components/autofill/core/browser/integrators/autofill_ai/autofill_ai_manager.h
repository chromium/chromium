// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
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

namespace base {
class Uuid;
}  // namespace base

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
  AutofillAiManager(autofill::AutofillClient* client,
                    autofill::StrikeDatabase* strike_database);
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

  virtual void OnSuggestionsShown(const FormStructure& form,
                                  const AutofillField& field,
                                  ukm::SourceId ukm_source_id);
  virtual void OnFormSeen(const FormStructure& form);
  virtual void OnDidFillSuggestion(
      const base::Uuid& guid,
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

  // Strike database related methods:
  void AddStrikeForSaveAttempt(const GURL& url, const EntityInstance& entity);
  void AddStrikeForUpdateAttempt(const base::Uuid& entity_uuid);
  void ClearStrikesForSave(const GURL& url, const EntityInstance& entity);
  void ClearStrikesForUpdate(const base::Uuid& entity_uuid);
  bool IsSaveBlockedByStrikeDatabase(const GURL& url,
                                     const EntityInstance& entity) const;
  bool IsUpdateBlockedByStrikeDatabase(const base::Uuid& entity_uuid) const;

  // Attempts to display an import bubble for `form` if Autofill AI is
  // interested in the form. Returns whether an import bubble will be shown.
  bool MaybeImportForm(const FormStructure& form);

  // Updates the `EntityDataManager` and the save strike database depending on
  // the prompt `result`.
  void HandleSavePromptResult(
      const GURL& form_url,
      const EntityInstance& entity,
      AutofillClient::EntitySaveOrUpdatePromptResult result);
  // Updates the `EntityDataManager` and the update strike database depending on
  // the prompt `result`.
  void HandleUpdatePromptResult(
      const base::Uuid& entity_uuid,
      AutofillClient::EntitySaveOrUpdatePromptResult result);

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

  base::WeakPtrFactory<AutofillAiManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_MANAGER_H_

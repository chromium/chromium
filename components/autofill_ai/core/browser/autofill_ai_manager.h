// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_attribute.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"
#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_update_strike_database.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/metrics/autofill_ai_logger.h"

namespace autofill {
class FormData;
class FormStructure;
class LogManager;
class StrikeDatabase;
}  // namespace autofill

namespace base {
class Uuid;
}  // namespace base

namespace autofill_ai {

// The class for embedder-independent, tab-specific Autofill AI logic. This
// class is an interface.
class AutofillAiManager : public autofill::AutofillAiDelegate {
 public:
  AutofillAiManager(AutofillAiClient* client,
                    autofill::StrikeDatabase* strike_database);
  AutofillAiManager(const AutofillAiManager&) = delete;
  AutofillAiManager& operator=(const AutofillAiManager&) = delete;
  ~AutofillAiManager() override;

  // autofill::AutofillAiDelegate:
  std::vector<autofill::Suggestion> GetSuggestions(
      autofill::FormGlobalId form_global_id,
      autofill::FieldGlobalId field_global_id) override;
  bool OnFormSubmitted(const autofill::FormStructure& form,
                       ukm::SourceId ukm_source_id) override;
  bool ShouldDisplayIph(autofill::FormGlobalId form,
                        autofill::FieldGlobalId field) const override;
  void OnSuggestionsShown(const autofill::FormStructure& form,
                          const autofill::AutofillField& field,
                          ukm::SourceId ukm_source_id) override;
  void OnFormSeen(const autofill::FormStructure& form) override;
  void OnDidFillSuggestion(
      const base::Uuid& guid,
      const autofill::FormStructure& form,
      const autofill::AutofillField& trigger_field,
      base::span<const autofill::AutofillField* const> filled_fields,
      ukm::SourceId ukm_source_id) override;
  void OnEditedAutofilledField(const autofill::FormStructure& form,
                               const autofill::AutofillField& field,
                               ukm::SourceId ukm_source_id) override;

  base::WeakPtr<AutofillAiManager> GetWeakPtr();

 private:
  friend class AutofillAiManagerTestApi;

  // Strike database related methods:
  void AddStrikeForSaveAttempt(const GURL& url,
                               const autofill::EntityInstance& entity);
  void AddStrikeForUpdateAttempt(const base::Uuid& entity_uuid);
  void ClearStrikesForSave(const GURL& url,
                           const autofill::EntityInstance& entity);
  void ClearStrikesForUpdate(const base::Uuid& entity_uuid);
  bool IsSaveBlockedByStrikeDatabase(
      const GURL& url,
      const autofill::EntityInstance& entity) const;
  bool IsUpdateBlockedByStrikeDatabase(const base::Uuid& entity_uuid) const;

  // Attempts to display an import bubble for `form` if Autofill AI is
  // interested in the form. Returns whether an import bubble will be shown.
  bool MaybeImportForm(const autofill::FormStructure& form);

  // Updates the `EntityDataManager` and the save strike database depending on
  // the prompt `result`.
  void HandleSavePromptResult(
      const GURL& form_url,
      const autofill::EntityInstance& entity,
      AutofillAiClient::SaveOrUpdatePromptResult result);
  // Updates the `EntityDataManager` and the update strike database depending on
  // the prompt `result`.
  void HandleUpdatePromptResult(
      const base::Uuid& entity_uuid,
      AutofillAiClient::SaveOrUpdatePromptResult result);

  autofill::LogManager* GetCurrentLogManager();

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<AutofillAiClient> client_;

  // Logger that records various Autofill AI metrics.
  AutofillAiLogger logger_{&*client_};

  // A strike database for save prompts keyed by (entity_type_name, host).
  std::unique_ptr<autofill::AutofillAiSaveStrikeDatabaseByHost>
      save_strike_db_by_host_;

  // A strike database for save prompts keyed by (entity_type_name,
  // attribute_type_name_1, attribute_value_1, ...).
  std::unique_ptr<autofill::AutofillAiSaveStrikeDatabaseByAttribute>
      save_strike_db_by_attribute_;

  // A strike database for update prompts keyed by the guid of the entity that
  // is to be updated.
  std::unique_ptr<autofill::AutofillAiUpdateStrikeDatabase> update_strike_db_;

  base::WeakPtrFactory<AutofillAiManager> weak_ptr_factory_{this};
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_H_

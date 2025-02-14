// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/integrators/autofill_ai_delegate.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_annotation_prompt_strike_database.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/autofill_ai/core/browser/autofill_ai_logger.h"

namespace autofill {
class FormStructure;
class LogManager;
}  // namespace autofill

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
  void GetSuggestions(autofill::FormGlobalId form_global_id,
                      autofill::FieldGlobalId field_global_id,
                      bool is_manual_fallback,
                      GetSuggestionsCallback callback) override;
  bool IsEligibleForAutofillAi(
      const autofill::FormStructure& form,
      const autofill::AutofillField& field) const override;
  bool IsUserEligible() const override;
  void MaybeImportForm(
      std::unique_ptr<autofill::FormStructure> form,
      base::OnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                              bool autofill_ai_shows_bubble)> callback)
      override;
  bool ShouldDisplayIph(const autofill::AutofillField& field) const override;
  void OnSuggestionsShown(
      const autofill::DenseSet<autofill::SuggestionType>&
          shown_suggestion_types,
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field,
      UpdateSuggestionsCallback update_suggestions_callback) override;
  void OnFormSeen(const autofill::FormStructure& form) override;
  void OnDidFillSuggestion(autofill::FormGlobalId form_id) override;
  void OnEditedAutofilledField(autofill::FormGlobalId form_id) override;

  // Methods for strike counting of rejected forms.
  bool IsFormBlockedForImport(const autofill::FormStructure& form) const;
  void AddStrikeForImportFromForm(const autofill::FormStructure& form);
  void RemoveStrikesForImportFromForm(const autofill::FormStructure& form);

  base::flat_map<autofill::FieldGlobalId, bool> GetFieldValueSensitivityMap(
      const autofill::FormData& form_data);

  base::WeakPtr<AutofillAiManager> GetWeakPtr();

 private:
  friend class AutofillAiManagerTestApi;

  // Run after the user has either accepted, decline or ignored a save prompt.
  void OnSavePromptAcceptance(
      AutofillAiClient::SavePromptAcceptanceResult result);

  void OnReceivedAXTree(const autofill::FormData& form,
                        const autofill::FormFieldData& trigger_field,
                        optimization_guide::proto::AXTreeUpdate);

  // Returns values to fill based on the `cache_`.
  base::flat_map<autofill::FieldGlobalId, std::u16string> GetValuesToFill();

  // Logger that records various Autofill AI metrics.
  AutofillAiLogger logger_;

  autofill::LogManager* GetCurrentLogManager();

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<AutofillAiClient> client_;

  // A strike data base used blocking save prompt for specific form signatures
  // to prevent over prompting.
  std::unique_ptr<AutofillPrectionImprovementsAnnotationPromptStrikeDatabase>
      user_annotation_prompt_strike_database_;

  base::WeakPtrFactory<AutofillAiManager> weak_ptr_factory_{this};
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_MANAGER_H_

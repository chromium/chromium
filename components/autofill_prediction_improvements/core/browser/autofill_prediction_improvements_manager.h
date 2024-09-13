// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_annotation_prompt_strike_database.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "url/gurl.h"

namespace optimization_guide {
class OptimizationGuideDecider;
}

namespace autofill {
class FormStructure;
}  // namespace autofill

namespace autofill_prediction_improvements {

// The class for embedder-independent, tab-specific
// autofill_prediction_improvements logic. This class is an interface.
class AutofillPredictionImprovementsManager
    : public autofill::AutofillPredictionImprovementsDelegate {
 public:
  AutofillPredictionImprovementsManager(
      AutofillPredictionImprovementsClient* client,
      optimization_guide::OptimizationGuideDecider* decider,
      autofill::StrikeDatabase* strike_database);
  AutofillPredictionImprovementsManager(
      const AutofillPredictionImprovementsManager&) = delete;
  AutofillPredictionImprovementsManager& operator=(
      const AutofillPredictionImprovementsManager&) = delete;
  ~AutofillPredictionImprovementsManager() override;

  // autofill::AutofillPredictionImprovementsDelegate
  bool MaybeUpdateSuggestions(
      std::vector<autofill::Suggestion>& address_suggestions,
      const autofill::FormFieldData& field,
      bool should_add_trigger_suggestion) override;
  bool IsFormEligible(const autofill::FormStructure& form) override;
  bool ShouldProvidePredictionImprovements(const GURL& url) override;
  void UserFeedbackReceived(
      autofill::AutofillPredictionImprovementsDelegate::UserFeedback feedback)
      override;
  void UserClickedLearnMore() override;
  void OnClickedTriggerSuggestion(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field,
      UpdateSuggestionsCallback update_suggestions_callback) override;
  void MaybeImportForm(const autofill::FormData& form,
                       const autofill::FormStructure& form_structure,
                       ImportFormCallback callback) override;
  void HasDataStored(HasDataCallback callback) override;

  // Methods for strike counting of rejected forms.
  bool IsFormBlockedForImport(const autofill::FormStructure& form) const;
  void AddStrikeForImportFromForm(const autofill::FormStructure& form);
  void RemoveStrikesForImportFromForm(const autofill::FormStructure& form);

 private:
  // Receives prediction improvements for all fields in `form`, then calls
  // `update_suggestions_callback_`.
  void ExtractPredictionImprovementsForFormFields(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field);

  void OnReceivedAXTree(const autofill::FormData& form,
                        const autofill::FormFieldData& trigger_field,
                        optimization_guide::proto::AXTreeUpdate);

  // The unexpected value is always `false` if there was an error retrieving
  // predictions.
  void OnReceivedPredictions(const autofill::FormData& form,
                             const autofill::FormFieldData& trigger_field,
                             base::expected<autofill::FormData, bool>);

  // Resets the state of this class.
  void Reset();

  // Updates currently shown suggestions via `update_suggestions_callback_`.
  void UpdateSuggestions(const std::vector<autofill::Suggestion>& suggestions);

  // Returns whether improved predictions exist for the `field`. Used to decide
  // whether a context menu entry is displayed or not.
  bool HasImprovedPredictionsForField(const autofill::FormFieldData& field);

  void OnReceivedAXTreeForFormImport(
      const autofill::FormData& form,
      ImportFormCallback callback,
      optimization_guide::proto::AXTreeUpdate ax_tree_update);

  // Creates a suggestion that calls `ExtractImprovedPredictionsForFormFields()`
  // when invoked.
  std::vector<autofill::Suggestion> CreateTriggerSuggestion(bool add_separator);

  // Returns the prediction improvements suggestions if available for the
  // `field`.
  std::vector<autofill::Suggestion> CreateFillingSuggestion(
      const autofill::FormFieldData& field);

  // Returns values to fill based on the `cache_`.
  base::flat_map<autofill::FieldGlobalId, std::u16string> GetValuesToFill();

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<AutofillPredictionImprovementsClient> client_;

  // Most recently retrieved form with field values set to prediction
  // improvements.
  // TODO(crbug.com/361414075): Set `cache_` and manage its lifecycle.
  std::optional<autofill::FormData> cache_ = std::nullopt;

  // Updates currently shown suggestions if their
  // `AutofillClient::SuggestionUiSessionId` hasn't changed since the trigger
  // suggestion was accepted.
  base::RepeatingCallback<void(std::vector<autofill::Suggestion>,
                               autofill::AutofillSuggestionTriggerSource)>
      update_suggestions_callback_ = base::NullCallback();

  // The `decider_` is used to check if the
  // `AUTOFILL_PREDICTION_IMPROVEMENTS_ALLOWLIST` optimization guide can be
  // applied to the main frame's last committed URL. `decider_` is null if the
  // corresponding feature is not enabled.
  const raw_ptr<optimization_guide::OptimizationGuideDecider> decider_;

  // A strike data base used blocking save prompt for specific form signatures
  // to prevent over prompting.
  std::unique_ptr<AutofillPrectionImprovementsAnnotationPromptStrikeDatabase>
      user_annotation_prompt_strike_database_;

  base::WeakPtrFactory<AutofillPredictionImprovementsManager> weak_ptr_factory_{
      this};
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_

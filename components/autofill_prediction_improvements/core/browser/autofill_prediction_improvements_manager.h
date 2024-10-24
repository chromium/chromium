// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_annotation_prompt_strike_database.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_logger.h"
#include "url/gurl.h"

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace autofill {
class FormStructure;
}  // namespace autofill

namespace autofill_prediction_improvements {

// Minimum time for the loading suggestion to be visible to the user, in order
// to avoid flickering UI scenarios.
// TODO(crbug.com/365512352): Evaluate what constant is best for this purpose.
inline constexpr base::TimeDelta kMinTimeToShowLoading =
    base::Milliseconds(300);

// The class for embedder-independent, tab-specific
// autofill_prediction_improvements logic. This class is an interface.
class AutofillPredictionImprovementsManager
    : public autofill::AutofillPredictionImprovementsDelegate {
 public:
  // Enum specifying the states of retrieving prediction improvements.
  enum class PredictionRetrievalState {
    // Ready for retrieving prediction improvements. This is the default state
    // for this class. It's also set whenever a field of a non-cached form is
    // focused while not loading.
    kReady = 0,
    // Prediction improvements are being retrieved right now.
    kIsLoadingPredictions = 1,
    // Prediction improvements were received successfully. Note that the
    // predictions map might be empty.
    kDoneSuccess = 2,
    // Retrieving prediction improvements resulted in an error.
    kDoneError = 3
  };

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
  std::vector<autofill::Suggestion> GetSuggestions(
      const std::vector<autofill::Suggestion>& autofill_suggestions,
      const autofill::FormData& form,
      const autofill::FormFieldData& field) override;
  bool IsPredictionImprovementsEligible(
      const autofill::FormStructure& form,
      const autofill::AutofillField& field) const override;
  bool IsUserEligible() const override;
  bool ShouldProvidePredictionImprovements(const GURL& url) const override;
  void UserFeedbackReceived(
      autofill::AutofillPredictionImprovementsDelegate::UserFeedback feedback)
      override;
  void UserClickedLearnMore() override;
  void OnClickedTriggerSuggestion(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field,
      UpdateSuggestionsCallback update_suggestions_callback) override;
  void MaybeImportForm(std::unique_ptr<autofill::FormStructure> form,
                       user_annotations::ImportFormCallback callback) override;
  void HasDataStored(HasDataCallback callback) override;
  bool ShouldDisplayIph(const autofill::FormStructure& form,
                        const autofill::AutofillField& field) const override;
  void GoToSettings() const override;
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

  base::flat_map<autofill::FieldGlobalId, bool> GetFieldFillingEligibilityMap(
      const autofill::FormData& form_data);

  base::flat_map<autofill::FieldGlobalId, bool> GetFieldValueSensitivityMap(
      const autofill::FormData& form_data);

 private:
  friend class AutofillPredictionImprovementsManagerTestApi;

  // Event handler called when the loading suggestion is shown. Used for the
  // automatic triggering path.
  void OnLoadingSuggestionShown(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field,
      UpdateSuggestionsCallback update_suggestions_callback);

  // Event handler called when either error or no info suggestion is shown. Used
  // for ensuring the respective popups are not shown more than once.
  void OnErrorOrNoInfoSuggestionShown();

  // Receives prediction improvements for all fields in `form`, then calls
  // `update_suggestions_callback_`.
  void RetrievePredictions(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field,
      UpdateSuggestionsCallback update_suggestions_callback,
      bool update_to_loading_suggestion);

  void OnReceivedAXTree(const autofill::FormData& form,
                        const autofill::FormFieldData& trigger_field,
                        optimization_guide::proto::AXTreeUpdate);

  // The unexpected value is always `false` if there was an error retrieving
  // predictions.
  void OnReceivedPredictions(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field,
      AutofillPredictionImprovementsFillingEngine::PredictionsOrError
          predictions_or_error,
      std::optional<std::string> feedback_id);

  // Method for showing filling or error suggestions, depending on the outcome
  // of the retrieval attempts.
  void UpdateSuggestionsAfterReceivedPredictions(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field);

  // Resets the state of this class.
  void Reset();

  // Updates currently shown suggestions via `update_suggestions_callback_`.
  void UpdateSuggestions(const std::vector<autofill::Suggestion>& suggestions);

  // Returns whether improved predictions exist for the `field`. Used to decide
  // whether a context menu entry is displayed or not.
  bool HasImprovedPredictionsForField(const autofill::FormFieldData& field);

  void OnReceivedAXTreeForFormImport(
      const GURL& url,
      const std::string& title,
      std::unique_ptr<autofill::FormStructure> form,
      user_annotations::ImportFormCallback callback,
      optimization_guide::proto::AXTreeUpdate ax_tree_update);

  // Returns true if the user has the flag enabled and is overall eligible for
  // the feature, such as signin state (check the implementation for details).
  // It also checks whether the `url` is eligible. Note that differently from
  // `ShouldProvidePredictionImprovements()` this method does not check whether
  // the pref is enabled.
  bool IsURLEligibleForPredictionImprovements(const GURL& url) const;

  // Creates filling suggestions listing the ones for prediction improvements
  // first and `address_suggestions` afterwards.
  std::vector<autofill::Suggestion> CreateFillingSuggestions(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const std::vector<autofill::Suggestion>& address_suggestions);

  // Returns values to fill based on the `cache_`.
  base::flat_map<autofill::FieldGlobalId, std::u16string> GetValuesToFill();

  // Handles what suggestions to show when the main filling suggestions
  // generation fails.
  void OnFailedToGenerateSuggestions();

  // Logger that records various prediction improvements metrics.
  AutofillPredictionImprovementsLogger logger_;

  // Checks if the cached predictions for a given `form` and Autofill profile
  // have at least one matching autofill suggestion for the specified
  // `field_type`.
  bool CacheHasMatchingAutofillSuggestion(
      const autofill::FormData& form,
      const std::string& autofill_profile_guid,
      autofill::FieldType field_type);

  // Returns true if the type of `autofill_suggestion` should not be added to
  // prediction improvements or if `autofill_suggestion` likely matches the
  // cached prediction improvements.
  bool ShouldSkipAutofillSuggestion(
      const autofill::FormData& form,
      const autofill::Suggestion& autofill_suggestion);

  // Current state for retrieving predictions.
  PredictionRetrievalState prediction_retrieval_state_ =
      PredictionRetrievalState::kReady;

  // Used to ensure that then error / no info suggestions will only be shown
  // once after a failed retrieval of predictions or one with an empty response.
  bool error_or_no_info_suggestion_shown_ = false;

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<AutofillPredictionImprovementsClient> client_;

  // Most recently retrieved form with field values set to prediction
  // improvements.
  std::optional<
      AutofillPredictionImprovementsFillingEngine::PredictionsByGlobalId>
      cache_ = std::nullopt;
  // The form global id for which predictions were retrieved last. Set at the
  // beginning of retrieving prediction improvements.
  std::optional<autofill::FormGlobalId> last_queried_form_global_id_;
  // Address suggestions that will be shown as defined in
  // `CreateFillingSuggestions()` after prediction improvements was triggered.
  std::vector<autofill::Suggestion> autofill_suggestions_;

  // Stores the execution id for the latest successful retrieval of prediction
  // improvements. If set, the feedback page will open when the "thumbs down"
  // icon is clicked.
  std::optional<std::string> feedback_id_ = std::nullopt;

  // Updates currently shown suggestions if their
  // `AutofillClient::SuggestionUiSessionId` hasn't changed since the trigger
  // suggestion was accepted.
  base::RepeatingCallback<void(std::vector<autofill::Suggestion>,
                               autofill::AutofillSuggestionTriggerSource)>
      update_suggestions_callback_ = base::NullCallback();

  // Timer to delay the replacement of the loading suggestion with the fetched
  // suggestions. This avoids a flickering UI for cases where retrieval happens
  // quickly.
  base::OneShotTimer loading_suggestion_timer_;

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

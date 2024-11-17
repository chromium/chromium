// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_manager.h"

#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_filling_skip_reason.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
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

bool IsFormAndFieldEligible(const autofill::FormStructure& form,
                            const autofill::AutofillField& field) {
  return IsFieldEligibleByTypeCriteria(field) && IsFormEligibleForFilling(form);
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
AutofillAiManager::GetFieldFillingEligibilityMap(
    const autofill::FormData& form_data) {
  autofill::FormStructure* form_structure =
      client_->GetCachedFormStructure(form_data);

  if (!form_structure) {
    return base::flat_map<autofill::FieldGlobalId, bool>();
  }

  return base::MakeFlatMap<autofill::FieldGlobalId, bool>(
      form_structure->fields(), {}, [](const auto& field) {
        return std::make_pair(field->global_id(),
                              IsFieldEligibleForFilling(*field));
      });
}

base::flat_map<autofill::FieldGlobalId, bool>
AutofillAiManager::GetFieldValueSensitivityMap(
    const autofill::FormData& form_data) {
  autofill::FormStructure* form_structure =
      client_->GetCachedFormStructure(form_data);

  if (!form_structure) {
    return base::flat_map<autofill::FieldGlobalId, bool>();
  }

  FilterSensitiveValues(*form_structure);
  SetFieldFillingEligibility(*form_structure);

  return base::MakeFlatMap<autofill::FieldGlobalId, bool>(
      form_structure->fields(), {}, [](const auto& field) {
        return std::make_pair(
            field->global_id(),
            field->value_identified_as_potentially_sensitive());
      });
}

AutofillAiManager::~AutofillAiManager() = default;

bool AutofillAiManager::HasImprovedPredictionsForField(
    const autofill::FormFieldData& field) {
  return cache_ && cache_->contains(field.global_id());
}

bool AutofillAiManager::IsEligibleForAutofillAi(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field) const {
  return IsFormAndFieldEligible(form, field) &&
         client_->IsAutofillAiEnabledPref() && IsUserEligible() &&
         IsURLEligibleForAutofillAi(form.main_frame_origin().GetURL());
}

bool AutofillAiManager::IsUserEligible() const {
  return client_->IsUserEligible();
}

void AutofillAiManager::UpdateFieldFocusabilityInCache(
    const autofill::FormData& form) {
  if (!cache_) {
    return;
  }
  for (const autofill::FormFieldData& field : form.fields()) {
    if (!cache_->contains(field.global_id())) {
      continue;
    }
    cache_->at(field.global_id()).is_focusable = field.IsFocusable();
  }
}

std::vector<autofill::Suggestion> AutofillAiManager::GetSuggestions(
    const std::vector<autofill::Suggestion>& autofill_suggestions,
    const autofill::FormData& form,
    const autofill::FormFieldData& field) {
  // If `form` is not the one currently cashed, `Reset()` the state unless
  // predictions are currently retrieved.
  if (last_queried_form_global_id_ &&
      *last_queried_form_global_id_ != form.global_id()) {
    if (prediction_retrieval_state_ !=
        PredictionRetrievalState::kIsLoadingPredictions) {
      // Reset state if the trigger form global id has changed from the
      // `last_queried_form_global_id_` while not loading predictions.
      // TODO(crbug.com/370695713): Reset also for dynamically changed forms
      // that keep their global id.
      Reset();
    } else {
      // Return an empty vector of suggestions while retrieving predictions for
      // a different form. This will continue the regular Autofill flow (e.g.
      // show Autofill or Autocomplete suggestions) in the
      // `BrowserAutofillManager`.
      return {};
    }
  }

  // Store `autofill_suggestions` to potentially show them with prediction
  // improvements later.
  // TODO(crbug.com/370693653): Also store autocomplete suggestions.
  autofill_suggestions_ = autofill_suggestions;

  switch (prediction_retrieval_state_) {
    case PredictionRetrievalState::kReady:
      if (kTriggerAutomatically.Get()) {
        return CreateLoadingSuggestions();
      }
      return CreateTriggerSuggestions();
    case PredictionRetrievalState::kIsLoadingPredictions:
      // Keep showing the loading suggestion while prediction improvements are
      // being retrieved.
      return CreateLoadingSuggestions();
    case PredictionRetrievalState::kDoneSuccess:
      // The `form` fields' focusability states may have changed since the last
      // `form` field focus event, so they need to be updated in the `cache_`.
      // This ensures that child suggestions will be created correctly in
      // `CreateFillingSuggestions()`. This only needs to be done in the
      // `kDoneSuccess` case because otherwise `cache_` is null.
      UpdateFieldFocusabilityInCache(form);
      // Show a cached prediction improvements filling suggestion for `field` if
      // it exists. This may contain additional `autofill_suggestions`, appended
      // to the prediction improvements.
      if (HasImprovedPredictionsForField(field)) {
        return CreateFillingSuggestions(*client_, *cache_, form, field,
                                        autofill_suggestions);
      }
      // If there are no cached predictions for the `field`, continue the
      // regular Autofill flow if it has data to show.
      // TODO(crbug.com/370695713): Add check for autocomplete.
      if (!autofill_suggestions.empty()) {
        // Returning an empty vector will continue the regular Autofill flow
        // (e.g. show Autofill or Autocomplete suggestions) in the
        // `BrowserAutofillManager`.
        return {};
      }
      // Show the no info suggestion exactly once, otherwise show the trigger
      // suggestion again.
      // TODO(crbug.com/374715268): Consider not showing the trigger suggestion
      // again, since this will also result in an error.
      return error_or_no_info_suggestion_shown_ ? CreateTriggerSuggestions()
                                                : CreateNoInfoSuggestions();
    case PredictionRetrievalState::kDoneError:
      // In the error state, continue the regular Autofill flow if it has data
      // to show.
      // TODO(crbug.com/370695713): Add check for autocomplete.
      if (!autofill_suggestions.empty()) {
        // Returning an empty vector will continue the regular Autofill flow
        // (e.g. show Autofill or Autocomplete suggestions) in the
        // `BrowserAutofillManager`.
        return {};
      }
      // Show the error suggestion exactly once, otherwise show nothing.
      return error_or_no_info_suggestion_shown_ ? CreateTriggerSuggestions()
                                                : CreateErrorSuggestions();
  }
}

void AutofillAiManager::RetrievePredictions(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    UpdateSuggestionsCallback update_suggestions_callback,
    bool update_to_loading_suggestion) {
  if (prediction_retrieval_state_ ==
      PredictionRetrievalState::kIsLoadingPredictions) {
    return;
  }
  update_suggestions_callback_ = std::move(update_suggestions_callback);
  if (update_to_loading_suggestion) {
    UpdateSuggestions(CreateLoadingSuggestions());
  }
  prediction_retrieval_state_ = PredictionRetrievalState::kIsLoadingPredictions;
  last_queried_form_global_id_ = form.global_id();
  if (kExtractAXTreeForPredictions.Get()) {
    client_->GetAXTree(base::BindOnce(&AutofillAiManager::OnReceivedAXTree,
                                      weak_ptr_factory_.GetWeakPtr(), form,
                                      trigger_field));
  } else {
    optimization_guide::proto::AXTreeUpdate ax_tree_update;
    OnReceivedAXTree(form, trigger_field, std::move(ax_tree_update));
  }
}

void AutofillAiManager::OnReceivedAXTree(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  client_->GetModelExecutor()->GetPredictions(
      form, GetFieldFillingEligibilityMap(form),
      GetFieldValueSensitivityMap(form), std::move(ax_tree_update),
      base::BindOnce(&AutofillAiManager::OnReceivedPredictions,
                     weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
}

void AutofillAiManager::OnReceivedPredictions(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    AutofillAiModelExecutor::PredictionsOrError predictions_or_error,
    std::optional<std::string> model_execution_id) {
  LOG_AF(GetLogManager()) << LoggingScope::kAutofillAi
                          << LogMessage::kAutofillAi
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

  form_filling_predictions_model_execution_id_ = model_execution_id;

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
      if (HasImprovedPredictionsForField(trigger_field)) {
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

void AutofillAiManager::UserFeedbackReceived(UserFeedback feedback) {
  if (form_filling_predictions_model_execution_id_ &&
      feedback == UserFeedback::kThumbsDown) {
    client_->TryToOpenFeedbackPage(
        *form_filling_predictions_model_execution_id_);
  }
}

void AutofillAiManager::SaveAutofillPredictionsUserFeedbackReceived(
    const std::string& model_execution_id,
    UserFeedback feedback) {
  if (feedback == UserFeedback::kThumbsDown) {
    client_->TryToOpenFeedbackPage(model_execution_id);
  }
}

// TODO(crbug.com/362468426): Rename this method to
// `UserClickedManagePredictionsImprovements()`.
void AutofillAiManager::UserClickedLearnMore() {
  client_->OpenPredictionImprovementsSettings();
}

bool AutofillAiManager::IsURLEligibleForAutofillAi(const GURL& url) const {
  if (!decider_) {
    return false;
  }

  if (kSkipAllowlist.Get()) {
    return true;
  }

  if (!url.SchemeIs("https")) {
    return false;
  }

  optimization_guide::OptimizationGuideDecision decision =
      decider_->CanApplyOptimization(
          url,
          optimization_guide::proto::AUTOFILL_PREDICTION_IMPROVEMENTS_ALLOWLIST,
          /*optimization_metadata=*/nullptr);
  return decision == optimization_guide::OptimizationGuideDecision::kTrue;
}

void AutofillAiManager::OnClickedTriggerSuggestion(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    UpdateSuggestionsCallback update_suggestions_callback) {
  // Reset the manager's state. This is necessary because the trigger suggestion
  // may have been shown as a last resort after a failed prediction retrieval.
  // In this case, the manager might contain stale state (e.g. error state,
  // previous predictions) that needs to be cleared before starting a new
  // retrieval.
  Reset();
  RetrievePredictions(form, trigger_field,
                      std::move(update_suggestions_callback),
                      /*update_to_loading_suggestion=*/true);
}

void AutofillAiManager::OnLoadingSuggestionShown(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    AutofillAiManager::UpdateSuggestionsCallback update_suggestions_callback) {
  logger_.OnTriggeredFillingSuggestions(form.global_id());
  if (kTriggerAutomatically.Get() &&
      prediction_retrieval_state_ !=
          PredictionRetrievalState::kIsLoadingPredictions) {
    RetrievePredictions(form, trigger_field,
                        std::move(update_suggestions_callback),
                        /*update_to_loading_suggestion=*/false);
  } else if (prediction_retrieval_state_ ==
             PredictionRetrievalState::kIsLoadingPredictions) {
    // Update the `update_suggestions_callback_` to the current instance. This
    // is necessary when the loading suggestion was closed (by defocusing the
    // triggering field) and an eligible form field is focused again, while
    // retrieving the predictions is still ongoing. In that case the loading
    // suggestion will be shown again and potentially updated later to error or
    // filling suggestions.
    // Note that this might overwrite the original callback set in
    // `OnClickedTriggerSuggestion()` to one with the same
    // `AutofillClient::SuggestionUiSessionId`, which doesn't matter though.
    update_suggestions_callback_ = std::move(update_suggestions_callback);
  }
}

void AutofillAiManager::OnErrorOrNoInfoSuggestionShown() {
  error_or_no_info_suggestion_shown_ = true;
}

void AutofillAiManager::OnSuggestionsShown(
    const autofill::DenseSet<autofill::SuggestionType>& shown_suggestion_types,
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    UpdateSuggestionsCallback update_suggestions_callback) {
  logger_.OnSuggestionsShown(form.global_id());
  if (shown_suggestion_types.contains(
          autofill::SuggestionType::kPredictionImprovementsLoadingState)) {
    OnLoadingSuggestionShown(form, trigger_field, update_suggestions_callback);
  }
  if (shown_suggestion_types.contains(
          autofill::SuggestionType::kPredictionImprovementsError)) {
    OnErrorOrNoInfoSuggestionShown();
  }
  if (shown_suggestion_types.contains(
          autofill::SuggestionType::kFillPredictionImprovements)) {
    logger_.OnFillingSuggestionsShown(form.global_id());
  }
}

void AutofillAiManager::OnFormSeen(const autofill::FormStructure& form) {
  bool is_eligible = IsFormEligibleForFilling(form);
  logger_.OnFormEligibilityAvailable(form.global_id(), is_eligible);
  if (is_eligible) {
    HasDataStored(base::BindOnce(
        [](base::WeakPtr<AutofillAiManager> manager,
           autofill::FormGlobalId form_id, HasData has_data) {
          if (!manager) {
            return;
          }
          LOG_AF(manager->GetLogManager())
              << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
              << "Has data for " << form_id;
          if (has_data) {
            manager->logger_.OnFormHasDataToFill(form_id);
          }
        },
        weak_ptr_factory_.GetWeakPtr(), form.global_id()));
  }
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
  form_filling_predictions_model_execution_id_ = std::nullopt;
  loading_suggestion_timer_.Stop();
  prediction_retrieval_state_ = PredictionRetrievalState::kReady;
  error_or_no_info_suggestion_shown_ = false;
}

void AutofillAiManager::UpdateSuggestions(
    const std::vector<autofill::Suggestion>& suggestions) {
  loading_suggestion_timer_.Stop();
  if (update_suggestions_callback_.is_null()) {
    return;
  }
  update_suggestions_callback_.Run(
      suggestions,
      autofill::AutofillSuggestionTriggerSource::kPredictionImprovements);
}

void AutofillAiManager::MaybeImportForm(
    std::unique_ptr<autofill::FormStructure> form,
    base::OnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                            bool autofill_ai_shows_bubble)> autofill_callback) {
  user_annotations::ImportFormCallback callback = base::BindOnce(
      [](base::WeakPtr<AutofillAiManager> self,
         base::OnceCallback<void(std::unique_ptr<autofill::FormStructure> form,
                                 bool autofill_ai_shows_bubble)>
             autofill_callback,
         std::unique_ptr<autofill::FormStructure> form,
         std::unique_ptr<user_annotations::FormAnnotationResponse>
             form_annotation_response,
         user_annotations::PromptAcceptanceCallback
             prompt_acceptance_callback) {
        const bool autofill_ai_shows_bubble =
            self && form_annotation_response &&
            !form_annotation_response->to_be_upserted_entries.empty();
        LOG_AF(self ? self->GetLogManager() : nullptr)
            << LoggingScope::kAutofillAi << LogMessage::kAutofillAi << "Form "
            << form->global_id() << " submission is "
            << (autofill_ai_shows_bubble ? "" : "not ")
            << "showing import bubble";
        if (autofill_ai_shows_bubble) {
          self->client_->ShowSaveAutofillAiBubble(
              std::move(form_annotation_response),
              std::move(prompt_acceptance_callback));
        }
        std::move(autofill_callback)
            .Run(std::move(form), autofill_ai_shows_bubble);
      },
      GetWeakPtr(), std::move(autofill_callback));

  user_annotations::UserAnnotationsService* annotation_service =
      client_->GetUserAnnotationsService();

  // Apply the filter rules to mark potentially sensitive values.
  FilterSensitiveValues(*form.get());

  bool skip_import = false;

  if (!client_->IsAutofillAiEnabledPref()) {
    // `autofill::prefs::kAutofillAiEnabled` is disabled.
    skip_import = true;
    LOG_AF(GetLogManager()) << LoggingScope::kAutofillAi
                            << LogMessage::kAutofillAi << "Pref is disabled";
  } else if (!annotation_service) {
    // The import is skipped because the annotation service is not available.
    skip_import = true;
    LOG_AF(GetLogManager())
        << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
        << "Annotation service is not available";
  } else if (!annotation_service->ShouldAddFormSubmissionForURL(
                 form->source_url())) {
    // The import is disabled because the origin criteria is not fulfilled.
    skip_import = true;
    LOG_AF(GetLogManager())
        << LoggingScope::kAutofillAi << LogMessage::kAutofillAi << "Form "
        << form->global_id() << " is ineligible due to URL "
        << form->source_url();
  } else if (!IsFormEligibleForImportByFieldCriteria(*form.get())) {
    // The form does not contain enough values that can be imported.
    skip_import = true;
    LOG_AF(GetLogManager())
        << LoggingScope::kAutofillAi << LogMessage::kAutofillAi << "Form "
        << form->global_id() << " is ineligible due to field criteria.";
  }

  if (skip_import) {
    std::move(callback).Run(std::move(form),
                            /*form_annotation_response=*/nullptr,
                            /*prompt_acceptance_callback=*/base::DoNothing());
    return;
  }
  GURL url = kSendTitleURL.Get() ? client_->GetLastCommittedURL()
                                 : client_->GetLastCommittedOrigin().GetURL();

  if (user_annotations::ShouldExtractAXTreeForFormsAnnotations()) {
    // TODO(crbug.com/366222226): Ensure the AX tree retrieval is not delayed,
    // e.g. by async filters added in future.
    client_->GetAXTree(base::BindOnce(
        &AutofillAiManager::OnReceivedAXTreeForFormImport,
        weak_ptr_factory_.GetWeakPtr(), url,
        kSendTitleURL.Get() ? client_->GetTitle() : std::string(),
        std::move(form), std::move(callback)));
  } else {
    OnReceivedAXTreeForFormImport(
        url, kSendTitleURL.Get() ? client_->GetTitle() : std::string(),
        std::move(form), std::move(callback),
        optimization_guide::proto::AXTreeUpdate());
  }
}

void AutofillAiManager::OnReceivedAXTreeForFormImport(
    const GURL& url,
    const std::string& title,
    std::unique_ptr<autofill::FormStructure> form,
    user_annotations::ImportFormCallback callback,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  if (user_annotations::UserAnnotationsService* user_annotations_service =
          client_->GetUserAnnotationsService()) {
    user_annotations_service->AddFormSubmission(
        url, title, std::move(ax_tree_update), std::move(form),
        std::move(callback));
    return;
  }
  std::move(callback).Run(std::move(form),
                          /*form_annotation_response=*/nullptr,
                          base::DoNothing());
}

void AutofillAiManager::HasDataStored(HasDataCallback callback) {
  if (user_annotations::UserAnnotationsService* user_annotations_service =
          client_->GetUserAnnotationsService()) {
    user_annotations_service->RetrieveAllEntries(base::BindOnce(
        [](base::WeakPtr<AutofillAiManager> self, HasDataCallback callback,
           const user_annotations::UserAnnotationsEntries entries) {
          LOG_AF(self ? self->GetLogManager() : nullptr)
              << LoggingScope::kAutofillAi << LogMessage::kAutofillAi
              << "Received user annotation entries:" << [&] {
                   LogBuffer buffer;
                   buffer << autofill::Tag{"table"};
                   for (const optimization_guide::proto::UserAnnotationsEntry&
                            entry : entries) {
                     buffer << autofill::Tr{} << entry.entry_id() << entry.key()
                            << entry.value()
                            << autofill::SetParentTagContainsPII{};
                   }
                   buffer << autofill::CTag{"table"};
                   return buffer;
                 }();
          std::move(callback).Run(HasData(!entries.empty()));
        },
        GetWeakPtr(), std::move(callback)));
    return;
  }
  std::move(callback).Run(HasData(false));
}

bool AutofillAiManager::ShouldDisplayIph(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field) const {
  // Iph can be shown if:
  // 1. The pref is off.
  // 2. The user can access the feature (for example the experiment flag is on).
  // 2. The focused form/field can trigger the feature.
  // 3. The current domain can trigger the feature.
  return !client_->IsAutofillAiEnabledPref() && IsUserEligible() &&
         IsFormAndFieldEligible(form, field) &&
         IsURLEligibleForAutofillAi(form.main_frame_origin().GetURL());
}

void AutofillAiManager::GoToSettings() const {
  client_->OpenPredictionImprovementsSettings();
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

autofill::LogManager* AutofillAiManager::GetLogManager() const {
  return client_->GetAutofillClient().GetLogManager();
}

base::WeakPtr<AutofillAiManager> AutofillAiManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill_ai

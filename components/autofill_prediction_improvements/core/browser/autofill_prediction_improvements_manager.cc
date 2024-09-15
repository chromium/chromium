// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_utils.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"

namespace autofill_prediction_improvements {

namespace {

// Define `field_types_to_fill` as Autofill address types +
// `IMPROVED_PREDICTION`.
// TODO(crbug.com/364808228): Remove `UNKNOWN_TYPE` from `field_types_to_fill`.
// Also see TODO below.
autofill::FieldTypeSet GetFieldTypesToFill() {
  autofill::FieldTypeSet field_types_to_fill = {autofill::UNKNOWN_TYPE,
                                                autofill::IMPROVED_PREDICTION};
  for (autofill::FieldType field_type : autofill::kAllFieldTypes) {
    if (IsAddressType(field_type)) {
      field_types_to_fill.insert(field_type);
    }
  }
  return field_types_to_fill;
}

// Ignore `FieldFillingSkipReason::kNoFillableGroup` during filling because
// `kFieldTypesToFill` contains `UNKNOWN_TYPE` which would result in false
// positives.
// TODO(crbug.com/364808228): Remove.
constexpr autofill::DenseSet<autofill::FieldFillingSkipReason>
    kIgnoreableSkipReasons = {
        autofill::FieldFillingSkipReason::kNoFillableGroup};

// Returns a field-by-field filling suggestion for `filled_field`, meant to be
// added to another suggestion's `autofill::Suggestion::children`.
autofill::Suggestion CreateChildSuggestionForFilling(
    const autofill::FormFieldData& filled_field) {
  autofill::Suggestion child_suggestion(
      filled_field.value(),
      autofill::SuggestionType::kFillPredictionImprovements);
  child_suggestion.payload =
      autofill::Suggestion::ValueToFill(filled_field.value());
  child_suggestion.labels.emplace_back();
  child_suggestion.labels.back().emplace_back(filled_field.label().empty()
                                                  ? filled_field.placeholder()
                                                  : filled_field.label());
  return child_suggestion;
}

// Creates a spinner-like suggestion shown while improved predictions are
// loaded.
std::vector<autofill::Suggestion> CreateLoadingSuggestion() {
  // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd file.
  autofill::Suggestion loading_suggestion(
      u"Loading",
      autofill::SuggestionType::kPredictionImprovementsLoadingState);
  loading_suggestion.is_acceptable = false;
  loading_suggestion.is_loading = autofill::Suggestion::IsLoading(true);
  return {loading_suggestion};
}

// Creates a suggestion shown when retrieving prediction improvements wasn't
// successful.
std::vector<autofill::Suggestion> CreateErrorSuggestion() {
  // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd file.
  autofill::Suggestion error_suggestion(
      u"Error", autofill::SuggestionType::kAutocompleteEntry);
  error_suggestion.is_acceptable = false;
  return {error_suggestion};
}

}  // namespace

AutofillPredictionImprovementsManager::AutofillPredictionImprovementsManager(
    AutofillPredictionImprovementsClient* client,
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

bool AutofillPredictionImprovementsManager::IsFormBlockedForImport(
    const autofill::FormStructure& form) const {
  if (!user_annotation_prompt_strike_database_) {
    return true;
  }

  return user_annotation_prompt_strike_database_->ShouldBlockFeature(
      AutofillPrectionImprovementsAnnotationPromptStrikeDatabaseTraits::GetId(
          form.form_signature()));
}
void AutofillPredictionImprovementsManager::AddStrikeForImportFromForm(
    const autofill::FormStructure& form) {
  if (!user_annotation_prompt_strike_database_) {
    return;
  }

  user_annotation_prompt_strike_database_->AddStrike(
      AutofillPrectionImprovementsAnnotationPromptStrikeDatabaseTraits::GetId(
          form.form_signature()));
}

void AutofillPredictionImprovementsManager::RemoveStrikesForImportFromForm(
    const autofill::FormStructure& form) {
  if (!user_annotation_prompt_strike_database_) {
    return;
  }

  user_annotation_prompt_strike_database_->ClearStrikes(
      AutofillPrectionImprovementsAnnotationPromptStrikeDatabaseTraits::GetId(
          form.form_signature()));
}

AutofillPredictionImprovementsManager::
    ~AutofillPredictionImprovementsManager() = default;

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::CreateFillingSuggestion(
    const autofill::FormFieldData& field) {
  if (!cache_) {
    return {};
  }
  const autofill::FormFieldData* filled_field =
      (*cache_).FindFieldByGlobalId(field.global_id());
  if (!filled_field) {
    return {};
  }
  const std::u16string predicted_value = filled_field->value();

  autofill::Suggestion suggestion(
      predicted_value, autofill::SuggestionType::kFillPredictionImprovements);
  auto payload = autofill::Suggestion::PredictionImprovementsPayload(
      GetValuesToFill(), GetFieldTypesToFill(), kIgnoreableSkipReasons);
  // Add a `kFillPredictionImprovements` suggestion with a separator to
  // `suggestion.children` before the field-by-field filling entries.
  {
    // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd
    // file.
    autofill::Suggestion fill_all_child(
        u"Fill all", autofill::SuggestionType::kFillPredictionImprovements);
    fill_all_child.payload = payload;
    suggestion.children.emplace_back(fill_all_child);
    suggestion.children.emplace_back(autofill::SuggestionType::kSeparator);
  }
  // Add the child suggestion for the triggering field on top.
  suggestion.children.emplace_back(
      CreateChildSuggestionForFilling(*filled_field));
  // Then add child suggestions for all remaining, non-empty fields.
  for (const auto& cached_field : (*cache_).fields()) {
    // Only add a child suggestion if the field is not the triggering field and
    // the value to fill is not empty.
    if (cached_field.global_id() == filled_field->global_id() ||
        cached_field.value().empty()) {
      continue;
    }
    suggestion.children.emplace_back(
        CreateChildSuggestionForFilling(cached_field));
  }
  if (!suggestion.children.empty()) {
    suggestion.labels.emplace_back();
    // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd
    // file.
    suggestion.labels.back().emplace_back(u"& more");
  }
  autofill::Suggestion feedback_suggestion(
      autofill::SuggestionType::kPredictionImprovementsFeedback);
  feedback_suggestion.is_acceptable = false;
  suggestion.payload = payload;
  return {suggestion, feedback_suggestion};
}

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::CreateTriggerSuggestion(
    bool add_separator) {
  std::vector<autofill::Suggestion> suggestions;
  if (add_separator) {
    suggestions.emplace_back(autofill::SuggestionType::kSeparator);
  }
  // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd file.
  autofill::Suggestion retrieve_suggestion(
      u"Autocomplete",
      autofill::SuggestionType::kRetrievePredictionImprovements);
  retrieve_suggestion.icon = autofill::Suggestion::Icon::kSettings;
  suggestions.emplace_back(retrieve_suggestion);

  autofill::Suggestion details_suggestion(
      autofill::SuggestionType::kPredictionImprovementsDetails);
  details_suggestion.is_acceptable = false;
  details_suggestion.voice_over =
      u"Details about prediction improvements enter to learn more";
  suggestions.emplace_back(details_suggestion);

  return suggestions;
}

bool AutofillPredictionImprovementsManager::HasImprovedPredictionsForField(
    const autofill::FormFieldData& field) {
  if (!cache_) {
    return false;
  }
  return (*cache_).FindFieldByGlobalId(field.global_id());
}

bool AutofillPredictionImprovementsManager::IsFormEligible(
    const autofill::FormStructure& form) {
  if (!IsFormEligibleByFieldCriteria(form)) {
    return false;
  }

  return ShouldProvidePredictionImprovements(form.main_frame_origin().GetURL());
}

bool AutofillPredictionImprovementsManager::MaybeUpdateSuggestions(
    std::vector<autofill::Suggestion>& address_suggestions,
    const autofill::FormFieldData& field,
    bool should_add_trigger_suggestion) {
  // Show a cached prediction improvements filling suggestion for `field` if
  // it exists.
  if (HasImprovedPredictionsForField(field)) {
    address_suggestions = CreateFillingSuggestion(field);
    return true;
  }
  // Add prediction improvements trigger suggestion.
  else if (should_add_trigger_suggestion) {
    if (address_suggestions.empty()) {
      // Set `address_suggestions` to the trigger suggestion.
      address_suggestions = CreateTriggerSuggestion(/*add_separator=*/false);
      return true;
    } else {
      // Expect that there's an `kUndoOrClear` or `kManageAddress` suggestion
      // in `address_suggestions` if `address_suggestions` is not empty. Insert
      // the trigger suggestion for prediction improvements before.
      for (size_t i = 1; i < address_suggestions.size() - 1; ++i) {
        if (address_suggestions[i].type ==
                autofill::SuggestionType::kSeparator &&
            (address_suggestions[i + 1].type ==
                 autofill::SuggestionType::kUndoOrClear ||
             address_suggestions[i + 1].type ==
                 autofill::SuggestionType::kManageAddress)) {
          const std::vector<autofill::Suggestion> trigger_suggestion =
              CreateTriggerSuggestion(/*add_separator=*/true);
          address_suggestions.insert(address_suggestions.begin() + i,
                                     trigger_suggestion.begin(),
                                     trigger_suggestion.end());
          return true;
        }
      }
    }
  }
  return false;
}

void AutofillPredictionImprovementsManager::
    ExtractPredictionImprovementsForFormFields(
        const autofill::FormData& form,
        const autofill::FormFieldData& trigger_field) {
  if (!ShouldProvidePredictionImprovements(client_->GetLastCommittedURL())) {
    UpdateSuggestions(CreateErrorSuggestion());
    return;
  }
  client_->GetAXTree(
      base::BindOnce(&AutofillPredictionImprovementsManager::OnReceivedAXTree,
                     weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
}

void AutofillPredictionImprovementsManager::OnReceivedAXTree(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  client_->GetFillingEngine()->GetPredictions(
      form, std::move(ax_tree_update),
      base::BindOnce(
          &AutofillPredictionImprovementsManager::OnReceivedPredictions,
          weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
}

void AutofillPredictionImprovementsManager::OnReceivedPredictions(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    base::expected<autofill::FormData, bool> improved_predictions) {
  if (!improved_predictions.has_value()) {
    UpdateSuggestions(CreateErrorSuggestion());
    return;
  }

  cache_ = improved_predictions.value();

  UpdateSuggestions(CreateFillingSuggestion(trigger_field));
}

void AutofillPredictionImprovementsManager::UserFeedbackReceived(
    autofill::AutofillPredictionImprovementsDelegate::UserFeedback feedback) {}

void AutofillPredictionImprovementsManager::UserClickedLearnMore() {}

bool AutofillPredictionImprovementsManager::ShouldProvidePredictionImprovements(
    const GURL& url) {
  if (!decider_ || !IsAutofillPredictionImprovementsEnabled()) {
    return false;
  }
  if (ShouldSkipAllowlist()) {
    return true;
  }
  optimization_guide::OptimizationGuideDecision decision =
      decider_->CanApplyOptimization(
          url,
          optimization_guide::proto::AUTOFILL_PREDICTION_IMPROVEMENTS_ALLOWLIST,
          /*optimization_metadata=*/nullptr);
  return decision == optimization_guide::OptimizationGuideDecision::kTrue;
}

base::flat_map<autofill::FieldGlobalId, std::u16string>
AutofillPredictionImprovementsManager::GetValuesToFill() {
  if (!cache_) {
    return {};
  }
  std::vector<std::pair<autofill::FieldGlobalId, std::u16string>>
      values_to_fill((*cache_).fields().size());
  for (size_t i = 0; i < (*cache_).fields().size(); i++) {
    const autofill::FormFieldData& field = (*cache_).fields()[i];
    values_to_fill[i] = {field.global_id(), field.value()};
  }
  return values_to_fill;
}

void AutofillPredictionImprovementsManager::OnClickedTriggerSuggestion(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    UpdateSuggestionsCallback update_suggestions_callback) {
  Reset();
  update_suggestions_callback_ = std::move(update_suggestions_callback);
  UpdateSuggestions(CreateLoadingSuggestion());
  ExtractPredictionImprovementsForFormFields(form, trigger_field);
}

void AutofillPredictionImprovementsManager::Reset() {
  cache_ = std::nullopt;
  update_suggestions_callback_ = base::NullCallback();
}

void AutofillPredictionImprovementsManager::UpdateSuggestions(
    const std::vector<autofill::Suggestion>& suggestions) {
  if (update_suggestions_callback_.is_null()) {
    return;
  }
  update_suggestions_callback_.Run(
      suggestions,
      autofill::AutofillSuggestionTriggerSource::kPredictionImprovements);
}

void AutofillPredictionImprovementsManager::MaybeImportForm(
    const autofill::FormData& form,
    const autofill::FormStructure& form_structure,
    ImportFormCallback callback) {
  // TODO(crbug.com/365962363): Also return early here if
  // `!IsFormEligibleByFieldCriteria(form_structure))` once the parser is
  // implemented.
  if (user_annotations::IsUserAnnotationsObserveFormSubmissionsEnabled() ||
      !client_->GetUserAnnotationsService() ||
      !client_->GetUserAnnotationsService()->ShouldAddFormSubmissionForURL(
          form.url())) {
    std::move(callback).Run(
        /*to_be_upserted_entries=*/{},
        /*prompt_acceptance_callback=*/base::DoNothing());
    return;
  }
  // TODO(crbug.com/366222226): Ensure the AX tree retrieval is not delayed,
  // e.g. by async filters added in future.
  client_->GetAXTree(base::BindOnce(
      &AutofillPredictionImprovementsManager::OnReceivedAXTreeForFormImport,
      weak_ptr_factory_.GetWeakPtr(), form, std::move(callback)));
}

void AutofillPredictionImprovementsManager::OnReceivedAXTreeForFormImport(
    const autofill::FormData& form,
    ImportFormCallback callback,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  if (user_annotations::UserAnnotationsService* user_annotations_service =
          client_->GetUserAnnotationsService()) {
    user_annotations_service->AddFormSubmission(std::move(ax_tree_update), form,
                                                std::move(callback));
    return;
  }
  std::move(callback).Run(/*to_be_upserted_entries=*/{},
                          /*prompt_acceptance_callback=*/base::DoNothing());
}

void AutofillPredictionImprovementsManager::HasDataStored(
    HasDataCallback callback) {
  if (user_annotations::UserAnnotationsService* user_annotations_service =
          client_->GetUserAnnotationsService()) {
    user_annotations_service->RetrieveAllEntries(base::BindOnce(
        [](HasDataCallback callback,
           const user_annotations::UserAnnotationsEntries entries) {
          std::move(callback).Run(HasData(!entries.empty()));
        },
        std::move(callback)));
    return;
  }
  std::move(callback).Run(HasData(false));
}

}  // namespace autofill_prediction_improvements

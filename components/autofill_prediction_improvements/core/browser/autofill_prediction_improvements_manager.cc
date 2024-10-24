// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

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
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_logger.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_utils.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_value_filter.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"

namespace autofill_prediction_improvements {

namespace {

constexpr int kNumberFieldsToShowInSuggestionLabel = 2;

bool IsFormAndFieldEligible(const autofill::FormStructure& form,
                            const autofill::AutofillField& field) {
  return IsFieldEligibleByTypeCriteria(field) && IsFormEligibleForFilling(form);
}

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

// Return the correct prediction improvements icon depending on the current
// theme.
// TODO(crbug.com/372405533): Move this decision inside UI code.
autofill::Suggestion::Icon GetAutofillPredictionImprovementsIcon() {
  return ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
             ? autofill::Suggestion::Icon::kAutofillPredictionImprovementsDark
             : autofill::Suggestion::Icon::kAutofillPredictionImprovements;
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
    const AutofillPredictionImprovementsFillingEngine::Prediction& prediction) {
  const std::u16string& value_to_fill = prediction.select_option_text
                                            ? *prediction.select_option_text
                                            : prediction.value;
  autofill::Suggestion child_suggestion(
      value_to_fill, autofill::SuggestionType::kFillPredictionImprovements);
  child_suggestion.payload = autofill::Suggestion::ValueToFill(value_to_fill);
  child_suggestion.labels.emplace_back();
  child_suggestion.labels.back().emplace_back(prediction.label);
  return child_suggestion;
}

autofill::Suggestion CreateTriggerSuggestion() {
  autofill::Suggestion retrieve_suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_TRIGGER_SUGGESTION_MAIN_TEXT),
      autofill::SuggestionType::kRetrievePredictionImprovements);
  retrieve_suggestion.icon = GetAutofillPredictionImprovementsIcon();
  return retrieve_suggestion;
}

// Creates a spinner-like suggestion shown while improved predictions are
// loaded.
autofill::Suggestion CreateLoadingSuggestion() {
  autofill::Suggestion loading_suggestion(
      autofill::SuggestionType::kPredictionImprovementsLoadingState);
  loading_suggestion.trailing_icon = GetAutofillPredictionImprovementsIcon();
  loading_suggestion.is_acceptable = false;
  return loading_suggestion;
}

autofill::Suggestion CreateFeedbackSuggestion() {
  autofill::Suggestion feedback_suggestion(
      autofill::SuggestionType::kPredictionImprovementsFeedback);
  feedback_suggestion.is_acceptable = false;
  feedback_suggestion.voice_over = base::JoinString(
      {
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_DETAILS),
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_TEXT,
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_SUGGESTION_MANAGE_LINK_A11Y_HINT)),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_SUGGESTION_FEEDBACK_BUTTONS_A11Y_HINT),
      },
      u" ");
  feedback_suggestion.highlight_on_select = false;
  return feedback_suggestion;
}

autofill::Suggestion CreateEditPredictionImprovementsInformation() {
  autofill::Suggestion edit_suggestion;
  edit_suggestion.type =
      autofill::SuggestionType::kEditPredictionImprovementsInformation;
  edit_suggestion.icon = autofill::Suggestion::Icon::kEdit;
  edit_suggestion.main_text = autofill::Suggestion::Text(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_EDIT_INFORMATION_SUGGESTION_MAIN_TEXT),
      autofill::Suggestion::Text::IsPrimary(true));
  return edit_suggestion;
}

// Creates suggestions shown when retrieving prediction improvements wasn't
// successful or there's nothing to fill (not even by Autofill or Autocomplete).
std::vector<autofill::Suggestion> CreateErrorOrNoInfoSuggestions(
    int message_id) {
  autofill::Suggestion error_suggestion(
      autofill::SuggestionType::kPredictionImprovementsError);
  error_suggestion.main_text = autofill::Suggestion::Text(
      l10n_util::GetStringUTF16(message_id),
      autofill::Suggestion::Text::IsPrimary(true),
      autofill::Suggestion::Text::ShouldTruncate(true));
  error_suggestion.highlight_on_select = false;
  error_suggestion.is_acceptable = false;
  return {error_suggestion,
          autofill::Suggestion(autofill::SuggestionType::kSeparator),
          CreateFeedbackSuggestion()};
}

// Creates a suggestion shown when retrieving prediction improvements wasn't
// successful.
std::vector<autofill::Suggestion> CreateErrorSuggestions() {
  return CreateErrorOrNoInfoSuggestions(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_ERROR_POPUP_MAIN_TEXT);
}

// Creates suggestions shown when there's nothing to fill (not even by Autofill
// or Autocomplete).
std::vector<autofill::Suggestion> CreateNoInfoSuggestions() {
  return CreateErrorOrNoInfoSuggestions(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_NO_INFO_POPUP_MAIN_TEXT);
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

base::flat_map<autofill::FieldGlobalId, bool>
AutofillPredictionImprovementsManager::GetFieldFillingEligibilityMap(
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
AutofillPredictionImprovementsManager::GetFieldValueSensitivityMap(
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

AutofillPredictionImprovementsManager::
    ~AutofillPredictionImprovementsManager() = default;

bool AutofillPredictionImprovementsManager::CacheHasMatchingAutofillSuggestion(
    const autofill::FormData& form,
    const std::string& autofill_profile_guid,
    autofill::FieldType field_type) {
  autofill::FormStructure* form_structure =
      client_->GetCachedFormStructure(form);
  if (!form_structure) {
    return false;
  }
  for (const std::unique_ptr<autofill::AutofillField>& autofill_field :
       form_structure->fields()) {
    // Skip fields that aren't focusable because they wouldn't be filled
    // anyways.
    if (!autofill_field->IsFocusable()) {
      continue;
    }
    if (autofill_field->Type().GetStorableType() == field_type) {
      const std::u16string normalized_autofill_filling_value =
          autofill::NormalizeValue(
              client_->GetAutofillFillingValue(autofill_profile_guid,
                                               field_type, *autofill_field),
              /*keep_white_space=*/false);
      if (normalized_autofill_filling_value.empty() ||
          !cache_->contains(autofill_field->global_id())) {
        continue;
      }
      const std::u16string normalized_improved_prediction =
          autofill::NormalizeValue(
              cache_->at(autofill_field->global_id()).value,
              /*keep_white_space=*/false);
      if (normalized_improved_prediction == normalized_autofill_filling_value) {
        return true;
      }
    }
  }

  return false;
}

bool AutofillPredictionImprovementsManager::ShouldSkipAutofillSuggestion(
    const autofill::FormData& form,
    const autofill::Suggestion& autofill_suggestion) {
  CHECK(cache_);
  if (autofill_suggestion.type != autofill::SuggestionType::kAddressEntry &&
      autofill_suggestion.type !=
          autofill::SuggestionType::kAddressFieldByFieldFilling) {
    return true;
  }
  const autofill::Suggestion::BackendId backend_id =
      autofill_suggestion.GetPayload<autofill::Suggestion::BackendId>();
  const autofill::Suggestion::Guid* guid =
      absl::get_if<autofill::Suggestion::Guid>(&backend_id);
  const std::string autofill_profile_guid = guid ? guid->value() : "";
  if (guid->value().empty()) {
    return true;
  }

  return CacheHasMatchingAutofillSuggestion(form, autofill_profile_guid,
                                            autofill::FieldType::NAME_FIRST) &&
         CacheHasMatchingAutofillSuggestion(form, autofill_profile_guid,
                                            autofill::FieldType::NAME_LAST);
}

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::CreateFillingSuggestions(
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    const std::vector<autofill::Suggestion>& autofill_suggestions) {
  if (!cache_) {
    return {};
  }
  if (!cache_->contains(field.global_id())) {
    return {};
  }
  const AutofillPredictionImprovementsFillingEngine::Prediction& prediction =
      cache_->at(field.global_id());

  autofill::Suggestion suggestion(
      prediction.value, autofill::SuggestionType::kFillPredictionImprovements);
  auto payload = autofill::Suggestion::PredictionImprovementsPayload(
      GetValuesToFill(), GetFieldTypesToFill(), kIgnoreableSkipReasons);
  suggestion.payload = payload;
  suggestion.icon = GetAutofillPredictionImprovementsIcon();
  // Add a `kFillPredictionImprovements` suggestion with a separator to
  // `suggestion.children` before the field-by-field filling entries.
  {
    autofill::Suggestion fill_all_child(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_ALL_MAIN_TEXT),
        autofill::SuggestionType::kFillPredictionImprovements);
    fill_all_child.payload = payload;
    suggestion.children.emplace_back(fill_all_child);
    suggestion.children.emplace_back(autofill::SuggestionType::kSeparator);
  }
  // Add the child suggestion for the triggering field on top.
  suggestion.children.emplace_back(CreateChildSuggestionForFilling(prediction));
  // Initialize as 1 because of the suggestion added above.
  size_t n_fields_to_fill = 1;
  // The label depends on the fields that will be filled.
  std::u16string label =
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_LABEL_TEXT) +
      u" " + prediction.label;
  for (const auto& [child_field_global_id, child_prediction] : *cache_) {
    // Only add a child suggestion if the field is not the triggering field
    // and the value to fill is not empty.
    if (child_field_global_id == field.global_id() ||
        child_prediction.value.empty()) {
      continue;
    }
    suggestion.children.emplace_back(
        CreateChildSuggestionForFilling(child_prediction));
    ++n_fields_to_fill;
    if (n_fields_to_fill == 2) {
      label += l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_LABEL_SEPARATOR) +
               child_prediction.label;
    }
  }

  suggestion.children.emplace_back(autofill::SuggestionType::kSeparator);
  suggestion.children.emplace_back(
      CreateEditPredictionImprovementsInformation());

  if (n_fields_to_fill > kNumberFieldsToShowInSuggestionLabel) {
    // When more than `kNumberFieldsToShowInSuggestionLabel` are filled,
    // include the "& More".
    size_t number_of_more_fields_to_fill =
        n_fields_to_fill - kNumberFieldsToShowInSuggestionLabel;
    const std::u16string more_fields_label_substr =
        number_of_more_fields_to_fill > 1
            ? l10n_util::GetStringFUTF16(
                  IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_SUGGESTION_AND_N_MORE_FIELDS,
                  base::NumberToString16(number_of_more_fields_to_fill))
            : l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_SUGGESTION_AND_ONE_MORE_FIELD);
    label = base::StrCat({label, u" ", more_fields_label_substr});
  }
  suggestion.labels.emplace_back();
  suggestion.labels.back().emplace_back(label);

  // TODO(crbug.com/365512352): Figure out how to handle Undo suggestion.
  std::vector<autofill::Suggestion> filling_suggestions = {suggestion};
  for (const autofill::Suggestion& autofill_suggestion : autofill_suggestions) {
    if (ShouldSkipAutofillSuggestion(form, autofill_suggestion)) {
      continue;
    }
    filling_suggestions.push_back(autofill_suggestion);
  }
  filling_suggestions.emplace_back(autofill::SuggestionType::kSeparator);
  filling_suggestions.emplace_back(CreateFeedbackSuggestion());
  return filling_suggestions;
}

bool AutofillPredictionImprovementsManager::HasImprovedPredictionsForField(
    const autofill::FormFieldData& field) {
  if (!cache_) {
    return false;
  }
  return cache_->contains(field.global_id());
}

bool AutofillPredictionImprovementsManager::IsPredictionImprovementsEligible(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field) const {
  return IsFormAndFieldEligible(form, field) &&
         ShouldProvidePredictionImprovements(form.main_frame_origin().GetURL());
}

bool AutofillPredictionImprovementsManager::IsUserEligible() const {
  return client_->IsUserEligible();
}

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::GetSuggestions(
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
        return {CreateLoadingSuggestion()};
      }
      return {CreateTriggerSuggestion()};
    case PredictionRetrievalState::kIsLoadingPredictions:
      // Keep showing the loading suggestion while prediction improvements are
      // being retrieved.
      return {CreateLoadingSuggestion()};
    case PredictionRetrievalState::kDoneSuccess:
      // Show a cached prediction improvements filling suggestion for `field` if
      // it exists. This may contain additional `autofill_suggestions`, appended
      // to the prediction improvements.
      if (HasImprovedPredictionsForField(field)) {
        return CreateFillingSuggestions(form, field, autofill_suggestions);
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
      return error_or_no_info_suggestion_shown_
                 ? std::vector<autofill::Suggestion>{CreateTriggerSuggestion()}
                 : std::vector<autofill::Suggestion>{CreateNoInfoSuggestions()};
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
      return error_or_no_info_suggestion_shown_
                 ? std::vector<autofill::Suggestion>{CreateTriggerSuggestion()}
                 : CreateErrorSuggestions();
  }
}

void AutofillPredictionImprovementsManager::RetrievePredictions(
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
    UpdateSuggestions({CreateLoadingSuggestion()});
  }
  prediction_retrieval_state_ = PredictionRetrievalState::kIsLoadingPredictions;
  last_queried_form_global_id_ = form.global_id();
  if (kExtractAXTreeForPredictions.Get()) {
    client_->GetAXTree(
        base::BindOnce(&AutofillPredictionImprovementsManager::OnReceivedAXTree,
                       weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
  } else {
    optimization_guide::proto::AXTreeUpdate ax_tree_update;
    OnReceivedAXTree(form, trigger_field, std::move(ax_tree_update));
  }
}

void AutofillPredictionImprovementsManager::OnReceivedAXTree(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  client_->GetFillingEngine()->GetPredictions(
      form, GetFieldFillingEligibilityMap(form),
      GetFieldValueSensitivityMap(form), std::move(ax_tree_update),
      base::BindOnce(
          &AutofillPredictionImprovementsManager::OnReceivedPredictions,
          weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
}

void AutofillPredictionImprovementsManager::OnReceivedPredictions(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    AutofillPredictionImprovementsFillingEngine::PredictionsOrError
        predictions_or_error,
    std::optional<std::string> feedback_id) {
  feedback_id_ = feedback_id;

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
      base::BindRepeating(&AutofillPredictionImprovementsManager::
                              UpdateSuggestionsAfterReceivedPredictions,
                          weak_ptr_factory_.GetWeakPtr(), form, trigger_field));
}

void AutofillPredictionImprovementsManager::
    UpdateSuggestionsAfterReceivedPredictions(
        const autofill::FormData& form,
        const autofill::FormFieldData& trigger_field) {
  switch (prediction_retrieval_state_) {
    case PredictionRetrievalState::kDoneSuccess:
      // TODO(crbug.com/365512352): CHECK that `cache_` should not be null here.
      if (cache_ && !cache_->contains(trigger_field.global_id())) {
        OnFailedToGenerateSuggestions();
      } else {
        UpdateSuggestions(CreateFillingSuggestions(form, trigger_field,
                                                   autofill_suggestions_));
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

void AutofillPredictionImprovementsManager::UserFeedbackReceived(
    autofill::AutofillPredictionImprovementsDelegate::UserFeedback feedback) {
  if (feedback_id_ && feedback ==
                          autofill::AutofillPredictionImprovementsDelegate::
                              UserFeedback::kThumbsDown) {
    client_->TryToOpenFeedbackPage(*feedback_id_);
  }
}

// TODO(crbug.com/362468426): Rename this method to
// `UserClickedManagePredictionsImprovements()`.
void AutofillPredictionImprovementsManager::UserClickedLearnMore() {
  client_->OpenPredictionImprovementsSettings();
}

bool AutofillPredictionImprovementsManager::
    IsURLEligibleForPredictionImprovements(const GURL& url) const {
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

bool AutofillPredictionImprovementsManager::ShouldProvidePredictionImprovements(
    const GURL& url) const {
  return client_->IsAutofillPredictionImprovementsEnabledPref() &&
         IsUserEligible() && IsURLEligibleForPredictionImprovements(url);
}

base::flat_map<autofill::FieldGlobalId, std::u16string>
AutofillPredictionImprovementsManager::GetValuesToFill() {
  if (!cache_) {
    return {};
  }
  std::vector<std::pair<autofill::FieldGlobalId, std::u16string>>
      values_to_fill(cache_->size());
  size_t i = 0;
  for (const auto& [field_global_id, prediction] : *cache_) {
    values_to_fill[i++] = {field_global_id, prediction.value};
  }
  return values_to_fill;
}

void AutofillPredictionImprovementsManager::OnClickedTriggerSuggestion(
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

void AutofillPredictionImprovementsManager::OnLoadingSuggestionShown(
    const autofill::FormData& form,
    const autofill::FormFieldData& trigger_field,
    AutofillPredictionImprovementsManager::UpdateSuggestionsCallback
        update_suggestions_callback) {
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

void AutofillPredictionImprovementsManager::OnErrorOrNoInfoSuggestionShown() {
  error_or_no_info_suggestion_shown_ = true;
}

void AutofillPredictionImprovementsManager::OnSuggestionsShown(
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

void AutofillPredictionImprovementsManager::OnFormSeen(
    const autofill::FormStructure& form) {
  bool is_eligible = IsFormEligibleForFilling(form);
  logger_.OnFormEligibilityAvailable(form.global_id(), is_eligible);
  if (is_eligible) {
    HasDataStored(base::BindOnce(
        [](base::WeakPtr<AutofillPredictionImprovementsManager> manager,
           autofill::FormGlobalId form_id, HasData has_data) {
          if (manager && has_data) {
            manager->logger_.OnFormHasDataToFill(form_id);
          }
        },
        weak_ptr_factory_.GetWeakPtr(), form.global_id()));
  }
}

void AutofillPredictionImprovementsManager::OnDidFillSuggestion(
    autofill::FormGlobalId form_id) {
  logger_.OnDidFillSuggestion(form_id);
}

void AutofillPredictionImprovementsManager::OnEditedAutofilledField(
    autofill::FormGlobalId form_id) {
  logger_.OnDidCorrectFillingSuggestion(form_id);
}

void AutofillPredictionImprovementsManager::Reset() {
  cache_ = std::nullopt;
  last_queried_form_global_id_ = std::nullopt;
  update_suggestions_callback_ = base::NullCallback();
  feedback_id_ = std::nullopt;
  loading_suggestion_timer_.Stop();
  prediction_retrieval_state_ = PredictionRetrievalState::kReady;
  error_or_no_info_suggestion_shown_ = false;
}

void AutofillPredictionImprovementsManager::UpdateSuggestions(
    const std::vector<autofill::Suggestion>& suggestions) {
  loading_suggestion_timer_.Stop();
  if (update_suggestions_callback_.is_null()) {
    return;
  }
  update_suggestions_callback_.Run(
      suggestions,
      autofill::AutofillSuggestionTriggerSource::kPredictionImprovements);
}

void AutofillPredictionImprovementsManager::MaybeImportForm(
    std::unique_ptr<autofill::FormStructure> form,
    user_annotations::ImportFormCallback callback) {
  user_annotations::UserAnnotationsService* annotation_service =
      client_->GetUserAnnotationsService();

  // Apply the filter rules to mark potentially sensitive values.
  FilterSensitiveValues(*form.get());

  bool skip_import = false;

  if (!client_->IsAutofillPredictionImprovementsEnabledPref()) {
    // `autofill::prefs::kAutofillPredictionImprovementsEnabled` is disabled.
    skip_import = true;
  } else if (!annotation_service) {
    // The import is skipped because the annotation service is not available.
    skip_import = true;
  } else if (!annotation_service->ShouldAddFormSubmissionForURL(
                 form->source_url())) {
    // The import is disabled because the origin criteria is not fulfilled.
    skip_import = true;
  } else if (!IsFormEligibleForImportByFieldCriteria(*form.get())) {
    // The form does not contain enough values that can be imported.
    skip_import = true;
  }

  if (skip_import) {
    std::move(callback).Run(std::move(form),
                            /*to_be_upserted_entries=*/{},
                            /*prompt_acceptance_callback=*/base::DoNothing());
    return;
  }

  if (user_annotations::ShouldExtractAXTreeForFormsAnnotations()) {
    // TODO(crbug.com/366222226): Ensure the AX tree retrieval is not delayed,
    // e.g. by async filters added in future.
    client_->GetAXTree(base::BindOnce(
        &AutofillPredictionImprovementsManager::OnReceivedAXTreeForFormImport,
        weak_ptr_factory_.GetWeakPtr(), client_->GetLastCommittedURL(),
        client_->GetTitle(), std::move(form), std::move(callback)));
  } else {
    OnReceivedAXTreeForFormImport(
        client_->GetLastCommittedURL(), client_->GetTitle(), std::move(form),
        std::move(callback), optimization_guide::proto::AXTreeUpdate());
  }
}

void AutofillPredictionImprovementsManager::OnReceivedAXTreeForFormImport(
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
  std::move(callback).Run(std::move(form), /*to_be_upserted_entries=*/{},
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

bool AutofillPredictionImprovementsManager::ShouldDisplayIph(
    const autofill::FormStructure& form,
    const autofill::AutofillField& field) const {
  // Iph can be shown if:
  // 1. The pref is off.
  // 2. The user can access the feature (for example the experiment flag is on).
  // 2. The focused form/field can trigger the feature.
  // 3. The current domain can trigger the feature.
  return !client_->IsAutofillPredictionImprovementsEnabledPref() &&
         IsUserEligible() && IsFormAndFieldEligible(form, field) &&
         IsURLEligibleForPredictionImprovements(
             form.main_frame_origin().GetURL());
}

void AutofillPredictionImprovementsManager::GoToSettings() const {
  client_->OpenPredictionImprovementsSettings();
}

void AutofillPredictionImprovementsManager::OnFailedToGenerateSuggestions() {
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
      NOTREACHED_NORETURN();
    case PredictionRetrievalState::kDoneSuccess:
      UpdateSuggestions(CreateNoInfoSuggestions());
      break;
    case PredictionRetrievalState::kDoneError:
      UpdateSuggestions(CreateErrorSuggestions());
      break;
  }
}

}  // namespace autofill_prediction_improvements

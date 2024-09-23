// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_value_filter.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"
#include "ui/base/l10n/l10n_util.h"

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

autofill::Suggestion CreateFeedbackSuggestion() {
  autofill::Suggestion feedback_suggestion(
      autofill::SuggestionType::kPredictionImprovementsFeedback);
  feedback_suggestion.is_acceptable = false;
  feedback_suggestion.voice_over = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_SUGGESTION_A11Y_HINT);
  feedback_suggestion.highlight_on_select = false;
  return feedback_suggestion;
}

// Creates a suggestion shown when retrieving prediction improvements wasn't
// successful.
std::vector<autofill::Suggestion> CreateErrorSuggestion() {
  autofill::Suggestion error_suggestion(
      autofill::SuggestionType::kPredictionImprovementsError);
  error_suggestion.main_text = autofill::Suggestion::Text(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_ERROR_POPUP_MAIN_TEXT),
      autofill::Suggestion::Text::IsPrimary(true),
      autofill::Suggestion::Text::ShouldTruncate(true));
  error_suggestion.highlight_on_select = false;
  error_suggestion.is_acceptable = false;
  return {error_suggestion, CreateFeedbackSuggestion()};
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
AutofillPredictionImprovementsManager::CreateFillingSuggestions(
    const autofill::FormFieldData& field,
    const std::vector<autofill::Suggestion>& address_suggestions) {
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
  suggestion.payload = payload;
  suggestion.icon = autofill::Suggestion::Icon::kAccount;
  // Add a `kFillPredictionImprovements` suggestion with a separator to
  // `suggestion.children` before the field-by-field filling entries.
  {
    // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd
    // file.
    autofill::Suggestion fill_all_child(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_ALL_MAIN_TEXT),
        autofill::SuggestionType::kFillPredictionImprovements);
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
  suggestion.children.emplace_back(autofill::SuggestionType::kSeparator);
  suggestion.children.emplace_back(CreateFeedbackSuggestion());

  std::vector<autofill::Suggestion> filling_suggestions = {suggestion};
  filling_suggestions.insert(filling_suggestions.end(),
                             address_suggestions.begin(),
                             address_suggestions.end());
  return filling_suggestions;
}

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::CreateTriggerSuggestion() {
  std::vector<autofill::Suggestion> suggestions;
  // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd file.
  autofill::Suggestion retrieve_suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_TRIGGER_SUGGESTION_MAIN_TEXT),
      autofill::SuggestionType::kRetrievePredictionImprovements);
  retrieve_suggestion.icon = autofill::Suggestion::Icon::kSettings;
  suggestions.emplace_back(retrieve_suggestion);

  autofill::Suggestion details_suggestion(
      autofill::SuggestionType::kPredictionImprovementsDetails);
  details_suggestion.is_acceptable = false;
  details_suggestion.highlight_on_select = false;
  details_suggestion.voice_over = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_DETAILS_SUGGESTION_A11Y_HINT);
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
  if (!IsFormEligibleForFillingByFieldCriteria(form)) {
    return false;
  }

  return ShouldProvidePredictionImprovements(form.main_frame_origin().GetURL());
}

bool AutofillPredictionImprovementsManager::MaybeUpdateSuggestions(
    std::vector<autofill::Suggestion>& address_suggestions,
    const autofill::FormFieldData& field,
    bool should_add_trigger_suggestion) {
  loading_suggestion_timer_.Stop();
  // Show a cached prediction improvements filling suggestion for `field` if
  // it exists.
  if (HasImprovedPredictionsForField(field)) {
    address_suggestions = CreateFillingSuggestions(field, address_suggestions);
    return true;
  }
  // Add prediction improvements trigger suggestion.
  else if (should_add_trigger_suggestion) {
    // Store `address_suggestions` to show them with prediction improvements
    // later if the trigger was accepted.
    address_suggestions_ = address_suggestions;
    // Set `address_suggestions` to the trigger suggestion.
    address_suggestions = CreateTriggerSuggestion();
    return true;
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
    base::expected<autofill::FormData, bool> prediction_improvements,
    std::optional<std::string> feedback_id) {
  if (prediction_improvements.has_value()) {
    cache_ = prediction_improvements.value();
    feedback_id_ = feedback_id;
  }

  // Depending on whether predictions where retrieved or not, we need to show
  // the corresponding suggestions. This is delayed a little bit so that we
  // don't see a flickering UI.
  loading_suggestion_timer_.Start(
      FROM_HERE, kMinTimeToShowLoading,
      prediction_improvements.has_value()
          ? base::BindRepeating(
                &AutofillPredictionImprovementsManager::UpdateSuggestions,
                weak_ptr_factory_.GetWeakPtr(),
                CreateFillingSuggestions(trigger_field, address_suggestions_))
          : base::BindRepeating(
                &AutofillPredictionImprovementsManager::UpdateSuggestions,
                weak_ptr_factory_.GetWeakPtr(), CreateErrorSuggestion()));
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

bool AutofillPredictionImprovementsManager::ShouldProvidePredictionImprovements(
    const GURL& url) {
  if (!client_->IsAutofillPredictionImprovementsEnabledPref()) {
    return false;
  }
  if (!decider_ || !IsAutofillPredictionImprovementsEnabled()) {
    return false;
  }
  if (kSkipAllowlist.Get()) {
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
  feedback_id_ = std::nullopt;
  loading_suggestion_timer_.Stop();
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
    ImportFormCallback callback) {
  user_annotations::UserAnnotationsService* annotation_service =
      client_->GetUserAnnotationsService();

  // Apply the filter rules to mark potentially sensitive values.
  FilterSensitiveValues(*form.get());

  bool skip_import = false;

  if (user_annotations::IsUserAnnotationsObserveFormSubmissionsEnabled()) {
    // The import is skipped because importing is done by a different path.
    skip_import = true;
  } else if (!client_->IsAutofillPredictionImprovementsEnabledPref()) {
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

  // TODO(crbug.com/366222226): Ensure the AX tree retrieval is not delayed,
  // e.g. by async filters added in future.
  client_->GetAXTree(base::BindOnce(
      &AutofillPredictionImprovementsManager::OnReceivedAXTreeForFormImport,
      weak_ptr_factory_.GetWeakPtr(), std::move(form), std::move(callback)));
}

void AutofillPredictionImprovementsManager::OnReceivedAXTreeForFormImport(
    std::unique_ptr<autofill::FormStructure> form,
    ImportFormCallback callback,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  if (user_annotations::UserAnnotationsService* user_annotations_service =
          client_->GetUserAnnotationsService()) {
    user_annotations_service->AddFormSubmission(
        std::move(ax_tree_update), std::move(form), std::move(callback));
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

}  // namespace autofill_prediction_improvements

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace autofill_prediction_improvements {

namespace {

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

}  // namespace

AutofillPredictionImprovementsManager::AutofillPredictionImprovementsManager(
    AutofillPredictionImprovementsClient* client,
    optimization_guide::OptimizationGuideDecider* decider)
    : client_(CHECK_DEREF(client)), decider_(decider) {
  if (decider_) {
    decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::
             AUTOFILL_PREDICTION_IMPROVEMENTS_ALLOWLIST});
  }
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
  if (predicted_value.empty()) {
    return {};
  }

  autofill::Suggestion suggestion(
      predicted_value, autofill::SuggestionType::kFillPredictionImprovements);
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
  return {suggestion};
}

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::CreateLoadingSuggestion() {
  // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd file.
  autofill::Suggestion loading_suggestion(
      u"Loading",
      autofill::SuggestionType::kPredictionImprovementsLoadingState);
  loading_suggestion.is_acceptable = false;
  loading_suggestion.is_loading = autofill::Suggestion::IsLoading(true);
  return {loading_suggestion};
}

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::CreateTriggerSuggestion(
    bool add_separator) {
  std::vector<autofill::Suggestion> suggestions;
  if (add_separator) {
    suggestions.emplace_back(autofill::SuggestionType::kSeparator);
  }
  // TODO(crbug.com/361434879): Add hardcoded string to an appropriate grd file.
  autofill::Suggestion suggestion(
      u"Autocomplete",
      autofill::SuggestionType::kRetrievePredictionImprovements);
  suggestion.icon = autofill::Suggestion::Icon::kSettings;
  suggestions.emplace_back(suggestion);
  return suggestions;
}

bool AutofillPredictionImprovementsManager::HasImprovedPredictionsForField(
    const autofill::FormFieldData& field) {
  return true;
}

bool AutofillPredictionImprovementsManager::UsedImprovedPredictionsForField(
    const autofill::FormFieldData& field) {
  return true;
}

void AutofillPredictionImprovementsManager::
    ExtractImprovedPredictionsForFormFields(
        const autofill::FormData& form,
        FillPredictionsCallback fill_callback) {
  if (!ShouldProvidePredictionImprovements(client_->GetLastCommittedURL())) {
    return;
  }
  client_->GetAXTree(base::BindOnce(
      &AutofillPredictionImprovementsManager::OnReceivedAXTree,
      weak_ptr_factory_.GetWeakPtr(), form, std::move(fill_callback)));
}

void AutofillPredictionImprovementsManager::OnReceivedAXTree(
    const autofill::FormData& form,
    FillPredictionsCallback fill_callback,
    optimization_guide::proto::AXTreeUpdate ax_tree_update) {
  client_->GetFillingEngine()->GetPredictions(
      form, std::move(ax_tree_update),
      base::BindOnce(
          &AutofillPredictionImprovementsManager::OnReceivedPredictions,
          weak_ptr_factory_.GetWeakPtr(), std::move(fill_callback)));
}

void AutofillPredictionImprovementsManager::OnReceivedPredictions(
    FillPredictionsCallback fill_callback,
    base::expected<autofill::FormData, bool> filled_form) {
  if (!filled_form.has_value()) {
    // TODO(crbug.com/359440030): Add error handling.
    return;
  }

  for (const autofill::FormFieldData& field : filled_form.value().fields()) {
    fill_callback.Run(autofill::mojom::ActionPersistence::kFill,
                      autofill::mojom::FieldActionType::kReplaceAll,
                      filled_form.value(), field, field.value(),
                      autofill::SuggestionType::kAutocompleteEntry,
                      std::nullopt);
  }
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

}  // namespace autofill_prediction_improvements

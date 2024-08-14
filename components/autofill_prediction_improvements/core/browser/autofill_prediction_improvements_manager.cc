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
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace autofill_prediction_improvements {

AutofillPredictionImprovementsManager::AutofillPredictionImprovementsManager(
    AutofillPredictionImprovementsClient* client)
    : client_(CHECK_DEREF(client)) {}

AutofillPredictionImprovementsManager::
    ~AutofillPredictionImprovementsManager() = default;

std::vector<autofill::Suggestion>
AutofillPredictionImprovementsManager::GetSuggestions(
    const autofill::FormFieldData& field) {
  return {};
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

}  // namespace autofill_prediction_improvements

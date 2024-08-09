// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_manager.h"

#include <string>

#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"

namespace autofill_prediction_improvements {

AutofillPredictionImprovementsManager::AutofillPredictionImprovementsManager(
    AutofillPredictionImprovementsClient* client)
    : client_(*client) {}

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
        base::OnceCallback<void(bool success)> finished_callback) {}

}  // namespace autofill_prediction_improvements

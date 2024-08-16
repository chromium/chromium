// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_filling_engine.h"

namespace autofill_prediction_improvements {

// The class for embedder-independent, tab-specific
// autofill_prediction_improvements logic. This class is an interface.
class AutofillPredictionImprovementsManager
    : public autofill::AutofillPredictionImprovementsDelegate {
 public:
  explicit AutofillPredictionImprovementsManager(
      AutofillPredictionImprovementsClient* client);
  AutofillPredictionImprovementsManager(
      const AutofillPredictionImprovementsManager&) = delete;
  AutofillPredictionImprovementsManager& operator=(
      const AutofillPredictionImprovementsManager&) = delete;
  ~AutofillPredictionImprovementsManager() override;

  // autofill::AutofillPredictionImprovementsDelegate
  std::vector<autofill::Suggestion> GetSuggestions(
      const autofill::FormFieldData& field) override;
  bool HasImprovedPredictionsForField(
      const autofill::FormFieldData& field) override;
  bool UsedImprovedPredictionsForField(
      const autofill::FormFieldData& field) override;
  void ExtractImprovedPredictionsForFormFields(
      const autofill::FormData& form,
      FillPredictionsCallback fill_callback) override;

 private:
  void OnReceivedAXTree(const autofill::FormData& form,
                        FillPredictionsCallback fill_callback,
                        optimization_guide::proto::AXTreeUpdate);

  // The unexpected value is always `false` if there was an error retrieving
  // predictions.
  void OnReceivedPredictions(FillPredictionsCallback fill_callback,
                             base::expected<autofill::FormData, bool>);

  // A raw reference to the client, which owns `this` and therefore outlives it.
  const raw_ref<AutofillPredictionImprovementsClient> client_;

  base::WeakPtrFactory<AutofillPredictionImprovementsManager> weak_ptr_factory_{
      this};
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_

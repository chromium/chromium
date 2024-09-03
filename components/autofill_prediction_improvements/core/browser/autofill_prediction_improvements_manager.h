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
#include "url/gurl.h"

namespace optimization_guide {
class OptimizationGuideDecider;
}

namespace autofill_prediction_improvements {

// The class for embedder-independent, tab-specific
// autofill_prediction_improvements logic. This class is an interface.
class AutofillPredictionImprovementsManager
    : public autofill::AutofillPredictionImprovementsDelegate {
 public:
  AutofillPredictionImprovementsManager(
      AutofillPredictionImprovementsClient* client,
      optimization_guide::OptimizationGuideDecider* decider);
  AutofillPredictionImprovementsManager(
      const AutofillPredictionImprovementsManager&) = delete;
  AutofillPredictionImprovementsManager& operator=(
      const AutofillPredictionImprovementsManager&) = delete;
  ~AutofillPredictionImprovementsManager() override;

  // autofill::AutofillPredictionImprovementsDelegate
  std::vector<autofill::Suggestion> CreateFillingSuggestion(
      const autofill::FormFieldData& field) override;
  bool HasImprovedPredictionsForField(
      const autofill::FormFieldData& field) override;
  bool UsedImprovedPredictionsForField(
      const autofill::FormFieldData& field) override;
  void ExtractImprovedPredictionsForFormFields(
      const autofill::FormData& form,
      FillPredictionsCallback fill_callback) override;
  std::vector<autofill::Suggestion> CreateLoadingSuggestion() override;
  std::vector<autofill::Suggestion> CreateTriggerSuggestion(
      bool add_separator) override;
  bool ShouldProvidePredictionImprovements(const GURL& url) override;
  void UserFeedbackReceived(
      autofill::AutofillPredictionImprovementsDelegate::UserFeedback feedback)
      override;
  void UserClickedLearnMore() override;

 private:
  void OnReceivedAXTree(const autofill::FormData& form,
                        FillPredictionsCallback fill_callback,
                        optimization_guide::proto::AXTreeUpdate);

  // The unexpected value is always `false` if there was an error retrieving
  // predictions.
  void OnReceivedPredictions(FillPredictionsCallback fill_callback,
                             base::expected<autofill::FormData, bool>);

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<AutofillPredictionImprovementsClient> client_;

  // Most recently retrieved form with field values set to prediction
  // improvements.
  // TODO(crbug.com/361414075): Set `cache_` and manage its lifecycle.
  std::optional<autofill::FormData> cache_ = std::nullopt;

  // The `decider_` is used to check if the
  // `AUTOFILL_PREDICTION_IMPROVEMENTS_ALLOWLIST` optimization guide can be
  // applied to the main frame's last committed URL. `decider_` is null if the
  // corresponding feature is not enabled.
  const raw_ptr<optimization_guide::OptimizationGuideDecider> decider_;

  base::WeakPtrFactory<AutofillPredictionImprovementsManager> weak_ptr_factory_{
      this};
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_MANAGER_H_

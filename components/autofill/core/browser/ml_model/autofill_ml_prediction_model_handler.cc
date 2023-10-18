// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"

#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace autofill {

AutofillMlPredictionModelHandler::AutofillMlPredictionModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider)
    : optimization_guide::ModelHandler<
          AutofillModelExecutor::ModelOutput,
          const AutofillModelExecutor::ModelInput&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<AutofillModelExecutor>(),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
          /*model_metadata=*/absl::nullopt) {
  // Store the model in memory as soon as it is available and keep it loaded for
  // the whole browser session since we query predictions very regularly.
  // TODO(crbug.com/1465926): Maybe change both back to default behavior if we
  // see memory regressions during the rollout.
  SetShouldPreloadModel(true);
  SetShouldUnloadModelOnComplete(false);
}
AutofillMlPredictionModelHandler::~AutofillMlPredictionModelHandler() = default;

void AutofillMlPredictionModelHandler::GetModelPredictionsForForm(
    std::unique_ptr<FormStructure> form_structure,
    base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback) {
  if (!ModelAvailable()) {
    // No model, no predictions.
    std::move(callback).Run(std::move(form_structure));
    return;
  }

  // TODO(crbug.com/1465926): Remove `ToFormData()` as it creates a new copy
  // of the FormData.
  AutofillModelExecutor::ModelInput form_data = form_structure->ToFormData();
  ExecuteModelWithInput(
      base::BindOnce(
          [](std::unique_ptr<FormStructure> form_structure,
             base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback,
             const absl::optional<AutofillModelExecutor::ModelOutput>& output) {
            CHECK(output);
            // The model only outputs type for the first
            // `AutofillModelExecutor::kMaxNumberOfFields` many fields.
            CHECK_LE(output->size(), form_structure->field_count());
            for (size_t i = 0; i < output->size(); i++) {
              form_structure->field(i)->set_heuristic_type(
                  HeuristicSource::kMachineLearning, (*output)[i]);
            }
            std::move(callback).Run(std::move(form_structure));
          },
          std::move(form_structure), std::move(callback)),
      std::move(form_data));
}

void AutofillMlPredictionModelHandler::GetModelPredictionsForForms(
    std::vector<std::unique_ptr<FormStructure>> forms,
    base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>
        callback) {
  auto barrier_callback = base::BarrierCallback<std::unique_ptr<FormStructure>>(
      forms.size(), std::move(callback));
  for (std::unique_ptr<FormStructure>& form : forms) {
    GetModelPredictionsForForm(std::move(form), barrier_callback);
  }
}

}  // namespace autofill

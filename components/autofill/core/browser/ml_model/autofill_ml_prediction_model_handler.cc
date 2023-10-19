// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"

#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"
#include "components/autofill/core/common/autofill_features.h"
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
  if (!ModelAvailable() || !vectorizer_) {
    // No model, no predictions.
    std::move(callback).Run(std::move(form_structure));
    return;
  }

  AutofillModelExecutor::ModelInput vectorized_input =
      VectorizeForm(*form_structure);
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
      std::move(vectorized_input));
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

void AutofillMlPredictionModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  CHECK_EQ(optimization_target,
           optimization_guide::proto::OptimizationTarget::
               OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION);
  optimization_guide::ModelHandler<AutofillModelExecutor::ModelOutput,
                                   const AutofillModelExecutor::ModelInput&>::
      OnModelUpdated(optimization_target, model_info);
  if (!model_info.has_value()) {
    // The model was unloaded.
    return;
  }
  // The model was loaded or updated.
  InitializeVectorizer();
}

void AutofillMlPredictionModelHandler::InitializeVectorizer() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce([] {
        return AutofillModelVectorizer::CreateVectorizer(
            base::FilePath::FromASCII(
                features::kAutofillModelDictionaryFilePath.Get()));
      }),
      base::BindOnce(
          [](base::WeakPtr<AutofillMlPredictionModelHandler> handler,
             std::unique_ptr<AutofillModelVectorizer> vectorizer) {
            if (handler) {
              CHECK(vectorizer);
              handler->vectorizer_ = std::move(vectorizer);
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

AutofillModelExecutor::ModelInput
AutofillMlPredictionModelHandler::VectorizeForm(
    const FormStructure& form) const {
  CHECK(vectorizer_);
  AutofillModelExecutor::ModelInput vectorized_form(form.fields().size());
  for (size_t i = 0; i < form.field_count(); ++i) {
    vectorized_form[i] = vectorizer_->Vectorize(form.field(i)->label);
  }
  return vectorized_form;
}

}  // namespace autofill

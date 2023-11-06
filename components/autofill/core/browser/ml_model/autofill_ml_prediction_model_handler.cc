// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"

#include <algorithm>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"
#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"

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
  if (!ModelAvailable() || !state_) {
    // No model, no predictions.
    std::move(callback).Run(std::move(form_structure));
    return;
  }

  AutofillModelExecutor::ModelInput vectorized_input =
      VectorizeForm(*form_structure);
  ExecuteModelWithInput(
      base::BindOnce(
          [](base::WeakPtr<AutofillMlPredictionModelHandler> self,
             std::unique_ptr<FormStructure> form_structure,
             base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback,
             const absl::optional<AutofillModelExecutor::ModelOutput>& output) {
            if (!self) {
              return;
            }
            CHECK(output);
            self->AssignMostLikelyTypes(*form_structure, *output);
            std::move(callback).Run(std::move(form_structure));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(form_structure),
          std::move(callback)),
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
  state_.reset();
  ModelState state;
  if (!model_info->GetModelMetadata() ||
      !state.metadata.ParseFromString(
          model_info->GetModelMetadata()->value())) {
    // The model should always come with metadata - but since this comes from
    // the server-side and might change in the future, it might fail.
    return;
  }
  state.vectorizer = AutofillModelVectorizer(state.metadata.input_token());
  state_.emplace(std::move(state));
}

AutofillModelExecutor::ModelInput
AutofillMlPredictionModelHandler::VectorizeForm(
    const FormStructure& form) const {
  CHECK(state_);
  AutofillModelExecutor::ModelInput vectorized_form(form.fields().size());
  for (size_t i = 0; i < form.field_count(); ++i) {
    vectorized_form[i] = state_->vectorizer.Vectorize(form.field(i)->label);
  }
  return vectorized_form;
}

void AutofillMlPredictionModelHandler::AssignMostLikelyTypes(
    FormStructure& form,
    const AutofillModelExecutor::ModelOutput& output) const {
  // The model only outputs type for the first
  // `AutofillModelExecutor::kMaxNumberOfFields` many fields.
  CHECK_EQ(output.size(), std::min(form.field_count(),
                                   AutofillModelExecutor::kMaxNumberOfFields));
  for (size_t i = 0; i < output.size(); i++) {
    form.field(i)->set_heuristic_type(HeuristicSource::kMachineLearning,
                                      GetMostLikelyType(output[i]));
  }
}

ServerFieldType AutofillMlPredictionModelHandler::GetMostLikelyType(
    const std::vector<float>& model_output) const {
  CHECK(state_);
  int max_index =
      base::ranges::max_element(model_output) - model_output.begin();
  CHECK_LT(max_index, state_->metadata.output_type_size());
  if (!state_->metadata.has_confidence_threshold() ||
      model_output[max_index] >= state_->metadata.confidence_threshold()) {
    return ToSafeServerFieldType(state_->metadata.output_type(max_index),
                                 UNKNOWN_TYPE);
  }
  return UNKNOWN_TYPE;
}

}  // namespace autofill

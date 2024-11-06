// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"

#include <algorithm>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_executor.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"

namespace autofill {

namespace {

// Creates the model metadata and specifies the model input version to
// ensure client-server version compatibility while loading the model.
std::optional<optimization_guide::proto::Any> CreateModelMetadata() {
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "AutofillFieldClassificationModelMetadata");
  optimization_guide::proto::AutofillFieldClassificationModelMetadata
      model_metadata;
  model_metadata.set_input_version(
      FieldClassificationModelHandler::kAutofillModelInputVersion);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  return any_metadata;
}

// Returns true if all fields can be predicted at confidence >=
// `confidence_threshold`.
bool AllFieldsClassifiedWithConfidence(
    const FieldClassificationModelEncoder::ModelOutput& output,
    size_t num_fields,
    float confidence_threshold) {
  for (size_t i = 0; i < num_fields; i++) {
    if (base::ranges::max(output[i]) < confidence_threshold) {
      return false;
    }
  }
  return true;
}

HeuristicSource GetHeuristicSource(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  switch (optimization_target) {
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION:
      return HeuristicSource::kAutofillMachineLearning;
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION:
      return HeuristicSource::kPasswordManagerMachineLearning;
    default:
      NOTREACHED();
  }
}

}  // anonymous namespace

FieldClassificationModelHandler::FieldClassificationModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target)
    : optimization_guide::ModelHandler<
          FieldClassificationModelEncoder::ModelOutput,
          const FieldClassificationModelEncoder::ModelInput&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<FieldClassificationModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          CreateModelMetadata()),
      optimization_target_(optimization_target) {
  // Store the model in memory as soon as it is available and keep it loaded for
  // the whole browser session since we query predictions very regularly.
  // TODO(crbug.com/40276177): Maybe change both back to default behavior if we
  // see memory regressions during the rollout.
  SetShouldPreloadModel(true);
  SetShouldUnloadModelOnComplete(false);
}
FieldClassificationModelHandler::~FieldClassificationModelHandler() = default;

void FieldClassificationModelHandler::GetModelPredictionsForForm(
    std::unique_ptr<FormStructure> form_structure,
    base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback) {
  if (!ModelAvailable() || !state_) {
    // No model, no predictions.
    std::move(callback).Run(std::move(form_structure));
    return;
  }
  FieldClassificationModelEncoder::ModelInput encoded_input =
      state_->encoder.EncodeForm(*form_structure);
  ExecuteModelWithInput(
      base::BindOnce(
          [](base::WeakPtr<FieldClassificationModelHandler> self,
             std::unique_ptr<FormStructure> form_structure,
             base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback,
             const std::optional<FieldClassificationModelEncoder::ModelOutput>&
                 output) {
            if (self && output &&
                self->ShouldEmitPredictions(form_structure.get(), *output)) {
              self->AssignMostLikelyTypes(*form_structure, *output);
            }
            std::move(callback).Run(std::move(form_structure));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(form_structure),
          std::move(callback)),
      std::move(encoded_input));
}

void FieldClassificationModelHandler::GetModelPredictionsForForms(
    std::vector<std::unique_ptr<FormStructure>> forms,
    base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>
        callback) {
  auto barrier_callback = base::BarrierCallback<std::unique_ptr<FormStructure>>(
      forms.size(), std::move(callback));
  for (std::unique_ptr<FormStructure>& form : forms) {
    GetModelPredictionsForForm(std::move(form), barrier_callback);
  }
}

void FieldClassificationModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  CHECK_EQ(optimization_target, optimization_target_);
  optimization_guide::ModelHandler<
      FieldClassificationModelEncoder::ModelOutput,
      const FieldClassificationModelEncoder::ModelInput&>::
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
  state.encoder = FieldClassificationModelEncoder(
      state.metadata.input_token(), state.metadata.encoding_parameters());
  state_.emplace(std::move(state));
}

void FieldClassificationModelHandler::AssignMostLikelyTypes(
    FormStructure& form,
    const FieldClassificationModelEncoder::ModelOutput& output) const {
  // The ML model can process at most
  // `FieldClassificationModelEncoder::kModelMaxNumberOfFields`.
  size_t relevant_fields = std::min(form.field_count(), output.size());
  HeuristicSource heuristic_source = GetHeuristicSource(optimization_target_);
  for (size_t i = 0; i < relevant_fields; i++) {
    form.field(i)->set_heuristic_type(heuristic_source,
                                      GetMostLikelyType(output[i]));
  }
}

FieldType FieldClassificationModelHandler::GetMostLikelyType(
    const std::vector<float>& model_output) const {
  CHECK(state_);
  int max_index =
      base::ranges::max_element(model_output) - model_output.begin();
  CHECK_LT(max_index, state_->metadata.output_type_size());
  if (!state_->metadata.postprocessing_parameters()
           .has_confidence_threshold_per_field() ||
      model_output[max_index] >= state_->metadata.postprocessing_parameters()
                                     .confidence_threshold_per_field()) {
    return ToSafeFieldType(state_->metadata.output_type(max_index),
                           UNKNOWN_TYPE);
  }
  return NO_SERVER_DATA;
}

bool FieldClassificationModelHandler::ShouldEmitPredictions(
    const FormStructure* form,
    const FieldClassificationModelEncoder::ModelOutput& output) {
  return !state_->metadata.postprocessing_parameters()
              .has_confidence_threshold_to_disable_all_predictions() ||
         AllFieldsClassifiedWithConfidence(
             output, std::min(form->field_count(), output.size()),
             state_->metadata.postprocessing_parameters()
                 .confidence_threshold_to_disable_all_predictions());
}

}  // namespace autofill

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_HANDLER_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_executor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"

namespace autofill {

// Model Handler which asynchronously calls the
// `FieldClassificationModelExecutor`. It retrieves the model from the server,
// load it into memory, execute it with FormStructure as input and associate the
// model FieldType predictions with the FormStructure.
class FieldClassificationModelHandler
    : public optimization_guide::ModelHandler<
          FieldClassificationModelEncoder::ModelOutput,
          const FieldClassificationModelEncoder::ModelInput&>,
      public KeyedService {
 public:
  // The version of the input, based on which the relevant model
  // version will be used by the server.
  static constexpr int64_t kAutofillModelInputVersion = 3;

  FieldClassificationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target);
  ~FieldClassificationModelHandler() override;

  // This function asynchronously queries predictions for the `form_structure`
  // from the model and sets the model predictions in the FormStructure's fields
  // as heurstic type values. Once done, the `callback` is triggered on the UI
  // sequence and returns the `form_structure`. If `form_structure` has more
  // than `maximum_number_of_fields` (see model metadata) fields, it sets
  // predictions for the first `maximum_number_of_fields` fields in the form.
  void GetModelPredictionsForForm(
      std::unique_ptr<FormStructure> form_structure,
      base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback);

  // Same as `GetModelPredictionsForForm()` but executes the model on multiple
  // forms.
  // Virtual for testing.
  virtual void GetModelPredictionsForForms(
      std::vector<std::unique_ptr<FormStructure>> forms,
      base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>
          callback);

  // optimization_guide::ModelHandler:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

 private:
  // Computes the `GetMostLikelyType()` from every element of `outputs` and
  // asssigns it to the corresponding field of the `form`.
  void AssignMostLikelyTypes(
      FormStructure& form,
      const FieldClassificationModelEncoder::ModelOutput& output) const;

  // Given the confidences returned by the ML model, returns the most likely
  // type. This is currently just the argmax of `model_output`, mapped to the
  // corresponding FieldType.
  FieldType GetMostLikelyType(const std::vector<float>& model_output) const;

  // Returns true if the `output` allows to return predictions for `form`.
  bool ShouldEmitPredictions(
      const FormStructure* form,
      const FieldClassificationModelEncoder::ModelOutput& output);

  struct ModelState {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        metadata;
    FieldClassificationModelEncoder encoder;
  };
  // Initialized once the model was loaded and successfully initialized using
  // the model's metadata.
  std::optional<ModelState> state_;

  // Specifies the model to load and execute.
  const optimization_guide::proto::OptimizationTarget optimization_target_;

  base::WeakPtrFactory<FieldClassificationModelHandler> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_HANDLER_H_

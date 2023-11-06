// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_ML_PREDICTION_MODEL_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_ML_PREDICTION_MODEL_HANDLER_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"
#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"

namespace autofill {

// Model Handler which asynchronously calls the `AutofillModelExecutor`.
// It retrieves the model from the server, load it into memory, execute
// it with FormStructure as input and associate the model ServerFieldType
// predictions with the FormStructure.
class AutofillMlPredictionModelHandler
    : public optimization_guide::ModelHandler<
          AutofillModelExecutor::ModelOutput,
          const AutofillModelExecutor::ModelInput&>,
      public KeyedService {
 public:
  explicit AutofillMlPredictionModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~AutofillMlPredictionModelHandler() override;

  // This function asynchronously queries predictions for the `form_structure`
  // from the model and sets the model predictions with the FormStructure
  // using `HeuristicSource::kMachineLearning`. Once done, the `callback` is
  // triggered on the UI sequence and returns the `form_structure`.
  // If `form_structure` has more than
  // `AutofillModelExecutor::kMaxNumberOfFields` fields, it sets predictions for
  // the first `AutofillModelExecutor::kMaxNumberOfFields` fields in the form.
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
  // Encodes the `form` into the `ModelInput` representation understood by the
  // `AutofillModelExecutor`. This is done by vectorizing the labels of the
  // form's fields using the `vectorizer_`.
  AutofillModelExecutor::ModelInput VectorizeForm(
      const FormStructure& form) const;

  // Computes the `GetMostLikelyType()` from every element of `outputs` and
  // asssigns it to the corresponding field of the `form`.
  void AssignMostLikelyTypes(
      FormStructure& form,
      const AutofillModelExecutor::ModelOutput& output) const;

  // Given the confidences returned by the ML model, returns the most likely
  // type. This is currently just the argmax of `model_output`, mapped to the
  // corresponding ServerFieldType.
  ServerFieldType GetMostLikelyType(
      const std::vector<float>& model_output) const;

  struct ModelState {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        metadata;
    AutofillModelVectorizer vectorizer;
  };
  // Initialized once the model was loaded and successfully initialized using
  // the model's metadata.
  std::optional<ModelState> state_;

  base::WeakPtrFactory<AutofillMlPredictionModelHandler> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_ML_PREDICTION_MODEL_HANDLER_H_

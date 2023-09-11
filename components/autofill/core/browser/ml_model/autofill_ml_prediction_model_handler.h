// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_ML_PREDICTION_MODEL_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_ML_PREDICTION_MODEL_HANDLER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace autofill {

// Model Handler which asynchronously calls the `AutofillModelExecutor`.
// It retrieves the model from the server, load it into memory, execute
// it with FormStructure as input and associate the model ServerFieldType
// predictions with the FormStructure.
class AutofillMlPredictionModelHandler
    : public optimization_guide::ModelHandler<ServerFieldType,
                                              const FormFieldData&>,
      public KeyedService {
 public:
  explicit AutofillMlPredictionModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~AutofillMlPredictionModelHandler() override;

  // This function asynchronously queries predictions for the `form_structure`
  // from the model and sets the model predictions with the FormStructure
  // using `HeuristicSource::kMachineLearning`. Once done, the `callback` is
  // triggered on the UI sequence and returns the `form_structure`.
  void GetModelPredictionsForForm(
      std::unique_ptr<FormStructure> form_structure,
      base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_ML_PREDICTION_MODEL_HANDLER_H_

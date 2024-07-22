// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_

#include <optional>
#include <vector>

#include "components/autofill/core/browser/ml_model/autofill_model_encoder.h"
#include "components/optimization_guide/core/base_model_executor.h"

namespace autofill {

// Maximum number of form fields for which the model can predict types.
// When calling the executor with a larger form, predictions are only returned
// for the first `kModelExecutorMaxNumberOfFields` many fields.
inline constexpr size_t kModelExecutorMaxNumberOfFields = 30;

// The executor maps its inputs into TFLite's tensor format and converts the
// model output's tensor representation back. See `ModelInput` and `ModelOutput`
// for descriptions of the inputs and outputs.
class AutofillModelExecutor
    : public optimization_guide::BaseModelExecutor<
          std::array<std::vector<float>, kModelExecutorMaxNumberOfFields>,
          const std::vector<
              std::array<AutofillModelEncoder::TokenId,
                         AutofillModelEncoder::kOutputSequenceLength>>&> {
 public:
  // TODO(crbug.com/40276177): Move `ModelInput` and `ModelOutput` to the
  // autofill_model_encoder. An encoded representation of the form's labels.
  // Each element of the vector corresponds to an encoded label. See
  // `AutofillModelEncoder`,
  using ModelInput =
      std::vector<std::array<AutofillModelEncoder::TokenId,
                             AutofillModelEncoder::kOutputSequenceLength>>;

  // The model always returns predictions for `kModelExecutorMaxNumberOfFields`.
  // If the queried form was smaller, the last
  // (kModelExecutorMaxNumberOfFields - fields) elements of the output have
  // unspecified values.
  // The other indices contain a vector with one entry per supported FieldType,
  // representing the confidence in that type. The confidences don't have any
  // meaning per-se, but higher means more confidence. Since the model might not
  // support all FieldTypes, the indices don't map to field types directly. See
  // `AutofillMlPredictionModelHandler`.
  using ModelOutput =
      std::array<std::vector<float>, kModelExecutorMaxNumberOfFields>;

  AutofillModelExecutor();
  ~AutofillModelExecutor() override;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override;
  std::optional<ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_

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

// The executor maps its inputs into TFLite's tensor format and converts the
// model output's tensor representation back. See `ModelInput` and `ModelOutput`
// for descriptions of the inputs and outputs.
class AutofillModelExecutor : public optimization_guide::BaseModelExecutor<
                                  AutofillModelEncoder::ModelOutput,
                                  const AutofillModelEncoder::ModelInput&> {
 public:
  AutofillModelExecutor();
  ~AutofillModelExecutor() override;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const AutofillModelEncoder::ModelInput& input) override;
  std::optional<AutofillModelEncoder::ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_

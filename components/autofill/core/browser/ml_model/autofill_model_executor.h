// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_

#include <optional>
#include <vector>

#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"
#include "components/optimization_guide/core/base_model_executor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// The executor maps its inputs into TFLite's tensor format and converts the
// model output's tensor representation back. See `ModelInput` and `ModelOutput`
// for descriptions of the inputs and outputs.
// The executor supports at most `kMaxNumberOfFields`. When calling the executor
// with a larger form, predictions are only returned for the first
// `kMaxNumberOfFields` many fields.
class AutofillModelExecutor
    : public optimization_guide::BaseModelExecutor<
          std::vector<std::vector<float>>,
          const std::vector<
              std::array<AutofillModelVectorizer::TokenId,
                         AutofillModelVectorizer::kOutputSequenceLength>>&> {
 public:
  // A vectorized representation of the form's labels. Each element of the
  // vector corresponds to a vectorized label. See `AutofillModelVectorizer`,
  using ModelInput =
      std::vector<std::array<AutofillModelVectorizer::TokenId,
                             AutofillModelVectorizer::kOutputSequenceLength>>;

  // One element per `ModelInput::size()`, representing the raw model outputs
  // for the different field types. They don't have any meaning per-se, but
  // higher means more confidence. Since the model might not support all
  // ServerFieldTypes, the indices don't map to field types directly. See
  // `AutofillMlPredictionModelHandler`.
  using ModelOutput = std::vector<std::vector<float>>;

  // Maximum number of fields in one form that can be used as input.
  static constexpr size_t kMaxNumberOfFields = 20;

  AutofillModelExecutor();
  ~AutofillModelExecutor() override;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const ModelInput& input) override;
  absl::optional<ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;

  // Stores the number of fields sent to the model via `Preprocess()`. This will
  // be min(number of fields provided, kMaxNumberOfFields). `Postprocess()`
  // relies on this information to construct the output.
  // Nullopt indicates that no query is currently processing.
  // Since multiple queries are executed sequentially, ensuring that this value
  // only corresponds to a single query at a time.
  std::optional<size_t> fields_count_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_MODEL_EXECUTOR_H_

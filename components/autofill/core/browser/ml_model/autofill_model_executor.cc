// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"

#include <vector>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/internal/tensor_ctypes.h"

namespace autofill {

AutofillModelExecutor::AutofillModelExecutor() = default;
AutofillModelExecutor::~AutofillModelExecutor() = default;

bool AutofillModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const ModelInput& input) {
  CHECK(base::FeatureList::IsEnabled(features::kAutofillModelPredictions));
  CHECK_EQ(2u, input_tensors.size());
  CHECK_EQ(kTfLiteFloat32, input_tensors[0]->type);
  CHECK_EQ(kTfLiteBool, input_tensors[1]->type);
  CHECK(!fields_count_);
  fields_count_ = std::min(input.size(), kMaxNumberOfFields);
  // `input_tensors[0]` is a 3D vector. The first dimension is used for
  // batching, which the ML model declares with size 1 so only one form
  // is consumed at a time. The second and third dimensions hold the
  // values of the vectorized field labels.
  {
    CHECK_EQ(input_tensors[0]->dims->size, 3);
    CHECK_EQ(input_tensors[0]->dims->data[1],
             static_cast<int>(kMaxNumberOfFields));
    CHECK_EQ(input_tensors[0]->dims->data[2],
             static_cast<int>(AutofillModelVectorizer::kOutputSequenceLength));
    // Initialize with vectors having the first element = 1 which is what the
    // model expects for empty fields.
    std::vector<float> empty_field(
        AutofillModelVectorizer::kOutputSequenceLength);
    empty_field[0] = 1;
    std::vector<std::vector<float>> vectorized_input(kMaxNumberOfFields,
                                                     empty_field);

    for (size_t i = 0; i < fields_count_; ++i) {
      base::ranges::transform(input[i], vectorized_input[i].begin(),
                              [](AutofillModelVectorizer::TokenId token_id) {
                                return token_id.value();
                              });
    }
    // Populate tensors with the vectorized field labels.
    for (size_t i = 0; i < kMaxNumberOfFields; ++i) {
      for (size_t j = 0; j < AutofillModelVectorizer::kOutputSequenceLength;
           ++j) {
        tflite::GetTensorData<float>(input_tensors[0])
            [i * AutofillModelVectorizer::kOutputSequenceLength + j] =
                vectorized_input[i][j];
      }
    }
  }
  // `input_tensors[1]` is a 2D vector of boolean values. The first dimension
  // is used for batching, which the ML model declares with size 1 so only one
  // form is consumed at a time. The second dimension contains boolean elements
  // where each value corresponds to whether there is a form field in the form
  // in this index or not.
  {
    CHECK_EQ(input_tensors[1]->dims->size, 2);
    for (size_t i = 0; i < kMaxNumberOfFields; ++i) {
      tflite::GetTensorData<bool>(input_tensors[1])[i] = i < fields_count_;
    }
  }
  return true;
}

absl::optional<AutofillModelExecutor::ModelOutput>
AutofillModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  // `output_tensors` is a 3D vector of floats. The first dimension is used
  // for batching, which the ML model declares with size 1. The second and third
  // dimensions contain the raw predictions for every `ServerFieldType` in
  // `kSupportedFieldTypes` for the first `kMaxNumberOfFields` fields of the
  // form.
  CHECK_EQ(1u, output_tensors.size());
  CHECK_EQ(kTfLiteFloat32, output_tensors[0]->type);
  CHECK_EQ(output_tensors[0]->dims->data[1],
           static_cast<int>(kMaxNumberOfFields));
  const size_t num_outputs = output_tensors[0]->dims->data[2];
  ModelOutput model_predictions(*fields_count_,
                                std::vector<float>(num_outputs));
  for (size_t i = 0; i < fields_count_; ++i) {
    for (size_t j = 0; j < num_outputs; ++j) {
      model_predictions[i][j] =
          tflite::GetTensorData<float>(output_tensors[0])[i * num_outputs + j];
    }
  }
  fields_count_.reset();
  return model_predictions;
}

}  // namespace autofill

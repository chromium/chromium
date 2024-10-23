// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO: crbug.com/347137620 - Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "components/autofill/core/browser/ml_model/field_classification_model_executor.h"

#include <vector>

#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/internal/tensor_ctypes.h"

namespace autofill {

FieldClassificationModelExecutor::FieldClassificationModelExecutor() = default;
FieldClassificationModelExecutor::~FieldClassificationModelExecutor() = default;

bool FieldClassificationModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const FieldClassificationModelEncoder::ModelInput& input) {
  // `input_tensors[0]` has shape (batch_size, max_number_of_fields,
  // tokens_per_field) where the batch size is set to 1. The second and third
  // dimensions hold the values of the vectorized field labels.
  CHECK_EQ(input_tensors[0]->dims->size, 3);
  const size_t maximum_number_of_fields = input_tensors[0]->dims->data[1];
  const size_t output_sequence_length = input_tensors[0]->dims->data[2];

  CHECK_EQ(2u, input_tensors.size());
  CHECK_EQ(kTfLiteFloat32, input_tensors[0]->type);
  CHECK_EQ(kTfLiteBool, input_tensors[1]->type);
  size_t fields_count = std::min(input.size(), maximum_number_of_fields);
  {
    // Initialize with vectors with zeros which is what the model expects for
    // empty fields.
    std::vector<float> empty_field(output_sequence_length);
    std::vector<std::vector<float>> encoded_input(maximum_number_of_fields,
                                                  empty_field);

    for (size_t i = 0; i < fields_count; ++i) {
      base::ranges::transform(
          input[i], encoded_input[i].begin(),
          [](FieldClassificationModelEncoder::TokenId token_id) {
            return token_id.value();
          });
    }
    // Populate tensors with the vectorized field labels.
    for (size_t i = 0; i < maximum_number_of_fields; ++i) {
      base::ranges::copy(encoded_input[i],
                         tflite::GetTensorData<float>(input_tensors[0]) +
                             i * output_sequence_length);
    }
  }
  // `input_tensors[1]` is a boolean mask of shape
  // (batch_size, number_of_fields) indicating which fields are valid or the
  // result of padding to a length of `max_number_of_fields` (those fields
  // should be ignored).
  // batch_size is 1, and `GetTensorData(input_tensors[1])[i]` should be 1 iff
  // field `i` in the input is *not* padding.
  {
    CHECK_EQ(input_tensors[1]->dims->size, 2);
    for (size_t i = 0; i < maximum_number_of_fields; ++i) {
      tflite::GetTensorData<bool>(input_tensors[1])[i] = i < fields_count;
    }
  }
  return true;
}

std::optional<FieldClassificationModelEncoder::ModelOutput>
FieldClassificationModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  // `output_tensors` is a 3D vector of floats. The first dimension is used
  // for batching, which the ML model declares with size 1. The second and third
  // dimensions contain the raw predictions for every supported `FieldType` of
  // the model, for all `kModelExecutorMaxNumberOfFields`.
  CHECK_EQ(1u, output_tensors.size());
  CHECK_EQ(kTfLiteFloat32, output_tensors[0]->type);
  const size_t maximum_number_of_fields = output_tensors[0]->dims->data[1];
  const size_t num_outputs = output_tensors[0]->dims->data[2];
  FieldClassificationModelEncoder::ModelOutput model_predictions(
      maximum_number_of_fields);
  for (size_t i = 0; i < maximum_number_of_fields; ++i) {
    model_predictions[i].resize(num_outputs);
    const float* data_bgn =
        tflite::GetTensorData<float>(output_tensors[0]) + i * num_outputs;
    base::ranges::copy(data_bgn, data_bgn + num_outputs,
                       model_predictions[i].begin());
  }
  return model_predictions;
}

}  // namespace autofill

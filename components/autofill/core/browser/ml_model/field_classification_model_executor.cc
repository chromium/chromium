// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_executor.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"
#include "third_party/tflite/src/tensorflow/lite/core/c/common.h"
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
  auto input_dims = UNSAFE_BUFFERS(
      base::span<const int>(input_tensors[0]->dims->data,
                            static_cast<size_t>(input_tensors[0]->dims->size)));
  const size_t maximum_number_of_fields = input_dims[1];
  const size_t output_sequence_length = input_dims[2];

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
      // Mitigation for crbug.com/495252686.
      CHECK_LE(input[i].size(), output_sequence_length);

      std::ranges::transform(
          input[i], encoded_input[i].begin(),
          [](FieldClassificationModelEncoder::TokenId token_id) {
            return token_id.value();
          });
    }
    // Populate tensors with the vectorized field labels.
    CHECK_GE(input_tensors[0]->bytes,
             maximum_number_of_fields * output_sequence_length * sizeof(float));
    auto input_data = UNSAFE_BUFFERS(
        base::span<float>(tflite::GetTensorData<float>(input_tensors[0]),
                          maximum_number_of_fields * output_sequence_length));
    for (size_t i = 0; i < maximum_number_of_fields; ++i) {
      std::ranges::copy(
          encoded_input[i],
          input_data.subspan(i * output_sequence_length, output_sequence_length)
              .begin());
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

    // Mitigation for crbug.com/495252686.
    CHECK_LE(maximum_number_of_fields * sizeof(bool), input_tensors[1]->bytes);

    auto mask_data = UNSAFE_BUFFERS(
        base::span<bool>(tflite::GetTensorData<bool>(input_tensors[1]),
                         maximum_number_of_fields));
    for (size_t i = 0; i < maximum_number_of_fields; ++i) {
      mask_data[i] = i < fields_count;
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
  CHECK_EQ(output_tensors[0]->dims->size, 3);
  auto output_dims = UNSAFE_BUFFERS(base::span<const int>(
      output_tensors[0]->dims->data,
      static_cast<size_t>(output_tensors[0]->dims->size)));
  const size_t maximum_number_of_fields = output_dims[1];
  const size_t num_outputs = output_dims[2];

  CHECK_GE(output_tensors[0]->bytes,
           maximum_number_of_fields * num_outputs * sizeof(float));
  auto output_data = UNSAFE_BUFFERS(
      base::span<const float>(tflite::GetTensorData<float>(output_tensors[0]),
                              maximum_number_of_fields * num_outputs));

  FieldClassificationModelEncoder::ModelOutput model_predictions(
      maximum_number_of_fields);
  for (size_t i = 0; i < maximum_number_of_fields; ++i) {
    model_predictions[i].resize(num_outputs);
    auto src = output_data.subspan(i * num_outputs, num_outputs);
    std::ranges::copy(src, model_predictions[i].begin());
  }
  return model_predictions;
}

}  // namespace autofill

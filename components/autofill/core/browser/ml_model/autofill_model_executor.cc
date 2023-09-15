// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"

#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ml_model/autofill_model_vectorizer.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/internal/tensor_ctypes.h"

namespace autofill {

AutofillModelExecutor::AutofillModelExecutor() = default;
AutofillModelExecutor::~AutofillModelExecutor() = default;

bool AutofillModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const FormData& input) {
  CHECK(base::FeatureList::IsEnabled(features::kAutofillModelPredictions));
  if (!vectorizer_) {
    vectorizer_ =
        AutofillModelVectorizer::CreateVectorizer(base::FilePath::FromASCII(
            features::kAutofillModelDictionaryFilePath.Get()));
    CHECK(vectorizer_);
  }
  CHECK_EQ(2u, input_tensors.size());
  CHECK_EQ(kTfLiteFloat32, input_tensors[0]->type);
  CHECK_EQ(kTfLiteBool, input_tensors[1]->type);
  CHECK_EQ(fields_count_, 0u);
  fields_count_ = std::min(input.fields.size(), kMaxNumberOfFields);
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

    for (size_t i = 0; i < fields_count_; i++) {
      auto token_ids = vectorizer_->Vectorize(input.fields[i].label);
      base::ranges::transform(token_ids, vectorized_input[i].begin(),
                              [](AutofillModelVectorizer::TokenId token_id) {
                                return token_id.value();
                              });
    }
    // Populate tensors with the vectorized field labels.
    for (size_t i = 0; i < kMaxNumberOfFields; i++) {
      for (size_t j = 0; j < AutofillModelVectorizer::kOutputSequenceLength;
           j++) {
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
    for (size_t i = 0; i < kMaxNumberOfFields; i++) {
      tflite::GetTensorData<bool>(input_tensors[1])[i] = i < fields_count_;
    }
  }
  return true;
}

absl::optional<std::vector<ServerFieldType>> AutofillModelExecutor::Postprocess(
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
  CHECK_EQ(output_tensors[0]->dims->data[2],
           static_cast<int>(kSupportedFieldTypes.size()));

  std::vector<ServerFieldType> model_predictions(fields_count_);
  for (size_t i = 0; i < fields_count_; i++) {
    std::vector<float> output(kSupportedFieldTypes.size());
    for (size_t j = 0; j < kSupportedFieldTypes.size(); j++) {
      output[j] = tflite::GetTensorData<float>(
          output_tensors[0])[i * kSupportedFieldTypes.size() + j];
    }
    // Get index of greatest value in the array. This will be the index of the
    // server field type prediction in `kSupportedFieldTypes`
    size_t max_index =
        std::distance(output.begin(), base::ranges::max_element(output));
    model_predictions[i] = kSupportedFieldTypes[max_index];
  }
  fields_count_ = 0;
  return model_predictions;
}

}  // namespace autofill

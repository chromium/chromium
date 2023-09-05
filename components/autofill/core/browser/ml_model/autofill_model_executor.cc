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
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace autofill {

AutofillModelExecutor::AutofillModelExecutor() = default;
AutofillModelExecutor::~AutofillModelExecutor() = default;

bool AutofillModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const FormFieldData& input) {
  CHECK(base::FeatureList::IsEnabled(features::kAutofillModelPredictions));
  if (!vectorizer_) {
    vectorizer_ =
        AutofillModelVectorizer::CreateVectorizer(base::FilePath::FromASCII(
            features::kAutofillModelDictionaryFilePath.Get()));
    CHECK(vectorizer_);
  }
  CHECK_EQ(2u, input_tensors.size());
  CHECK_EQ(kTfLiteFloat32, input_tensors[0]->type);
  CHECK_EQ(kTfLiteFloat32, input_tensors[1]->type);
  // `input_tensors[0]` contains the vector of the vectorized field label
  {
    auto token_ids = vectorizer_->Vectorize(input.label);
    std::vector<float> vectorized_input(token_ids.size());
    base::ranges::transform(token_ids, vectorized_input.begin(),
                            [](AutofillModelVectorizer::TokenId token_id) {
                              return token_id.value();
                            });
    CHECK(tflite::task::core::PopulateTensor<float>(vectorized_input,
                                                    input_tensors[0])
              .ok());
  }
  // `input_tensors[1]` contains the normalized field ordinal value.
  {
    // The field ordinal currently used is equal to 1 for simplification.
    // TODO(crbug.com/1465926): Change field ordinal to the actual index
    // of the field.
    std::vector<float> normalized_value = {
        static_cast<float>((1 - 9.297777) / sqrt(104.379074))};
    CHECK(tflite::task::core::PopulateTensor<float>(normalized_value,
                                                    input_tensors[1])
              .ok());
  }
  return true;
}

absl::optional<ServerFieldType> AutofillModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  CHECK_EQ(kTfLiteFloat32, output_tensors[0]->type);
  CHECK_EQ(1u, output_tensors.size());
  std::vector<float> output;
  CHECK(tflite::task::core::PopulateVector(output_tensors[0], &output).ok());
  CHECK_EQ(kSupportedFieldTypes.size(), output.size());
  // Get index of greatest value in the array. This will be the index of the
  // server field type prediction in `kSupportedFieldTypes`
  size_t max_index =
      std::distance(output.begin(), base::ranges::max_element(output));
  return kSupportedFieldTypes[max_index];
}

}  // namespace autofill

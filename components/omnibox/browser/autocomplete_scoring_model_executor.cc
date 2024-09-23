// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scoring_model_executor.h"

#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

AutocompleteScoringModelExecutor::AutocompleteScoringModelExecutor() = default;
AutocompleteScoringModelExecutor::~AutocompleteScoringModelExecutor() = default;

bool AutocompleteScoringModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    ModelInput input) {
  const bool valid_input_size = input.size() == input_tensors.size();
  base::UmaHistogramBoolean(
      "Omnibox.URLScoringModelExecuted.Preprocess.ValidInputSize",
      valid_input_size);
  if (!valid_input_size) {
    return false;
  }
  DCHECK_EQ(kTfLiteFloat32, input_tensors[0]->type);
  for (size_t i = 0; i < input.size(); ++i) {
    std::vector<float> data = {input[i]};
    absl::Status status =
        tflite::task::core::PopulateTensor<float>(data, input_tensors[i]);
    if (!status.ok()) {
      return false;
    }
  }
  return true;
}

std::optional<AutocompleteScoringModelExecutor::ModelOutput>
AutocompleteScoringModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  DCHECK_EQ(1u, output_tensors.size());
  DCHECK_EQ(kTfLiteFloat32, output_tensors[0]->type);
  DCHECK_EQ(1u, output_tensors[0]->bytes / sizeof(output_tensors[0]->type));

  ModelOutput output;
  absl::Status status =
      tflite::task::core::PopulateVector<float>(output_tensors[0], &output);
  if (!status.ok()) {
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
  DCHECK_EQ(1u, output.size());
  return output;
}

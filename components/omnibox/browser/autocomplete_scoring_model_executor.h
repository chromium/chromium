// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_EXECUTOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_EXECUTOR_H_

#include <optional>
#include <vector>

#include "components/optimization_guide/core/base_model_executor.h"
#include "third_party/tflite/src/tensorflow/lite/c/common.h"

// Implements BaseModelExecutor to execute models with float vector input and
// output. Input represents scoring signals associated one autocomplete match
// candidate. Output is between 0 and 1, which represents the probability for
// the match candidate to be clicked. Preprocesses input float vectors for model
// executor. Postprocesses model executor output as float vectors.
class AutocompleteScoringModelExecutor
    : public optimization_guide::BaseModelExecutor<std::vector<float>,
                                                   const std::vector<float>&> {
 public:
  using ModelInput = const std::vector<float>&;
  using ModelOutput = std::vector<float>;

  AutocompleteScoringModelExecutor();
  ~AutocompleteScoringModelExecutor() override;

  // Disallow copy/assign.
  AutocompleteScoringModelExecutor(const AutocompleteScoringModelExecutor&) =
      delete;
  AutocompleteScoringModelExecutor& operator=(
      const AutocompleteScoringModelExecutor&) = delete;

 protected:
  // optimization_guide::BaseModelExecutor:
  //
  // The autocomplete scoring model has multiple inputs, one for each input
  // signal, and each should be added to a separate input tensor.
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  ModelInput input) override;
  std::optional<ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCORING_MODEL_EXECUTOR_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_EDU_CLASSIFIER_MODEL_EXECUTOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_EDU_CLASSIFIER_MODEL_EXECUTOR_H_

#include <optional>
#include <vector>

#include "components/optimization_guide/core/inference/base_model_executor.h"

namespace page_content_annotations {

// Implements BaseModelExecutor to execute models with float vector input and
// single float output. Input represents an embedding. Output is between 0 and
// 1, which represents the probability for the match candidate to be clicked.
// Preprocesses input float vectors for model executor. Postprocesses model
// executor output as float vectors.
class EduClassifierModelExecutor
    : public optimization_guide::BaseModelExecutor<float,
                                                   const std::vector<float>&> {
 public:
  using ModelInput = const std::vector<float>&;
  using ModelOutput = float;

  EduClassifierModelExecutor();
  ~EduClassifierModelExecutor() override;

  // Disallow copy/assign.
  EduClassifierModelExecutor(const EduClassifierModelExecutor&) = delete;
  EduClassifierModelExecutor& operator=(const EduClassifierModelExecutor&) =
      delete;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  ModelInput input) override;
  std::optional<ModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_EDU_CLASSIFIER_MODEL_EXECUTOR_H_

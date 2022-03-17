// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_OPTIMIZATION_GUIDE_SEGMENTATION_MODEL_EXECUTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_OPTIMIZATION_GUIDE_SEGMENTATION_MODEL_EXECUTOR_H_

#include <memory>
#include <vector>

#include "components/optimization_guide/core/base_model_executor.h"
#include "components/optimization_guide/proto/models.pb.h"

struct TfLiteTensor;

namespace segmentation_platform {

// Provides a framework for executing a particular segmentation model.
// It requires the loaded TensorFlow Lite model to use a single one dimensional
// tensor of floats as the input and a single output tensor with a single float.
// The length of the vector of floats given as input to execution needs to
// exactly match the length of the one-dimensional input tensor.
// Since the shape of the inputs and outputs across all segmentation models are
// the same, this class can be re-used across all the segmentation model
// executors.
class SegmentationModelExecutor
    : public optimization_guide::BaseModelExecutor<float,
                                                   const std::vector<float>&> {
 public:
  SegmentationModelExecutor();
  ~SegmentationModelExecutor() override;

  // Disallow copy/assign.
  SegmentationModelExecutor(const SegmentationModelExecutor&) = delete;
  SegmentationModelExecutor& operator=(const SegmentationModelExecutor&) =
      delete;

 protected:
  // optimization_guide::BaseModelExecutor overrides.
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const std::vector<float>& input) override;
  absl::optional<float> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_OPTIMIZATION_GUIDE_SEGMENTATION_MODEL_EXECUTOR_H_

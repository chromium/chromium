// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_CUSTOM_INPUT_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_CUSTOM_INPUT_PROCESSOR_H_

#include <vector>

#include "base/callback.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {
class FeatureProcessorState;

// CustomInputProcessor adds support to a larger variety of data type
// (timestamps, strings mapped to enums, etc), transforming them into valid
// input tensor to use when executing the ML model.
class CustomInputProcessor {
 public:
  explicit CustomInputProcessor();
  virtual ~CustomInputProcessor();

  // Disallow copy/assign.
  CustomInputProcessor(const CustomInputProcessor&) = delete;
  CustomInputProcessor& operator=(const CustomInputProcessor&) = delete;

  using FeatureListQueryProcessorCallback =
      base::OnceCallback<void(std::unique_ptr<FeatureProcessorState>)>;

  // Function for processing the next CustomInput type of input for ML model.
  // When the processing is successful, the feature processor state's input
  // tensor is updated accordingly, else if an error occurred, the feature
  // processor state's error flag is set.
  void ProcessCustomInput(
      const proto::CustomInput& custom_input,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      FeatureListQueryProcessorCallback callback);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_CUSTOM_INPUT_PROCESSOR_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_CUSTOM_INPUT_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_CUSTOM_INPUT_PROCESSOR_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/execution/query_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {
class FeatureProcessorState;

// CustomInputProcessor adds support to a larger variety of data type
// (timestamps, strings mapped to enums, etc), transforming them into valid
// input tensor to use when executing the ML model.
class CustomInputProcessor : public QueryProcessor {
 public:
  explicit CustomInputProcessor();
  explicit CustomInputProcessor(
      base::flat_map<FeatureIndex, proto::CustomInput>&& custom_inputs,
      base::Time prediction_time);
  ~CustomInputProcessor() override;

  using FeatureListQueryProcessorCallback =
      base::OnceCallback<void(std::unique_ptr<FeatureProcessorState>)>;

  // Function for processing a single CustomInput type of input for ML model.
  // When the processing is successful, the feature processor state's input
  // tensor is updated accordingly, else if an error occurred, the feature
  // processor state's error flag is set.
  void ProcessCustomInput(
      const proto::CustomInput& custom_input,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      FeatureListQueryProcessorCallback callback);

  // Called when the processing has finished to insert the result in the state
  // object and notify the feature list processor.
  void OnFinishProcessing(
      FeatureListQueryProcessorCallback callback,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      IndexedTensors result);

  // QueryProcessor implementation.
  void Process(std::unique_ptr<FeatureProcessorState> feature_processor_state,
               QueryProcessorCallback callback) override;

 private:
  // Helper function for parsing a single custom input and insert the result
  // along with the corresponding feature index.
  void ProcessSingleCustomInput(FeatureIndex index,
                                const proto::CustomInput& custom_input);

  // List of custom inputs to process into input tensors.
  base::flat_map<FeatureIndex, proto::CustomInput> custom_inputs_;

  // List of resulting input tensors.
  IndexedTensors result_;

  // Time at which we expect the model execution to run.
  base::Time prediction_time_;

  base::WeakPtrFactory<CustomInputProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_CUSTOM_INPUT_PROCESSOR_H_

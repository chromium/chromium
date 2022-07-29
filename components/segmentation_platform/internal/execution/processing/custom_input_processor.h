// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_CUSTOM_INPUT_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_CUSTOM_INPUT_PROCESSOR_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::processing {
class FeatureProcessorState;
class InputDelegateHolder;

// CustomInputProcessor adds support to a larger variety of data type
// (timestamps, strings mapped to enums, etc), transforming them into valid
// input tensor to use when executing the ML model.
class CustomInputProcessor : public QueryProcessor {
 public:
  CustomInputProcessor(const base::Time prediction_time,
                       InputDelegateHolder* input_delegate_holder);
  CustomInputProcessor(
      base::flat_map<FeatureIndex, proto::CustomInput>&& custom_inputs,
      const base::Time prediction_time,
      InputDelegateHolder* input_delegate_holder);
  ~CustomInputProcessor() override;

  using FeatureListQueryProcessorCallback =
      base::OnceCallback<void(std::unique_ptr<FeatureProcessorState>)>;

  // Function for processing a single CustomInput type of input for ML model.
  // When the processing is successful, the feature processor state's input
  // tensor is updated accordingly, else if an error occurred, the feature
  // processor state's error flag is set.
  // TODO(haileywang): Clean up this class and delete this method.
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

  template <typename IndexType>
  using TemplateCallback =
      base::OnceCallback<void(std::unique_ptr<FeatureProcessorState>,
                              base::flat_map<IndexType, Tensor>)>;

  // Process a data mapping with a customized index type and return the tensor
  // values in |callback|. Appends the input to the provided `result` and
  // returns it.
  template <typename IndexType>
  void ProcessIndexType(
      base::flat_map<IndexType, proto::CustomInput> custom_inputs,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      std::unique_ptr<base::flat_map<IndexType, Tensor>> result,
      TemplateCallback<IndexType> callback);

 private:
  // Helper method to handle async custom inputs for `ProcessIndexType()`
  template <typename IndexType>
  void OnGotProcessedValue(
      base::flat_map<IndexType, proto::CustomInput> custom_inputs,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      std::unique_ptr<base::flat_map<IndexType, Tensor>> result,
      TemplateCallback<IndexType> callback,
      IndexType current_index,
      size_t current_tensor_length,
      bool error,
      Tensor current_value);

  // Helper function for parsing a single sync custom input and insert the
  // result along with the corresponding feature index.
  QueryProcessor::Tensor ProcessSingleCustomInput(
      const proto::CustomInput& custom_input,
      FeatureProcessorState* feature_processor_state);

  // Add a tensor value for CustomInput::FILL_PREDICTION_TIME type and return
  // whether it succeeded.
  bool AddPredictionTime(const proto::CustomInput& custom_input,
                         std::vector<ProcessedValue>& out_tensor);

  // Add a tensor value for CustomInput::TIME_RANGE_BEFORE_PREDICTION type and
  // return whether it succeeded.
  bool AddTimeRangeBeforePrediction(const proto::CustomInput& custom_input,
                                    std::vector<ProcessedValue>& out_tensor);

  const raw_ptr<InputDelegateHolder> input_delegate_holder_;

  // List of custom inputs to process into input tensors.
  base::flat_map<FeatureIndex, proto::CustomInput> custom_inputs_;

  // Time at which we expect the model execution to run.
  base::Time prediction_time_;

  base::WeakPtrFactory<CustomInputProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_CUSTOM_INPUT_PROCESSOR_H_

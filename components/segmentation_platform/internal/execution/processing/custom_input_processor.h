// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_CUSTOM_INPUT_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_CUSTOM_INPUT_PROCESSOR_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::processing {
class FeatureProcessorState;
class InputDelegateHolder;
struct Data;

// CustomInputProcessor adds support to a larger variety of data type
// (timestamps, strings mapped to enums, etc), transforming them into valid
// input tensor to use when executing the ML model.
class CustomInputProcessor : public QueryProcessor {
 public:
  CustomInputProcessor(const base::Time prediction_time,
                       InputDelegateHolder* input_delegate_holder);
  CustomInputProcessor(base::flat_map<FeatureIndex, Data>&& data,
                       const base::Time prediction_time,
                       InputDelegateHolder* input_delegate_holder);
  ~CustomInputProcessor() override;

  // QueryProcessor implementation.
  void Process(FeatureProcessorState& feature_processor_state,
               QueryProcessorCallback callback) override;

  template <typename IndexType>
  using TemplateCallback =
      base::OnceCallback<void(base::flat_map<IndexType, Tensor>)>;

  // Process a data mapping with a customized index type and return the tensor
  // values in |callback|. Appends the input to the provided `result` and
  // returns it.
  template <typename IndexType>
  void ProcessIndexType(
      base::flat_map<IndexType, proto::CustomInput> custom_inputs,
      FeatureProcessorState& feature_processor_state,
      std::unique_ptr<base::flat_map<IndexType, Tensor>> result,
      TemplateCallback<IndexType> callback);

 private:
  // Helper method to handle async custom inputs for `ProcessIndexType()`
  template <typename IndexType>
  void OnGotProcessedValue(
      base::flat_map<IndexType, proto::CustomInput> custom_inputs,
      base::WeakPtr<FeatureProcessorState> feature_processor_state,
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
      FeatureProcessorState& feature_processor_state);

  // Add a tensor value for CustomInput::FILL_PREDICTION_TIME type and return
  // whether it succeeded.
  bool AddPredictionTime(const proto::CustomInput& custom_input,
                         std::vector<ProcessedValue>& out_tensor);

  // Add a tensor value for CustomInput::FILL_DEVICE_RAM type and return
  // whether it succeeded.
  bool AddDeviceRAMInMB(const proto::CustomInput& custom_input,
                        std::vector<ProcessedValue>& out_tensor);

  // Add a tensor value for CustomInput::FILL_DEVICE_OS type and return
  // whether it succeeded.
  bool AddDeviceOSVersionNumber(const proto::CustomInput& custom_input,
                                std::vector<ProcessedValue>& out_tensor);

  // Add a tensor value for CustomInput::FILL_DEVICE_PPI type and return
  // whether it succeeded.
  bool AddDevicePPI(const proto::CustomInput& custom_input,
                    std::vector<ProcessedValue>& out_tensor);

  // Add a tensor value for CustomInput::TIME_RANGE_BEFORE_PREDICTION type and
  // return whether it succeeded.
  bool AddTimeRangeBeforePrediction(const proto::CustomInput& custom_input,
                                    std::vector<ProcessedValue>& out_tensor);

  // Add a tensor value for CustomInput::FILL_FROM_INPUT_CONTEXT and return
  // whether it succeeded.
  bool AddFromInputContext(const proto::CustomInput& custom_input,
                           FeatureProcessorState& feature_processor_state,
                           std::vector<ProcessedValue>& out_tensor);

  // Add a random number for CustomInput::FILL_RANDOM and return whether it
  // succeeded.
  bool AddRandom(const proto::CustomInput& custom_input,
                 std::vector<ProcessedValue>& out_tensor);

  const raw_ptr<InputDelegateHolder, AcrossTasksDanglingUntriaged>
      input_delegate_holder_;

  // List of custom inputs to process into input tensors.
  base::flat_map<FeatureIndex, proto::CustomInput> custom_inputs_;

  // Time at which we expect the model execution to run.
  base::Time prediction_time_;

  base::WeakPtrFactory<CustomInputProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_CUSTOM_INPUT_PROCESSOR_H_

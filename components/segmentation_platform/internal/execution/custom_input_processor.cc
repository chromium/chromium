// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/custom_input_processor.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"

namespace segmentation_platform {

namespace {
// Index not actually used for legacy code in FeatureQueryProcessor.
const int kIndexNotUsed = 0;
}  // namespace

CustomInputProcessor::CustomInputProcessor() = default;

CustomInputProcessor::CustomInputProcessor(
    base::flat_map<FeatureIndex, proto::CustomInput>&& custom_inputs,
    base::Time prediction_time)
    : custom_inputs_(std::move(custom_inputs)),
      prediction_time_(prediction_time) {}

CustomInputProcessor::~CustomInputProcessor() = default;

void CustomInputProcessor::ProcessCustomInput(
    const proto::CustomInput& custom_input,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    FeatureListQueryProcessorCallback callback) {
  DCHECK(custom_inputs_.empty());
  prediction_time_ = feature_processor_state->prediction_time();
  custom_inputs_[kIndexNotUsed] = custom_input;
  Process(std::move(feature_processor_state),
          base::BindOnce(&CustomInputProcessor::OnFinishProcessing,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CustomInputProcessor::OnFinishProcessing(
    FeatureListQueryProcessorCallback callback,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    IndexedTensors result) {
  custom_inputs_.clear();
  feature_processor_state->AppendInputTensor(result[kIndexNotUsed]);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(feature_processor_state)));
}

void CustomInputProcessor::Process(
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessorCallback callback) {
  bool success = true;
  for (const auto& current : custom_inputs_) {
    // Get the next feature in the list to process.
    const proto::CustomInput& custom_input = current.second;

    // Skip custom input with tensor length of 0.
    if (custom_input.tensor_length() == 0) {
      continue;
    }
    // Validate the proto::CustomInput metadata.
    if (metadata_utils::ValidateMetadataCustomInput(custom_input) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      success = false;
    } else {
      ProcessSingleCustomInput(current.first, custom_input);
    }
  }

  // Processing of the feature list has completed.
  custom_inputs_.clear();
  if (!success) {
    custom_inputs_.clear();
    feature_processor_state->SetError();
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(feature_processor_state),
                     std::move(result_)));
}

void CustomInputProcessor::ProcessSingleCustomInput(
    FeatureIndex index,
    const proto::CustomInput& custom_input) {
  std::vector<ProcessedValue> tensor_result;
  if (custom_input.fill_policy() == proto::CustomInput::UNKNOWN_FILL_POLICY) {
    // When parsing a CustomInput object, if the fill policy is not
    // supported by the current version of the client, the fill policy field
    // will not be filled. When this happens, the custom input processor
    // will either use the default values to generate an input tensor or
    // fail the model execution.
    tensor_result =
        std::vector<ProcessedValue>(custom_input.default_value().begin(),
                                    custom_input.default_value().end());
  } else if (custom_input.fill_policy() ==
             proto::CustomInput::FILL_PREDICTION_TIME) {
    tensor_result.emplace_back(ProcessedValue(prediction_time_));
  }
  result_[index] = std::move(tensor_result);
}

}  // namespace segmentation_platform

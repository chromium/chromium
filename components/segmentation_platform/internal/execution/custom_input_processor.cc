// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/custom_input_processor.h"

#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"

namespace segmentation_platform {

CustomInputProcessor::CustomInputProcessor() = default;

CustomInputProcessor::~CustomInputProcessor() = default;

void CustomInputProcessor::ProcessCustomInput(
    const proto::CustomInput& custom_input,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    FeatureListQueryProcessorCallback callback) {
  // Skip custom input with tensor length of 0.
  if (custom_input.tensor_length() == 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::move(feature_processor_state)));
    return;
  }

  // Validate the proto::CustomInput metadata.
  if (metadata_utils::ValidateMetadataCustomInput(custom_input) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    feature_processor_state->SetError();
    feature_processor_state->RunCallback();
    return;
  }

  // TODO(haileywang): Fail validation when the output of custom function is
  // not float.
  if (custom_input.fill_policy() == proto::CustomInput::UNKNOWN_FILL_POLICY) {
    // When prasing a CustomInput object, if the fill policy is not supported by
    // the current version of the client, the fill policy field will not be
    // filled. When this happens, the custom input processor will either use
    // the default values to generate an input tensor or fail the model
    // execution.
    feature_processor_state->AppendInputTensor(
        std::vector<float>(custom_input.default_value().begin(),
                           custom_input.default_value().end()));
  } else if (custom_input.fill_policy() ==
             proto::CustomInput::FILL_PREDICTION_TIME) {
    feature_processor_state->AppendInputTensor(
        std::vector<float>(1, feature_processor_state->prediction_time()
                                  .ToDeltaSinceWindowsEpoch()
                                  .InMicroseconds()));
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(feature_processor_state)));
}

}  // namespace segmentation_platform

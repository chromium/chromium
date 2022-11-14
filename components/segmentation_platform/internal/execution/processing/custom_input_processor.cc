// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/custom_input_processor.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::processing {

namespace {

absl::optional<int> GetArgAsInt(
    const google::protobuf::Map<std::string, std::string>& args,
    const std::string& key) {
  int value;
  auto iter = args.find(key);

  // Did not find target key.
  if (iter == args.end())
    return absl::optional<int>();

  // Perform string to int conversion, return empty value if the conversion
  // failed.
  if (!base::StringToInt(base::StringPiece(iter->second), &value))
    return absl::optional<int>();

  return absl::optional<int>(value);
}

}  // namespace

CustomInputProcessor::CustomInputProcessor(
    const base::Time prediction_time,
    InputDelegateHolder* input_delegate_holder)
    : input_delegate_holder_(input_delegate_holder),
      prediction_time_(prediction_time) {}

CustomInputProcessor::CustomInputProcessor(
    base::flat_map<FeatureIndex, Data>&& data,
    const base::Time prediction_time,
    InputDelegateHolder* input_delegate_holder)
    : input_delegate_holder_(input_delegate_holder),
      prediction_time_(prediction_time) {
  for (const auto& item : data) {
    custom_inputs_[item.first] =
        std::move(item.second.input_feature->custom_input());
  }
}

CustomInputProcessor::~CustomInputProcessor() = default;

void CustomInputProcessor::Process(
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessorCallback callback) {
  auto result = std::make_unique<base::flat_map<FeatureIndex, Tensor>>();
  ProcessIndexType<FeatureIndex>(std::move(custom_inputs_),
                                 std::move(feature_processor_state),
                                 std::move(result), std::move(callback));
}

template <typename IndexType>
void CustomInputProcessor::ProcessIndexType(
    base::flat_map<IndexType, proto::CustomInput> custom_inputs,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    std::unique_ptr<base::flat_map<IndexType, Tensor>> result,
    TemplateCallback<IndexType> callback) {
  bool success = true;
  auto it = custom_inputs.begin();
  for (; it != custom_inputs.end(); it = custom_inputs.begin()) {
    // Get the next feature in the list to process.
    const proto::CustomInput custom_input(std::move(it->second));
    const IndexType index = it->first;
    custom_inputs.erase(it);

    InputDelegate* input_delegate = nullptr;
    if (input_delegate_holder_) {
      input_delegate =
          input_delegate_holder_->GetDelegate(custom_input.fill_policy());
    }
    if (input_delegate) {
      // If a delegate is available then use it to process the input. All the
      // state in this method is moved, so it is ok even if the client ran the
      // callback without posting it.
      const FeatureProcessorState& state = *feature_processor_state;
      input_delegate->Process(
          custom_input, state,
          base::BindOnce(
              &CustomInputProcessor::OnGotProcessedValue<IndexType>,
              weak_ptr_factory_.GetWeakPtr(), std::move(custom_inputs),
              std::move(feature_processor_state), std::move(result),
              std::move(callback), index, custom_input.tensor_length()));
      return;
    }

    DCHECK(custom_input.tensor_length() != 0);

    // Validate the proto::CustomInput metadata.
    if (metadata_utils::ValidateMetadataCustomInput(custom_input) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      success = false;
    } else {
      (*result)[index] =
          ProcessSingleCustomInput(custom_input, feature_processor_state.get());
    }
  }

  // Processing of the feature list has completed.
  DCHECK(custom_inputs.empty());
  if (!success || feature_processor_state->error()) {
    result->clear();
    feature_processor_state->SetError(
        stats::FeatureProcessingError::kCustomInputError);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(feature_processor_state),
                     std::move(*result)));
}

template <typename IndexType>
void CustomInputProcessor::OnGotProcessedValue(
    base::flat_map<IndexType, proto::CustomInput> custom_inputs,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    std::unique_ptr<base::flat_map<IndexType, Tensor>> result,
    TemplateCallback<IndexType> callback,
    IndexType current_index,
    size_t current_tensor_length,
    bool error,
    Tensor current_value) {
  if (error) {
    feature_processor_state->SetError(
        stats::FeatureProcessingError::kCustomInputError);
  } else {
    DCHECK_EQ(current_tensor_length, current_value.size());
  }
  (*result)[current_index] = std::move(current_value);
  ProcessIndexType<IndexType>(std::move(custom_inputs),
                              std::move(feature_processor_state),
                              std::move(result), std::move(callback));
}

using SqlCustomInputIndex = std::pair<int, int>;
template void CustomInputProcessor::ProcessIndexType(
    base::flat_map<SqlCustomInputIndex, proto::CustomInput> custom_inputs,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    std::unique_ptr<base::flat_map<SqlCustomInputIndex, Tensor>> result,
    TemplateCallback<std::pair<int, int>> callback);

template void CustomInputProcessor::OnGotProcessedValue(
    base::flat_map<SqlCustomInputIndex, proto::CustomInput> custom_inputs,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    std::unique_ptr<base::flat_map<SqlCustomInputIndex, Tensor>> result,
    TemplateCallback<SqlCustomInputIndex> callback,
    SqlCustomInputIndex current_index,
    size_t current_tensor_length,
    bool success,
    Tensor current_value);

QueryProcessor::Tensor CustomInputProcessor::ProcessSingleCustomInput(
    const proto::CustomInput& custom_input,
    FeatureProcessorState* feature_processor_state) {
  std::vector<ProcessedValue> tensor_result;
  if (custom_input.fill_policy() == proto::CustomInput::UNKNOWN_FILL_POLICY) {
    // When parsing a CustomInput object, if the fill policy is not
    // supported by the current version of the client, the fill policy field
    // will not be filled. When this happens, the custom input processor
    // will either use the default values to generate an input tensor or
    // fail the model execution.
    tensor_result = std::vector<ProcessedValue>(
        custom_input.default_value().begin(),
        custom_input.default_value().begin() + custom_input.tensor_length());
  } else if (custom_input.fill_policy() ==
             proto::CustomInput::FILL_PREDICTION_TIME) {
    if (!AddPredictionTime(custom_input, tensor_result))
      feature_processor_state->SetError(
          stats::FeatureProcessingError::kCustomInputError);
  } else if (custom_input.fill_policy() ==
             proto::CustomInput::TIME_RANGE_BEFORE_PREDICTION) {
    if (!AddTimeRangeBeforePrediction(custom_input, tensor_result))
      feature_processor_state->SetError(
          stats::FeatureProcessingError::kCustomInputError);
  } else if (custom_input.fill_policy() ==
             proto::CustomInput::FILL_FROM_INPUT_CONTEXT) {
    if (!AddFromInputContext(custom_input, feature_processor_state,
                             tensor_result))
      feature_processor_state->SetError(
          stats::FeatureProcessingError::kCustomInputError);
  } else if (custom_input.fill_policy() ==
             proto::CustomInput::PRICE_TRACKING_HINTS) {
    feature_processor_state->SetError(
        stats::FeatureProcessingError::kCustomInputError);
    NOTREACHED() << "InputDelegate is not found";
  }

  return tensor_result;
}

bool CustomInputProcessor::AddFromInputContext(
    const proto::CustomInput& custom_input,
    FeatureProcessorState* feature_processor_state,
    std::vector<ProcessedValue>& out_tensor) {
  if (custom_input.tensor_length() != 1) {
    return false;
  }
  const auto& input_context = feature_processor_state->input_context();
  auto custom_input_iter = custom_input.additional_args().find("name");
  if (custom_input_iter == custom_input.additional_args().end()) {
    return false;
  }

  auto input_context_iter =
      input_context->metadata_args.find(custom_input_iter->second);
  if (input_context_iter == input_context->metadata_args.end()) {
    return false;
  }

  out_tensor.emplace_back(input_context_iter->second);
  return true;
}

bool CustomInputProcessor::AddPredictionTime(
    const proto::CustomInput& custom_input,
    std::vector<ProcessedValue>& out_tensor) {
  if (custom_input.tensor_length() != 1) {
    return false;
  }
  out_tensor.emplace_back(prediction_time_);
  return true;
}

bool CustomInputProcessor::AddTimeRangeBeforePrediction(
    const proto::CustomInput& custom_input,
    std::vector<ProcessedValue>& out_tensor) {
  if (custom_input.tensor_length() != 2) {
    return false;
  }

  constexpr char kBucketCountArg[] = "bucket_count";
  absl::optional<int> bucket_count =
      GetArgAsInt(custom_input.additional_args(), kBucketCountArg);

  if (bucket_count.has_value()) {
    out_tensor.emplace_back(prediction_time_ -
                            base::Days(bucket_count.value()));
    out_tensor.emplace_back(prediction_time_);
  } else {
    return false;
  }

  return true;
}

}  // namespace segmentation_platform::processing

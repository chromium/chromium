// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/feature_processor_state.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"

namespace segmentation_platform {

FeatureProcessorState::FeatureProcessorState(
    base::Time prediction_time,
    base::TimeDelta bucket_duration,
    OptimizationTarget segment_id,
    std::unique_ptr<std::deque<proto::InputFeature>> input_features,
    FeatureListQueryProcessor::FeatureProcessorCallback callback)
    : prediction_time_(prediction_time),
      bucket_duration_(bucket_duration),
      segment_id_(segment_id),
      input_features_(std::move(input_features)),
      callback_(std::move(callback)) {}

FeatureProcessorState::~FeatureProcessorState() = default;

void FeatureProcessorState::SetError() {
  error_ = true;
  input_tensor_.clear();
}

proto::InputFeature FeatureProcessorState::PopNextInputFeature() {
  proto::InputFeature input_feature = std::move(input_features_->front());
  input_features_->pop_front();
  return input_feature;
}

bool FeatureProcessorState::IsFeatureListEmpty() const {
  return input_features_->empty();
}

void FeatureProcessorState::RunCallback() {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), error_, input_tensor_));
}

void FeatureProcessorState::AppendInputTensor(const std::vector<float>& data) {
  input_tensor_.insert(input_tensor_.end(), data.begin(), data.end());
}

void FeatureProcessorState::AppendInputTensor(
    const std::vector<ProcessedValue>& data) {
  std::vector<float> tensor_result;
  for (auto& value : data) {
    if (value.type == ProcessedValue::Type::FLOAT) {
      tensor_result.push_back(value.float_val);
    } else {
      SetError();
      return;
    }
  }

  input_tensor_.insert(input_tensor_.end(), tensor_result.begin(),
                       tensor_result.end());
}

}  // namespace segmentation_platform

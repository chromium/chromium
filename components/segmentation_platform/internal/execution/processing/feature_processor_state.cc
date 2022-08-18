// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"

#include "base/time/time.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform::processing {

FeatureProcessorState::Data::Data(proto::InputFeature input)
    : input_feature(std::move(input)) {}

FeatureProcessorState::Data::Data(proto::TrainingOutput output)
    : output_feature(std::move(output)) {}

FeatureProcessorState::Data::Data(Data&& other)
    : input_feature(std::move(other.input_feature)),
      output_feature(std::move(other.output_feature)) {}

FeatureProcessorState::Data::~Data() = default;

bool FeatureProcessorState::Data::IsInput() const {
  DCHECK(!input_feature.has_value() || !output_feature.has_value());
  DCHECK(input_feature.has_value() || output_feature.has_value());

  return input_feature.has_value();
}

FeatureProcessorState::FeatureProcessorState()
    : prediction_time_(base::Time::Now()),
      bucket_duration_(base::TimeDelta()),
      segment_id_(SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {}

FeatureProcessorState::FeatureProcessorState(
    base::Time prediction_time,
    base::TimeDelta bucket_duration,
    SegmentId segment_id,
    std::deque<Data> data,
    scoped_refptr<InputContext> input_context,
    FeatureListQueryProcessor::FeatureProcessorCallback callback)
    : prediction_time_(prediction_time),
      bucket_duration_(bucket_duration),
      segment_id_(segment_id),
      data_(std::move(data)),
      input_context_(std::move(input_context)),
      callback_(std::move(callback)) {}

FeatureProcessorState::~FeatureProcessorState() = default;

void FeatureProcessorState::SetError(stats::FeatureProcessingError error) {
  stats::RecordFeatureProcessingError(segment_id_, error);
  DVLOG(1) << "Processing error occured: model "
           << SegmentIdToHistogramVariant(segment_id_) << " failed with "
           << stats::FeatureProcessingErrorToString(error);
  error_ = true;
  input_tensor_.clear();
}

FeatureProcessorState::Data FeatureProcessorState::PopNextData() {
  Data data = std::move(data_.front());
  data_.pop_front();
  return data;
}

bool FeatureProcessorState::IsFeatureListEmpty() const {
  return data_.empty();
}

void FeatureProcessorState::RunCallback() {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), error_, input_tensor_,
                                output_tensor_));
}

void FeatureProcessorState::AppendTensor(
    const std::vector<ProcessedValue>& data,
    bool is_input) {
  std::vector<float> tensor_result;
  for (auto& value : data) {
    if (value.type == ProcessedValue::Type::FLOAT) {
      tensor_result.push_back(value.float_val);
    } else {
      SetError(stats::FeatureProcessingError::kResultTensorError);
      return;
    }
  }

  if (is_input) {
    input_tensor_.insert(input_tensor_.end(), tensor_result.begin(),
                         tensor_result.end());
  } else {
    output_tensor_.insert(output_tensor_.end(), tensor_result.begin(),
                          tensor_result.end());
  }
}

}  // namespace segmentation_platform::processing

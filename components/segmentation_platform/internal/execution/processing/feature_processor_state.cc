// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/config.h"

namespace segmentation_platform::processing {

FeatureProcessorState::FeatureProcessorState()
    : prediction_time_(base::Time::Now()),
      bucket_duration_(base::TimeDelta()),
      segment_id_(SegmentId::OPTIMIZATION_TARGET_UNKNOWN) {}

FeatureProcessorState::FeatureProcessorState(
    FeatureProcessorStateId id,
    base::Time prediction_time,
    base::Time observation_time,
    base::TimeDelta bucket_duration,
    SegmentId segment_id,
    scoped_refptr<InputContext> input_context,
    FeatureListQueryProcessor::FeatureProcessorCallback callback)
    : id_(id),
      prediction_time_(prediction_time),
      observation_time_(observation_time),
      bucket_duration_(bucket_duration),
      segment_id_(segment_id),
      input_context_(std::move(input_context)),
      callback_(std::move(callback)) {}

FeatureProcessorState::~FeatureProcessorState() = default;

void FeatureProcessorState::SetError(stats::FeatureProcessingError error,
                                     const std::string& message) {
  stats::RecordFeatureProcessingError(segment_id_, error);
  LOG(ERROR) << "Processing error occured: model "
             << SegmentIdToHistogramVariant(segment_id_) << " failed with "
             << stats::FeatureProcessingErrorToString(error)
             << ", message: " << message;
  error_ = true;
  input_tensor_.clear();
}

base::WeakPtr<FeatureProcessorState> FeatureProcessorState::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::optional<std::pair<std::unique_ptr<QueryProcessor>, bool>>
FeatureProcessorState::PopNextProcessor() {
  std::optional<std::pair<std::unique_ptr<QueryProcessor>, bool>>
      next_processor;
  if (!out_processors_.empty()) {
    std::unique_ptr<QueryProcessor> processor =
        std::move(out_processors_.front());
    out_processors_.pop_front();
    next_processor = std::make_pair(std::move(processor), false);
  } else if (!in_processors_.empty()) {
    std::unique_ptr<QueryProcessor> processor =
        std::move(in_processors_.front());
    in_processors_.pop_front();
    next_processor = std::make_pair(std::move(processor), true);
  }
  return next_processor;
}

void FeatureProcessorState::AppendProcessor(
    std::unique_ptr<QueryProcessor> processor,
    bool is_input) {
  if (is_input) {
    in_processors_.emplace_back(std::move(processor));
  } else {
    out_processors_.emplace_back(std::move(processor));
  }
}

void FeatureProcessorState::AppendIndexedTensors(
    const QueryProcessor::IndexedTensors& result,
    bool is_input) {
  if (is_input) {
    for (const auto& item : result) {
      input_tensor_[item.first] = item.second;
    }
  } else {
    for (const auto& item : result) {
      output_tensor_[item.first] = item.second;
    }
  }
}

void FeatureProcessorState::OnFinishProcessing() {
  std::vector<float> input;
  std::vector<float> output;
  if (!error_) {
    input = MergeTensors(std::move(input_tensor_));
    output = MergeTensors(std::move(output_tensor_));
    stats::RecordFeatureProcessingError(
        segment_id_, stats::FeatureProcessingError::kSuccess);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), error_, std::move(input),
                                std::move(output)));
}

std::vector<float> FeatureProcessorState::MergeTensors(
    const QueryProcessor::IndexedTensors& tensor) {
  std::vector<float> result;
  if (metadata_utils::ValidateIndexedTensors(tensor, tensor.size()) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    // Note that since the state does not know the expected size, if a tensor is
    // missing from the end of the indexed tensor, this validation will not
    // fail.
    SetError(stats::FeatureProcessingError::kResultTensorError);
  } else {
    for (size_t i = 0; i < tensor.size(); ++i) {
      for (const ProcessedValue& value : tensor.at(i)) {
        if (value.type == ProcessedValue::Type::FLOAT) {
          result.push_back(value.float_val);
        } else {
          SetError(stats::FeatureProcessingError::kResultTensorError,
                   "Expected ProcessedValue::Type::FLOAT but found " +
                       base::NumberToString(static_cast<int>(value.type)));
          return result;
        }
      }
    }
  }
  return result;
}

}  // namespace segmentation_platform::processing

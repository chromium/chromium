// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/uma_feature_processor.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/processing/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"

namespace segmentation_platform::processing {

namespace {

const proto::UMAFeature& GetAsUMA(const Data& data) {
  DCHECK(data.input_feature.has_value() || data.output_feature.has_value());

  if (data.input_feature.has_value()) {
    return data.input_feature->uma_feature();
  }

  return data.output_feature->uma_output().uma_feature();
}

}  // namespace

UmaFeatureProcessor::UmaFeatureProcessor(
    base::flat_map<FeatureIndex, Data>&& uma_features,
    SignalDatabase* signal_database,
    FeatureAggregator* feature_aggregator,
    const base::Time prediction_time,
    const base::Time observation_time,
    const base::TimeDelta bucket_duration,
    const SegmentId segment_id,
    bool is_output)
    : uma_features_(std::move(uma_features)),
      signal_database_(signal_database),
      feature_aggregator_(feature_aggregator),
      prediction_time_(prediction_time),
      observation_time_(observation_time),
      bucket_duration_(bucket_duration),
      segment_id_(segment_id),
      is_output_(is_output) {}

UmaFeatureProcessor::~UmaFeatureProcessor() = default;

void UmaFeatureProcessor::Process(
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessorCallback callback) {
  feature_processor_state_ = std::move(feature_processor_state);
  callback_ = std::move(callback);

  size_t max_bucket_count = 0;
  for (const auto& feature : uma_features_) {
    // Validate the proto::UMAFeature metadata.
    const proto::UMAFeature& uma_feature = GetAsUMA(feature.second);
    if (metadata_utils::ValidateMetadataUmaFeature(uma_feature) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      feature_processor_state_->SetError(
          stats::FeatureProcessingError::kUmaValidationError);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_),
                                    std::move(feature_processor_state_),
                                    std::move(result_)));
      return;
    }

    if (max_bucket_count < uma_feature.bucket_count()) {
      max_bucket_count = uma_feature.bucket_count();
    }
  }

  ProcessOnGotAllSamples(*signal_database_->GetAllSamples());
}

void UmaFeatureProcessor::GetStartAndEndTime(size_t bucket_count,
                                             base::Time& start_time,
                                             base::Time& end_time) const {
  base::TimeDelta duration = bucket_duration_ * bucket_count;
  if (is_output_) {
    if (observation_time_ == base::Time()) {
      start_time = prediction_time_ - duration;
      end_time = prediction_time_;
    } else if (observation_time_ - prediction_time_ > duration) {
      start_time = observation_time_ - duration;
      end_time = observation_time_;
    } else {
      start_time = prediction_time_;
      end_time = observation_time_;
    }
  } else {
    start_time = prediction_time_ - duration;
    end_time = prediction_time_;
  }
}

void UmaFeatureProcessor::ProcessOnGotAllSamples(
    const std::vector<SignalDatabase::DbEntry>& samples) {
  while (!uma_features_.empty()) {
    if (feature_processor_state_->error()) {
      break;
    }

    const auto& it = uma_features_.begin();
    proto::UMAFeature next_feature = GetAsUMA(it->second);
    FeatureIndex index = it->first;
    uma_features_.erase(it);

    ProcessSingleUmaFeature(samples, index, next_feature);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(feature_processor_state_),
                     std::move(result_)));
}

void UmaFeatureProcessor::ProcessSingleUmaFeature(
    const std::vector<SignalDatabase::DbEntry>& samples,
    FeatureIndex index,
    const proto::UMAFeature& feature) {
  // Enum histograms can optionally only accept some of the enum values.
  // While the proto::UMAFeature is available, capture a vector of the
  // accepted enum values. An empty vector is ignored (all values are
  // considered accepted).
  std::vector<int32_t> accepted_enum_ids{};
  if (feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
    for (int i = 0; i < feature.enum_ids_size(); ++i) {
      accepted_enum_ids.emplace_back(feature.enum_ids(i));
    }
  }

  base::Time start_time;
  base::Time end_time;
  GetStartAndEndTime(feature.bucket_count(), start_time, end_time);
  base::ElapsedTimer timer;

  // We now have all the data required to process a single feature, so we can
  // process it synchronously, and insert it into the
  // FeatureProcessorState::input_tensor so we can later pass it to the ML model
  // executor.
  absl::optional<std::vector<float>> result = feature_aggregator_->Process(
      feature.type(), feature.name_hash(), feature.aggregation(),
      feature.bucket_count(), start_time, end_time, bucket_duration_,
      accepted_enum_ids, samples);

  // If no feature data is available, use the default values specified instead.
  if (result.has_value()) {
    const std::vector<float>& feature_data = result.value();
    DCHECK_EQ(feature.tensor_length(), feature_data.size());
    result_[index] =
        std::vector<ProcessedValue>(feature_data.begin(), feature_data.end());
  } else {
    DCHECK_EQ(feature.tensor_length(),
              static_cast<unsigned int>(feature.default_values_size()))
        << " Mismatch between expected value size and default value size for "
           "UMA feature '"
        << feature.name()
        << "'. Did you forget to specify a default value for this feature?";
    result_[index] = std::vector<ProcessedValue>(
        feature.default_values().begin(), feature.default_values().end());
  }

  stats::RecordModelExecutionDurationFeatureProcessing(segment_id_,
                                                       timer.Elapsed());
}

}  // namespace segmentation_platform::processing

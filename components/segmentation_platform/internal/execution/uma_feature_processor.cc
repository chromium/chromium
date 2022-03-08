// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/uma_feature_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/stats.h"

namespace segmentation_platform {

UmaFeatureProcessor::UmaFeatureProcessor(
    SignalDatabase* signal_database,
    std::unique_ptr<FeatureAggregator> feature_aggregator)
    : signal_database_(signal_database),
      feature_aggregator_(std::move(feature_aggregator)) {}

UmaFeatureProcessor::~UmaFeatureProcessor() = default;

void UmaFeatureProcessor::ProcessUmaFeature(
    const proto::UMAFeature& feature,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    FeatureListQueryProcessorCallback callback) {
  // Skip collection-only features.
  if (feature.bucket_count() == 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::move(feature_processor_state)));
    return;
  }

  // Validate the proto::UMAFeature metadata.
  if (metadata_utils::ValidateMetadataUmaFeature(feature) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    feature_processor_state->SetError();
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::move(feature_processor_state)));
    return;
  }

  auto name_hash = feature.name_hash();

  // Enum histograms can optionally only accept some of the enum values.
  // While the proto::UMAFeature is available, capture a vector of the
  // accepted enum values. An empty vector is ignored (all values are
  // considered accepted).
  std::vector<int32_t> accepted_enum_ids{};
  if (feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
    for (int i = 0; i < feature.enum_ids_size(); ++i)
      accepted_enum_ids.emplace_back(feature.enum_ids(i));
  }

  // Only fetch data that is relevant for the current proto::UMAFeature, since
  // the FeatureAggregator assumes that only relevant data is given to it.
  base::TimeDelta duration =
      feature_processor_state->bucket_duration() * feature.bucket_count();
  base::Time start_time = feature_processor_state->prediction_time() - duration;

  // Fetch the relevant samples for the current proto::UMAFeature. Once the
  // result has come back, it will be processed and inserted into the
  // FeatureProcessorState::input_tensor and will then invoke
  // ProcessInputFeatures(...) again to ensure we continue until all features
  // have been processed. Note: All parameters from the FeatureProcessorState
  // need to be captured locally before invoking GetSamples, because the state
  // is moved with the callback, and the order of the move and accessing the
  // members while invoking GetSamples is not guaranteed.
  auto signal_type = feature.type();
  auto prediction_time = feature_processor_state->prediction_time();
  signal_database_->GetSamples(
      signal_type, name_hash, start_time, prediction_time,
      base::BindOnce(&UmaFeatureProcessor::OnGetSamplesForUmaFeature,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(feature_processor_state), feature,
                     accepted_enum_ids));
}

void UmaFeatureProcessor::OnGetSamplesForUmaFeature(
    FeatureListQueryProcessorCallback callback,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    const proto::UMAFeature& feature,
    const std::vector<int32_t>& accepted_enum_ids,
    std::vector<SignalDatabase::Sample> samples) {
  base::ElapsedTimer timer;
  // HISTOGRAM_ENUM features might require us to filter out the result to only
  // keep enum values that match the accepted list. If the accepted list is'
  // empty, all histogram enum values are kept.
  // The SignalDatabase does not currently support this type of data filter,
  // so instead we are doing this here.
  if (feature.type() == proto::SignalType::HISTOGRAM_ENUM) {
    feature_aggregator_->FilterEnumSamples(accepted_enum_ids, samples);
  }

  // We now have all the data required to process a single feature, so we can
  // process it synchronously, and insert it into the
  // FeatureProcessorState::input_tensor so we can later pass it to the ML model
  // executor.
  std::vector<float> feature_data = feature_aggregator_->Process(
      feature.type(), feature.aggregation(), feature.bucket_count(),
      feature_processor_state->prediction_time(),
      feature_processor_state->bucket_duration(), samples);

  DCHECK_EQ(feature.tensor_length(), feature_data.size());
  feature_processor_state->AppendInputTensor(feature_data);

  stats::RecordModelExecutionDurationFeatureProcessing(
      feature_processor_state->segment_id(), timer.Elapsed());

  // Continue with the rest of the features.
  std::move(callback).Run(std::move(feature_processor_state));
}

}  // namespace segmentation_platform

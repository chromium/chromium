// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/custom_input_processor.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/execution/processing/input_delegate.h"
#include "components/segmentation_platform/internal/execution/processing/sql_feature_processor.h"
#include "components/segmentation_platform/internal/execution/processing/uma_feature_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"

namespace segmentation_platform::processing {

namespace {
// Index not actually used for legacy code in FeatureQueryProcessor.
const int kIndexNotUsed = 0;
}  // namespace

FeatureListQueryProcessor::FeatureListQueryProcessor(
    StorageService* storage_service,
    std::unique_ptr<InputDelegateHolder> input_delegate_holder,
    std::unique_ptr<FeatureAggregator> feature_aggregator)
    : storage_service_(storage_service),
      input_delegate_holder_(std::move(input_delegate_holder)),
      feature_aggregator_(std::move(feature_aggregator)) {}

FeatureListQueryProcessor::~FeatureListQueryProcessor() = default;

void FeatureListQueryProcessor::ProcessFeatureList(
    const proto::SegmentationModelMetadata& model_metadata,
    scoped_refptr<InputContext> input_context,
    SegmentId segment_id,
    base::Time prediction_time,
    ProcessOption process_option,
    FeatureProcessorCallback callback) {
  // The total bucket duration is defined by product of the bucket_duration
  // value and the length of related time_unit field, e.g. 28 * length(DAY).
  base::TimeDelta time_unit_len = metadata_utils::GetTimeUnit(model_metadata);
  base::TimeDelta bucket_duration =
      model_metadata.bucket_duration() * time_unit_len;

  // Grab the metadata for all the features, which will be processed one at a
  // time, before executing the model.
  std::deque<FeatureProcessorState::Data> features;
  if (process_option == ProcessOption::kInputsOnly ||
      process_option == ProcessOption::kInputsAndOutputs) {
    for (int i = 0; i < model_metadata.features_size(); ++i) {
      proto::InputFeature input_feature;
      input_feature.mutable_uma_feature()->CopyFrom(model_metadata.features(i));
      features.emplace_back(std::move(input_feature));
    }
    for (int i = 0; i < model_metadata.input_features_size(); ++i)
      features.emplace_back(model_metadata.input_features(i));
  }

  if (process_option == ProcessOption::kOutputsOnly ||
      process_option == ProcessOption::kInputsAndOutputs) {
    for (auto& output : model_metadata.training_outputs().outputs()) {
      DCHECK(output.has_uma_output()) << "Currently only support UMA output.";
      features.emplace_back(std::move(output));
    }
  }

  // Capture all the relevant metadata information into a FeatureProcessorState.
  auto feature_processor_state = std::make_unique<FeatureProcessorState>(
      prediction_time, bucket_duration, segment_id, std::move(features),
      input_context, std::move(callback));

  ProcessNext(std::move(feature_processor_state));
}

void FeatureListQueryProcessor::ProcessNext(
    std::unique_ptr<FeatureProcessorState> feature_processor_state) {
  // Finished processing all input features or an error occurred.
  if (feature_processor_state->IsFeatureListEmpty() ||
      feature_processor_state->error()) {
    feature_processor_state->RunCallback();
    return;
  }

  // Get next input feature to process.
  FeatureProcessorState::Data data = feature_processor_state->PopNextData();
  std::unique_ptr<QueryProcessor> processor;

  // Check either input or output has value.
  DCHECK(data.input_feature.has_value() || data.output_feature.has_value());
  DCHECK(!data.input_feature.has_value() || !data.output_feature.has_value());

  // Process all the features in-order, starting with the first feature.
  if (data.input_feature.has_value()) {
    if (data.input_feature->has_uma_feature()) {
      base::flat_map<QueryProcessor::FeatureIndex, proto::UMAFeature> queries =
          {{kIndexNotUsed, data.input_feature->uma_feature()}};
      processor = GetUmaFeatureProcessor(std::move(queries),
                                         feature_processor_state.get());
    } else if (data.input_feature->has_custom_input()) {
      base::flat_map<QueryProcessor::FeatureIndex, proto::CustomInput> queries =
          {{kIndexNotUsed, data.input_feature->custom_input()}};
      processor = std::make_unique<CustomInputProcessor>(
          std::move(queries), feature_processor_state->prediction_time(),
          input_delegate_holder_.get());
    } else if (data.input_feature->has_sql_feature()) {
      auto* ukm_manager = storage_service_->ukm_data_manager();
      if (!ukm_manager->IsUkmEngineEnabled()) {
        // UKM engine is disabled, feature cannot be processed.
        feature_processor_state->SetError(
            stats::FeatureProcessingError::kUkmEngineDisabled);
        feature_processor_state->RunCallback();
        return;
      }
      SqlFeatureProcessor::QueryList queries = {
          {kIndexNotUsed, data.input_feature->sql_feature()}};
      processor = std::make_unique<SqlFeatureProcessor>(
          std::move(queries), feature_processor_state->prediction_time(),
          input_delegate_holder_.get(), ukm_manager->GetUkmDatabase());
    }
  } else {
    // Process output features
    if (data.output_feature->has_uma_output()) {
      DCHECK(data.output_feature->uma_output().has_uma_feature());
      base::flat_map<QueryProcessor::FeatureIndex, proto::UMAFeature> queries =
          {{kIndexNotUsed, data.output_feature->uma_output().uma_feature()}};
      processor = GetUmaFeatureProcessor(std::move(queries),
                                         feature_processor_state.get());
    }
  }

  auto* processor_ptr = processor.get();
  processor_ptr->Process(
      std::move(feature_processor_state),
      base::BindOnce(&FeatureListQueryProcessor::OnFeatureProcessed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(processor),
                     data.input_feature.has_value()));
}

void FeatureListQueryProcessor::OnFeatureProcessed(
    std::unique_ptr<QueryProcessor> feature_processor,
    bool is_input,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessor::IndexedTensors result) {
  feature_processor_state->AppendTensor(result[kIndexNotUsed], is_input);
  ProcessNext(std::move(feature_processor_state));
}

std::unique_ptr<UmaFeatureProcessor>
FeatureListQueryProcessor::GetUmaFeatureProcessor(
    base::flat_map<FeatureIndex, proto::UMAFeature>&& uma_features,
    FeatureProcessorState* feature_processor_state) {
  return std::make_unique<UmaFeatureProcessor>(
      std::move(uma_features), storage_service_->signal_database(),
      feature_aggregator_.get(), feature_processor_state->prediction_time(),
      feature_processor_state->bucket_duration(),
      feature_processor_state->segment_id());
}

}  // namespace segmentation_platform::processing

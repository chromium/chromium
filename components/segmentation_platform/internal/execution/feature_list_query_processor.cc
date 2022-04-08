// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/custom_input_processor.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"
#include "components/segmentation_platform/internal/execution/sql_feature_processor.h"
#include "components/segmentation_platform/internal/execution/uma_feature_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/stats.h"

namespace segmentation_platform {

namespace {
// Index not actually used for legacy code in FeatureQueryProcessor.
const int kIndexNotUsed = 0;
}  // namespace

FeatureListQueryProcessor::FeatureListQueryProcessor(
    SignalDatabase* signal_database,
    std::unique_ptr<FeatureAggregator> feature_aggregator)
    : signal_database_(signal_database),
      feature_aggregator_(std::move(feature_aggregator)) {}

FeatureListQueryProcessor::~FeatureListQueryProcessor() = default;

void FeatureListQueryProcessor::ProcessFeatureList(
    const proto::SegmentationModelMetadata& model_metadata,
    OptimizationTarget segment_id,
    base::Time prediction_time,
    FeatureProcessorCallback callback) {
  // The total bucket duration is defined by product of the bucket_duration
  // value and the length of related time_unit field, e.g. 28 * length(DAY).
  base::TimeDelta time_unit_len = metadata_utils::GetTimeUnit(model_metadata);
  base::TimeDelta bucket_duration =
      model_metadata.bucket_duration() * time_unit_len;

  // Grab the metadata for all the features, which will be processed one at a
  // time, before executing the model.
  auto input_features = std::make_unique<std::deque<proto::InputFeature>>();
  for (int i = 0; i < model_metadata.features_size(); ++i) {
    proto::InputFeature input_feature;
    input_feature.mutable_uma_feature()->CopyFrom(model_metadata.features(i));
    input_features->emplace_back(input_feature);
  }
  for (int i = 0; i < model_metadata.input_features_size(); ++i)
    input_features->emplace_back(model_metadata.input_features(i));

  // Capture all the relevant metadata information into a FeatureProcessorState.
  auto feature_processor_state = std::make_unique<FeatureProcessorState>(
      prediction_time, bucket_duration, segment_id, std::move(input_features),
      std::move(callback));

  ProcessNextInputFeature(std::move(feature_processor_state));
}

void FeatureListQueryProcessor::ProcessNextInputFeature(
    std::unique_ptr<FeatureProcessorState> feature_processor_state) {
  // Finished processing all input features or an error occurred.
  if (feature_processor_state->IsFeatureListEmpty() ||
      feature_processor_state->error()) {
    feature_processor_state->RunCallback();
    return;
  }

  // Get next input feature to process.
  proto::InputFeature input_feature =
      feature_processor_state->PopNextInputFeature();
  std::unique_ptr<QueryProcessor> processor;

  // Process all the features in-order, starting with the first feature.
  if (input_feature.has_uma_feature()) {
    base::flat_map<QueryProcessor::FeatureIndex, proto::UMAFeature> queries = {
        {kIndexNotUsed, input_feature.uma_feature()}};
    processor = std::make_unique<UmaFeatureProcessor>(
        std::move(queries), signal_database_, feature_aggregator_.get(),
        feature_processor_state->prediction_time(),
        feature_processor_state->bucket_duration(),
        feature_processor_state->segment_id());
  } else if (input_feature.has_custom_input()) {
    base::flat_map<QueryProcessor::FeatureIndex, proto::CustomInput> queries = {
        {kIndexNotUsed, input_feature.custom_input()}};
    processor = std::make_unique<CustomInputProcessor>(
        std::move(queries), feature_processor_state->prediction_time());
  } else if (input_feature.has_sql_feature()) {
    SqlFeatureProcessor::QueryList queries = {
        {kIndexNotUsed, input_feature.sql_feature()}};
    processor = std::make_unique<SqlFeatureProcessor>(
        std::move(queries), feature_processor_state->prediction_time());
  }

  auto* processor_ptr = processor.get();
  processor_ptr->Process(
      std::move(feature_processor_state),
      base::BindOnce(&FeatureListQueryProcessor::OnFeatureProcessed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(processor)));
}

void FeatureListQueryProcessor::OnFeatureProcessed(
    std::unique_ptr<QueryProcessor> feature_processor,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessor::IndexedTensors result) {
  feature_processor_state->AppendInputTensor(result[kIndexNotUsed]);
  ProcessNextInputFeature(std::move(feature_processor_state));
}

}  // namespace segmentation_platform

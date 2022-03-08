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
    : uma_feature_processor_(signal_database, std::move(feature_aggregator)) {}

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

  if (input_feature.has_uma_feature()) {
    // Process all the features in-order, starting with the first feature.
    uma_feature_processor_.ProcessUmaFeature(
        input_feature.uma_feature(), std::move(feature_processor_state),
        base::BindOnce(&FeatureListQueryProcessor::ProcessNextInputFeature,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (input_feature.has_custom_input()) {
    custom_input_processor_.ProcessCustomInput(
        input_feature.custom_input(), std::move(feature_processor_state),
        base::BindOnce(&FeatureListQueryProcessor::ProcessNextInputFeature,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (input_feature.has_sql_feature()) {
    std::map<SqlFeatureProcessor::FeatureIndex, proto::SqlFeature> queries = {
        {kIndexNotUsed, input_feature.sql_feature()}};
    auto sql_feature_processor = std::make_unique<SqlFeatureProcessor>(queries);
    auto* sql_feature_processor_ptr = sql_feature_processor.get();
    sql_feature_processor_ptr->Process(
        std::move(feature_processor_state),
        base::BindOnce(&FeatureListQueryProcessor::OnSqlQueryProcessed,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(sql_feature_processor)));
  }
}

void FeatureListQueryProcessor::OnSqlQueryProcessed(
    std::unique_ptr<SqlFeatureProcessor> sql_feature_processor,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessor::IndexedTensors result) {
  feature_processor_state->AppendInputTensor(result[kIndexNotUsed]);
  ProcessNextInputFeature(std::move(feature_processor_state));
}

}  // namespace segmentation_platform

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/custom_input_processor.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/internal/execution/processing/sql_feature_processor.h"
#include "components/segmentation_platform/internal/execution/processing/uma_feature_processor.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/internal/ukm_data_manager.h"
#include "components/segmentation_platform/public/input_delegate.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::processing {

Data::Data(proto::InputFeature input) {
  if (input.has_uma_feature()) {
    type = DataType::INPUT_UMA;
  } else if (input.has_custom_input()) {
    type = DataType::INPUT_CUSTOM;
  } else if (input.has_sql_feature()) {
    type = DataType::INPUT_UKM;
  }
  input_feature = std::move(input);
}

Data::Data(proto::TrainingOutput output)
    : type(DataType::OUTPUT_UMA), output_feature(std::move(output)) {}

Data::Data(Data&& other)
    : input_feature(std::move(other.input_feature)),
      output_feature(std::move(other.output_feature)) {}

Data::~Data() = default;

bool Data::IsInput() const {
  DCHECK(!input_feature.has_value() || !output_feature.has_value());
  DCHECK(input_feature.has_value() || output_feature.has_value());

  return input_feature.has_value();
}

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
    base::Time observation_time,
    ProcessOption process_option,
    FeatureProcessorCallback callback) {
  // The total bucket duration is defined by product of the bucket_duration
  // value and the length of related time_unit field, e.g. 28 * length(DAY).
  base::TimeDelta time_unit_len = metadata_utils::GetTimeUnit(model_metadata);
  base::TimeDelta bucket_duration =
      model_metadata.bucket_duration() * time_unit_len;

  // Grab the metadata for all the features, which will be processed one type at
  // a time, before executing the model.
  std::map<Data::DataType, base::flat_map<QueryProcessor::FeatureIndex, Data>>
      data_to_process;
  QueryProcessor::FeatureIndex in_index = 0;
  QueryProcessor::FeatureIndex out_index = 0;

  if (process_option == ProcessOption::kInputsOnly ||
      process_option == ProcessOption::kInputsAndOutputs) {
    for (int i = 0; i < model_metadata.features_size(); ++i) {
      proto::InputFeature input_feature;
      input_feature.mutable_uma_feature()->CopyFrom(model_metadata.features(i));
      data_to_process[Data::DataType::INPUT_UMA].emplace(
          std::make_pair(in_index, std::move(input_feature)));
      in_index++;
    }
    for (int i = 0; i < model_metadata.input_features_size(); ++i) {
      Data data = Data(model_metadata.input_features(i));
      // Skip collection-only uma features.
      if (data.type == Data::DataType::INPUT_UMA &&
          data.input_feature->uma_feature().bucket_count() == 0) {
        continue;
      }

      // Skip custom inputs with no tensor length.
      if (data.type == Data::DataType::INPUT_CUSTOM &&
          data.input_feature->custom_input().tensor_length() == 0) {
        continue;
      }
      Data::DataType type = data.type;
      data_to_process[type].emplace(std::make_pair(in_index, std::move(data)));
      in_index++;
    }
  }

  if (process_option == ProcessOption::kOutputsOnly ||
      process_option == ProcessOption::kInputsAndOutputs) {
    for (auto& output : model_metadata.training_outputs().outputs()) {
      DCHECK(output.has_uma_output()) << "Currently only support UMA output.";
      // features.emplace_back(std::move(output));
      data_to_process[Data::DataType::OUTPUT_UMA].emplace(
          std::make_pair(out_index, std::move(output)));
      out_index++;
    }
  }

  // Capture all the relevant metadata information into a FeatureProcessorState.
  FeatureProcessorStateId id = state_id_generator.GenerateNextId();

  auto feature_processor_state = std::make_unique<FeatureProcessorState>(
      id, prediction_time, observation_time, bucket_duration, segment_id,
      input_context, std::move(callback));

  feature_processor_state_map_.emplace(
      std::make_pair(id, std::move(feature_processor_state)));

  CreateProcessors(*feature_processor_state_map_[id],
                   std::move(data_to_process));
}

void FeatureListQueryProcessor::CreateProcessors(
    FeatureProcessorState& feature_processor_state,
    std::map<Data::DataType,
             base::flat_map<QueryProcessor::FeatureIndex, Data>>&&
        data_to_process) {
  // Initialize a processors for each type of input features.
  UkmDataManager* ukm_manager = storage_service_->ukm_data_manager();
  for (auto& type : data_to_process) {
    switch (type.first) {
      case Data::DataType::INPUT_UMA:
        feature_processor_state.AppendProcessor(
            GetUmaFeatureProcessor(ukm_manager, std::move(type.second),
                                   feature_processor_state, false),
            true);
        break;
      case Data::DataType::INPUT_CUSTOM:
        feature_processor_state.AppendProcessor(
            std::make_unique<CustomInputProcessor>(
                std::move(type.second),
                feature_processor_state.prediction_time(),
                input_delegate_holder_.get()),
            true);
        break;
      case Data::DataType::INPUT_UKM:
        if (!ukm_manager->IsUkmEngineEnabled()) {
          // UKM engine is disabled, feature cannot be processed.
          feature_processor_state.SetError(
              stats::FeatureProcessingError::kUkmEngineDisabled);
          FinishProcessingAndCleanup(feature_processor_state);
          return;
        }
        feature_processor_state.AppendProcessor(
            std::make_unique<SqlFeatureProcessor>(
                std::move(type.second),
                feature_processor_state.prediction_time(),
                input_delegate_holder_.get(), ukm_manager->GetUkmDatabase()),
            true);
        break;
      case Data::DataType::OUTPUT_UMA:
        feature_processor_state.AppendProcessor(
            GetUmaFeatureProcessor(ukm_manager, std::move(type.second),
                                   feature_processor_state, true),
            false);
        break;
    }
  }

  Process(feature_processor_state);
}

void FeatureListQueryProcessor::Process(
    FeatureProcessorState& feature_processor_state) {
  std::optional<std::pair<std::unique_ptr<QueryProcessor>, bool>>
      next_processor = feature_processor_state.PopNextProcessor();
  if (next_processor.has_value()) {
    // Process input feature processors.
    std::unique_ptr<QueryProcessor> processor =
        std::move(next_processor.value().first);
    QueryProcessor* processor_ptr = processor.get();
    processor_ptr->Process(
        feature_processor_state,
        base::BindOnce(&FeatureListQueryProcessor::OnFeatureBatchProcessed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(processor),
                       next_processor.value().second,
                       feature_processor_state.GetWeakPtr()));
  } else {
    FinishProcessingAndCleanup(feature_processor_state);
  }
}

void FeatureListQueryProcessor::OnFeatureBatchProcessed(
    std::unique_ptr<QueryProcessor> feature_processor,
    bool is_input,
    base::WeakPtr<FeatureProcessorState> feature_processor_state,
    QueryProcessor::IndexedTensors result) {
  // Check for error state.
  if (feature_processor_state->error()) {
    FinishProcessingAndCleanup(*feature_processor_state);
    return;
  }

  // Store the indexed tensor result.
  feature_processor_state->AppendIndexedTensors(result, is_input);

  Process(*feature_processor_state);
}

std::unique_ptr<UmaFeatureProcessor>
FeatureListQueryProcessor::GetUmaFeatureProcessor(
    UkmDataManager* ukm_data_manager,
    base::flat_map<FeatureIndex, Data>&& uma_features,
    FeatureProcessorState& feature_processor_state,
    bool is_output) {
  return std::make_unique<UmaFeatureProcessor>(
      std::move(uma_features), storage_service_, storage_service_->profile_id(),
      feature_aggregator_.get(), feature_processor_state.prediction_time(),
      feature_processor_state.observation_time(),
      feature_processor_state.bucket_duration(),
      feature_processor_state.segment_id(), is_output);
}

void FeatureListQueryProcessor::FinishProcessingAndCleanup(
    FeatureProcessorState& feature_processor_state) {
  feature_processor_state.OnFinishProcessing();
  auto it = feature_processor_state_map_.find(feature_processor_state.id());
  feature_processor_state_map_.erase(it);
}

}  // namespace segmentation_platform::processing

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/sql_feature_processor.h"
#include <utility>

#include "base/containers/flat_map.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/execution/custom_input_processor.h"
#include "components/segmentation_platform/internal/execution/feature_processor_state.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {

SqlFeatureProcessor::SqlFeatureProcessor(QueryList&& queries,
                                         base::Time prediction_time)
    : queries_(std::move(queries)), prediction_time_(prediction_time) {}
SqlFeatureProcessor::~SqlFeatureProcessor() = default;

void SqlFeatureProcessor::Process(
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    QueryProcessorCallback callback) {
  DCHECK(!is_processed_);
  is_processed_ = true;
  callback_ = std::move(callback);
  feature_processor_state_ = std::move(feature_processor_state);

  // Prepare the sql queries for indexed custom inputs processing.
  base::flat_map<SqlFeatureAndBindValueIndices, proto::CustomInput> bind_values;
  for (const auto& query : queries_) {
    const proto::SqlFeature& feature = query.second;
    FeatureIndex sql_feature_index = query.first;

    // Validate the proto::SqlFeature metadata.
    if (metadata_utils::ValidateMetadataSqlFeature(feature) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      RunErrorCallback();
      return;
    }

    // Process bind values.
    // TODO(haileywang): bind_field_index is not currently being used.
    for (int i = 0; i < feature.bind_values_size(); ++i) {
      // The index is a pair of int constructed from:
      // 1. The index of the sql query, and
      // 2. The index of the bind value within the sql query.
      bind_values[std::make_pair(sql_feature_index, i)] =
          feature.bind_values(i).value();
    }
  }

  // Process the indexed custom inputs
  auto custom_input_processor =
      std::make_unique<CustomInputProcessor>(prediction_time_);
  auto* custom_input_processor_ptr = custom_input_processor.get();
  custom_input_processor_ptr->ProcessIndexType<SqlFeatureAndBindValueIndices>(
      std::move(bind_values), std::move(feature_processor_state_),
      base::BindOnce(&SqlFeatureProcessor::OnCustomInputProcessed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(custom_input_processor)));
}

// TODO(haileywang): Move this structure to ukm_types.
struct SqlFeatureProcessor::CustomSqlQuery {
  CustomSqlQuery() = default;
  ~CustomSqlQuery() = default;
  std::string query;
  std::vector<ProcessedValue> bind_values;
};

void SqlFeatureProcessor::OnCustomInputProcessed(
    std::unique_ptr<CustomInputProcessor> custom_input_processor,
    std::unique_ptr<FeatureProcessorState> feature_processor_state,
    base::flat_map<SqlFeatureAndBindValueIndices, Tensor> result) {
  // Validate the total number of bind values needed.
  size_t total_bind_values = 0;
  for (const auto& query : queries_) {
    const proto::SqlFeature& feature = query.second;
    total_bind_values += feature.bind_values_size();
  }

  if (total_bind_values != result.size()) {
    RunErrorCallback();
    return;
  }

  // Assemble the sql queries and the corresponding bind values.
  for (const auto& query : queries_) {
    const proto::SqlFeature& feature = query.second;
    FeatureIndex sql_feature_index = query.first;

    for (int i = 0; i < feature.bind_values_size(); ++i) {
      int bind_value_index = i;

      // Validate the result tensor.
      if (result.count(std::make_pair(sql_feature_index, bind_value_index)) !=
          1) {
        RunErrorCallback();
        return;
      }

      // Append query and query params to the list.
      const auto& custom_input_tensors =
          result[std::make_pair(sql_feature_index, bind_value_index)];
      CustomSqlQuery current;
      current.query = feature.sql();
      current.bind_values.insert(current.bind_values.end(),
                                 custom_input_tensors.begin(),
                                 custom_input_tensors.end());
      processed_queries_[sql_feature_index] = std::move(current);
    }
  }

  // TODO(haileywang): Custom inputs have been processed and sql queries are
  // ready to be sent to the ukm database.
}

void SqlFeatureProcessor::RunErrorCallback() {
  feature_processor_state_->SetError();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(feature_processor_state_),
                     std::move(result_)));
}

}  // namespace segmentation_platform

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/sql_feature_processor.h"
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/execution/processing/custom_input_processor.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform::processing {

SqlFeatureProcessor::SqlFeatureProcessor(
    QueryList&& queries,
    base::Time prediction_time,
    InputDelegateHolder* input_delegate_holder,
    UkmDatabase* ukm_database)
    : queries_(std::move(queries)),
      prediction_time_(prediction_time),
      input_delegate_holder_(input_delegate_holder),
      ukm_database_(ukm_database) {}
SqlFeatureProcessor::~SqlFeatureProcessor() = default;

void SqlFeatureProcessor::Process(
    FeatureProcessorState& feature_processor_state,
    QueryProcessorCallback callback) {
  DCHECK(!is_processed_);
  is_processed_ = true;
  callback_ = std::move(callback);

  // Prepare the sql queries for indexed custom inputs processing.
  base::flat_map<SqlFeatureAndBindValueIndices, proto::CustomInput> bind_values;
  for (const auto& query : queries_) {
    DCHECK(query.second.input_feature->has_sql_feature());
    const proto::SqlFeature& feature =
        query.second.input_feature->sql_feature();
    FeatureIndex sql_feature_index = query.first;

    // Validate the proto::SqlFeature metadata.
    if (metadata_utils::ValidateMetadataSqlFeature(feature) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      feature_processor_state.SetError(
          stats::FeatureProcessingError::kSqlValidationError);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_),

                                    std::move(result_)));
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
  auto custom_input_processor = std::make_unique<CustomInputProcessor>(
      prediction_time_, input_delegate_holder_);
  auto* custom_input_processor_ptr = custom_input_processor.get();
  custom_input_processor_ptr->ProcessIndexType<SqlFeatureAndBindValueIndices>(
      std::move(bind_values), feature_processor_state,
      std::make_unique<base::flat_map<std::pair<int, int>, Tensor>>(),
      base::BindOnce(&SqlFeatureProcessor::OnCustomInputProcessed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(custom_input_processor),
                     feature_processor_state.GetWeakPtr()));
}

void SqlFeatureProcessor::OnCustomInputProcessed(
    std::unique_ptr<CustomInputProcessor> custom_input_processor,
    base::WeakPtr<FeatureProcessorState> feature_processor_state,
    base::flat_map<SqlFeatureAndBindValueIndices, Tensor> result) {
  // Validate the total number of bind values needed.
  size_t total_bind_values = 0;
  for (const auto& query : queries_) {
    const proto::SqlFeature& feature =
        query.second.input_feature->sql_feature();
    total_bind_values += feature.bind_values_size();
  }

  if (total_bind_values != result.size()) {
    feature_processor_state->SetError(
        stats::FeatureProcessingError::kSqlBindValuesError);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
    return;
  }

  // Assemble the sql queries and the corresponding bind values.
  for (const auto& query : queries_) {
    const proto::SqlFeature& feature =
        query.second.input_feature->sql_feature();
    FeatureIndex sql_feature_index = query.first;
    UkmDatabase::CustomSqlQuery& current =
        processed_queries_[sql_feature_index];
    current.query = feature.sql();

    for (int i = 0; i < feature.bind_values_size(); ++i) {
      int bind_value_index = i;

      // Validate the result tensor.
      if (result.count(std::make_pair(sql_feature_index, bind_value_index)) !=
          1) {
        feature_processor_state->SetError(
            stats::FeatureProcessingError::kResultTensorError);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback_), std::move(result_)));
        return;
      }

      // Append query params to the list.
      const Tensor& custom_input_tensors =
          result[std::make_pair(sql_feature_index, bind_value_index)];
      current.bind_values.insert(current.bind_values.end(),
                                 custom_input_tensors.begin(),
                                 custom_input_tensors.end());
    }
  }

  // Send the queries to the ukm database to process.
  ukm_database_->RunReadOnlyQueries(
      std::move(processed_queries_),
      base::BindOnce(&SqlFeatureProcessor::OnQueriesRun,
                     weak_ptr_factory_.GetWeakPtr(), feature_processor_state));
}

void SqlFeatureProcessor::OnQueriesRun(
    base::WeakPtr<FeatureProcessorState> feature_processor_state,
    bool success,
    IndexedTensors result) {
  if (!success) {
    feature_processor_state->SetError(
        stats::FeatureProcessingError::kSqlQueryRunError);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result)));
}

}  // namespace segmentation_platform::processing

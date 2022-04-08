// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SQL_FEATURE_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SQL_FEATURE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/query_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {
class CustomInputProcessor;
class FeatureProcessorState;

// SqlFeatureProcessor takes a list of SqlFeature type of input, fetches samples
// from the UKMDatabase, and computes an input tensor to use when executing the
// ML model.
class SqlFeatureProcessor : public QueryProcessor {
 public:
  using QueryList = base::flat_map<FeatureIndex, proto::SqlFeature>;

  explicit SqlFeatureProcessor(QueryList&& queries, base::Time prediction_time);
  ~SqlFeatureProcessor() override;

  // QueryProcessor implementation.
  void Process(std::unique_ptr<FeatureProcessorState> feature_processor_state,
               QueryProcessorCallback callback) override;

 private:
  using SqlFeatureAndBindValueIndices =
      std::pair</*sql feature index*/ int, /*bind value index*/ int>;

  // Struct responsible for storing a sql query and its bind values.
  struct CustomSqlQuery;

  // Callback method for when all relevant bind values have been processed.
  void OnCustomInputProcessed(
      std::unique_ptr<CustomInputProcessor> custom_input_processor,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      base::flat_map<SqlFeatureAndBindValueIndices, Tensor> result);

  // Helper method for setting the error state and returning result to the
  // feature processor.
  void RunErrorCallback();

  // List of sql features to process into input tensors.
  QueryList queries_;

  // Time at which we expect the model execution to run.
  const base::Time prediction_time_;

  // Temporary storage of the processing state object.
  std::unique_ptr<FeatureProcessorState> feature_processor_state_;

  // Callback for sending the resulting indexed tensors to the feature list
  // processor.
  QueryProcessorCallback callback_;

  bool is_processed_{false};

  // List of sql queries and bind values ready to be sent to the ukm database
  // for processing.
  base::flat_map<FeatureIndex, CustomSqlQuery> processed_queries_;

  // List of resulting input tensors.
  IndexedTensors result_;

  base::WeakPtrFactory<SqlFeatureProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_SQL_FEATURE_PROCESSOR_H_

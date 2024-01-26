// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_SQL_FEATURE_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_SQL_FEATURE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform::processing {
class CustomInputProcessor;
class FeatureProcessorState;
class InputDelegateHolder;
struct Data;

// SqlFeatureProcessor takes a list of SqlFeature type of input, fetches samples
// from the UKMDatabase, and computes an input tensor to use when executing the
// ML model.
class SqlFeatureProcessor : public QueryProcessor {
 public:
  using QueryList = base::flat_map<FeatureIndex, Data>;

  SqlFeatureProcessor(QueryList&& queries,
                      base::Time prediction_time,
                      InputDelegateHolder* input_delegate_holder,
                      UkmDatabase* ukm_database);
  ~SqlFeatureProcessor() override;

  // QueryProcessor implementation.
  void Process(FeatureProcessorState& feature_processor_state,
               QueryProcessorCallback callback) override;

 private:
  using SqlFeatureAndBindValueIndices =
      std::pair</*sql feature index*/ int, /*bind value index*/ int>;

  // Callback method for when all relevant bind values have been processed.
  void OnCustomInputProcessed(
      std::unique_ptr<CustomInputProcessor> custom_input_processor,
      base::WeakPtr<FeatureProcessorState> feature_processor_state,
      base::flat_map<SqlFeatureAndBindValueIndices, Tensor> result);

  // Callback method for when all queries have been processed by the ukm
  // database.
  void OnQueriesRun(
      base::WeakPtr<FeatureProcessorState> feature_processor_state,
      bool success,
      IndexedTensors result);

  // List of sql features to process into input tensors.
  QueryList queries_;

  // Time at which we expect the model execution to run.
  const base::Time prediction_time_;

  const raw_ptr<InputDelegateHolder> input_delegate_holder_;

  // Main database for fetching data.
  const raw_ptr<UkmDatabase> ukm_database_;

  // Callback for sending the resulting indexed tensors to the feature list
  // processor.
  QueryProcessorCallback callback_;

  bool is_processed_{false};

  // List of sql queries and bind values ready to be sent to the ukm database
  // for processing.
  base::flat_map<FeatureIndex, UkmDatabase::CustomSqlQuery> processed_queries_;

  // List of resulting input tensors.
  IndexedTensors result_;

  base::WeakPtrFactory<SqlFeatureProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_SQL_FEATURE_PROCESSOR_H_

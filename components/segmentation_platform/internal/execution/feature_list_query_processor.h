// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_FEATURE_LIST_QUERY_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_FEATURE_LIST_QUERY_PROCESSOR_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/execution/custom_input_processor.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#include "components/segmentation_platform/internal/execution/query_processor.h"
#include "components/segmentation_platform/internal/execution/uma_feature_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {
class FeatureAggregator;
class FeatureProcessorState;
class SignalDatabase;

// FeatureListQueryProcessor takes a segmentation model's metadata, processes
// each feature in the metadata's feature list in order and computes an input
// tensor to use when executing the ML model.
class FeatureListQueryProcessor {
 public:
  FeatureListQueryProcessor(
      SignalDatabase* signal_database,
      std::unique_ptr<FeatureAggregator> feature_aggregator);
  virtual ~FeatureListQueryProcessor();

  // Disallow copy/assign.
  FeatureListQueryProcessor(const FeatureListQueryProcessor&) = delete;
  FeatureListQueryProcessor& operator=(const FeatureListQueryProcessor&) =
      delete;

  using FeatureProcessorCallback =
      base::OnceCallback<void(bool, const std::vector<float>&)>;

  // Given a model's metadata, processes the feature list from the metadata and
  // computes the input tensor for the ML model. Result is returned through a
  // callback.
  // |segment_id| is only used for recording performance metrics. This class
  // does not need to know about the segment itself. |prediction_time| is the
  // time at which we predict the model execution should happen.
  virtual void ProcessFeatureList(
      const proto::SegmentationModelMetadata& model_metadata,
      OptimizationTarget segment_id,
      base::Time prediction_time,
      FeatureProcessorCallback callback);

 private:
  // Called by ProcessFeatureList to process the next input feature in the list.
  // It then delegates the processing to the correct feature processor class
  // until the feature list is empty.
  void ProcessNextInputFeature(
      std::unique_ptr<FeatureProcessorState> feature_processor_state);

  // Callback called after a feature has been processed, indicating that we can
  // safely discard the feature processor that handled the processing. Continue
  // with the rest of the input features by calling ProcessNextInputFeature.
  void OnFeatureProcessed(
      std::unique_ptr<QueryProcessor> feature_processor,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      QueryProcessor::IndexedTensors result);

  // Signal database for uma features.
  const raw_ptr<SignalDatabase> signal_database_;

  // Feature aggregator that aggregates data for uma features.
  const std::unique_ptr<FeatureAggregator> feature_aggregator_;

  // Feature processor for uma type of input features.
  CustomInputProcessor custom_input_processor_;

  base::WeakPtrFactory<FeatureListQueryProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_FEATURE_LIST_QUERY_PROCESSOR_H_

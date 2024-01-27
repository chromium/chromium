// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_PROCESSOR_STATE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_PROCESSOR_STATE_H_

#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/processing/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"

namespace segmentation_platform::processing {

using proto::SegmentId;

// FeatureProcessorState is responsible for storing all necessary state during
// the processing of a model's metadata.
class FeatureProcessorState {
 public:
  FeatureProcessorState();
  FeatureProcessorState(
      FeatureProcessorStateId id,
      base::Time prediction_time,
      base::Time observation_time,
      base::TimeDelta bucket_duration,
      SegmentId segment_id,
      scoped_refptr<InputContext> input_context,
      FeatureListQueryProcessor::FeatureProcessorCallback callback);
  virtual ~FeatureProcessorState();

  // Disallow copy/assign.
  FeatureProcessorState(const FeatureProcessorState&) = delete;
  FeatureProcessorState& operator=(const FeatureProcessorState&) = delete;

  // Getters.
  FeatureProcessorStateId id() const { return id_; }

  base::TimeDelta bucket_duration() const { return bucket_duration_; }

  base::Time prediction_time() const { return prediction_time_; }

  base::Time observation_time() const { return observation_time_; }

  SegmentId segment_id() const { return segment_id_; }

  bool error() const { return error_; }

  const scoped_refptr<InputContext> input_context() const {
    return input_context_;
  }

  // Returns and pops the next feature processor.
  std::optional<std::pair<std::unique_ptr<QueryProcessor>, bool>>
  PopNextProcessor();

  // Add a processor to the list of processors waiting for processing.
  // TODO(haileywang): Send Data::DataType instead of bool.
  void AppendProcessor(std::unique_ptr<QueryProcessor> processor,
                       bool is_input);

  // Temporarily store indexed tensor results.
  void AppendIndexedTensors(const QueryProcessor::IndexedTensors& result,
                            bool is_input);

  // Format tensors and run callback.
  void OnFinishProcessing();

  // Sets an error to the current feature processor state.
  void SetError(stats::FeatureProcessingError error,
                const std::string& message = {});

  base::WeakPtr<FeatureProcessorState> GetWeakPtr();

  // For testing only.
  void set_input_context_for_testing(
      scoped_refptr<InputContext> input_context) {
    input_context_ = input_context;
  }

 private:
  // Format all indexed tensor results into final ordered tensor vector.
  std::vector<float> MergeTensors(const QueryProcessor::IndexedTensors& tensor);

  // ID generation for feature processor state.
  const FeatureProcessorStateId id_;

  const base::Time prediction_time_;
  const base::Time observation_time_;
  const base::TimeDelta bucket_duration_;
  const SegmentId segment_id_;
  scoped_refptr<InputContext> input_context_;
  std::deque<std::unique_ptr<QueryProcessor>> in_processors_;
  std::deque<std::unique_ptr<QueryProcessor>> out_processors_;

  // Feature processing results.
  QueryProcessor::IndexedTensors input_tensor_;
  QueryProcessor::IndexedTensors output_tensor_;
  bool error_{false};

  // Callback to return feature processing results to model execution manager.
  FeatureListQueryProcessor::FeatureProcessorCallback callback_;

  base::WeakPtrFactory<FeatureProcessorState> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_FEATURE_PROCESSOR_STATE_H_

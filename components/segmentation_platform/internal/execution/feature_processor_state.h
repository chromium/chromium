// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_FEATURE_PROCESSOR_STATE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_FEATURE_PROCESSOR_STATE_H_

#include <deque>
#include <vector>

#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {

// FeatureProcessorState is responsible for storing all necessary state during
// the processing of a model's metadata.
class FeatureProcessorState {
 public:
  FeatureProcessorState(
      base::Time prediction_time,
      base::TimeDelta bucket_duration,
      OptimizationTarget segment_id,
      std::unique_ptr<std::deque<proto::InputFeature>> input_features,
      FeatureListQueryProcessor::FeatureProcessorCallback callback);
  virtual ~FeatureProcessorState();

  // Disallow copy/assign.
  FeatureProcessorState(const FeatureProcessorState&) = delete;
  FeatureProcessorState& operator=(const FeatureProcessorState&) = delete;

  // Getters.
  base::TimeDelta bucket_duration() const { return bucket_duration_; }

  base::Time prediction_time() const { return prediction_time_; }

  OptimizationTarget segment_id() const { return segment_id_; }

  bool error() const { return error_; }

  // Returns and pops the next input feature in the feature list.
  proto::InputFeature PopNextInputFeature();

  // Sets an error to the current feature processor state.
  // TODO(haileywang): Pass in a reason for error enum here and record an enum
  // histogram of feature failure reasons.
  void SetError();

  // Returns whether the input feature list is empty.
  bool IsFeatureListEmpty() const;

  // Run the callback stored in the current feature processor state.
  void RunCallback();

  // Update the input tensor vector.
  void AppendInputTensor(const std::vector<float>& data);
  void AppendInputTensor(const std::vector<ProcessedValue>& data);

 private:
  const base::Time prediction_time_;
  const base::TimeDelta bucket_duration_;
  const OptimizationTarget segment_id_;
  std::unique_ptr<std::deque<proto::InputFeature>> input_features_;

  // Feature processing results.
  std::vector<float> input_tensor_;
  bool error_{false};

  // Callback to return feature processing results to model execution manager.
  FeatureListQueryProcessor::FeatureProcessorCallback callback_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_FEATURE_PROCESSOR_STATE_H_

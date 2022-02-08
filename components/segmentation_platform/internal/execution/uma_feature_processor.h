// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_UMA_FEATURE_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_UMA_FEATURE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {
class FeatureProcessorState;

// UmaFeatureProcessor takes an UMAFeature type of input, fetches samples from
// the SignalDatabase (raw signals) databases, and computes an input tensor to
// use when executing the ML model.
class UmaFeatureProcessor {
 public:
  UmaFeatureProcessor(SignalDatabase* signal_database,
                      std::unique_ptr<FeatureAggregator> feature_aggregator);
  virtual ~UmaFeatureProcessor();

  // Disallow copy/assign.
  UmaFeatureProcessor(const UmaFeatureProcessor&) = delete;
  UmaFeatureProcessor& operator=(const UmaFeatureProcessor&) = delete;

  using FeatureListQueryProcessorCallback =
      base::OnceCallback<void(std::unique_ptr<FeatureProcessorState>)>;

  // Function for processing the next UMAFeature type of input for ML model.
  void ProcessUmaFeature(
      const proto::UMAFeature& feature,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      FeatureListQueryProcessorCallback callback);

 private:
  // Callback method for when all relevant samples for a particular feature has
  // been loaded. Processes the samples, and inserts them into the input tensor
  // that is later given to the ML execution.
  void OnGetSamplesForUmaFeature(
      FeatureListQueryProcessorCallback callback,
      std::unique_ptr<FeatureProcessorState> feature_processor_state,
      const proto::UMAFeature& feature,
      const std::vector<int32_t>& accepted_enum_ids,
      std::vector<SignalDatabase::Sample> samples);

  // Main signal database for user actions and histograms.
  raw_ptr<SignalDatabase> signal_database_;

  // The FeatureAggregator aggregates all the data based on metadata and input.
  std::unique_ptr<FeatureAggregator> feature_aggregator_;

  base::WeakPtrFactory<UmaFeatureProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_UMA_FEATURE_PROCESSOR_H_

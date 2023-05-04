// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_UMA_FEATURE_PROCESSOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_UMA_FEATURE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/execution/processing/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform::processing {
class FeatureProcessorState;

// A query processor that takes a list of UMAFeatures, fetches samples from the
// SignalDatabase and computes an input tensor to be used for ML model
// execution.
class UmaFeatureProcessor : public QueryProcessor {
 public:
  UmaFeatureProcessor(base::flat_map<FeatureIndex, Data>&& uma_features,
                      SignalDatabase* signal_database,
                      FeatureAggregator* feature_aggregator,
                      const base::Time prediction_time,
                      const base::Time observation_time,
                      const base::TimeDelta bucket_duration,
                      const proto::SegmentId segment_id,
                      bool is_output);

  ~UmaFeatureProcessor() override;

  using FeatureListQueryProcessorCallback =
      base::OnceCallback<void(std::unique_ptr<FeatureProcessorState>)>;

  // QueryProcessor implementation.
  void Process(std::unique_ptr<FeatureProcessorState> feature_processor_state,
               QueryProcessorCallback callback) override;

 private:
  // Function for processing the next UMAFeature type of input for ML model.
  void ProcessNextUmaFeature();

  // Helper function for parsing a single uma feature.
  void ProcessSingleUmaFeature(FeatureIndex index,
                               const proto::UMAFeature& feature);

  // Callback method for when all relevant samples for a particular feature has
  // been loaded. Processes the samples, and inserts them into the input tensor
  // that is later given to the ML execution.
  void OnGetSamplesForUmaFeature(FeatureIndex index,
                                 const proto::UMAFeature& feature,
                                 const std::vector<int32_t>& accepted_enum_ids,
                                 const base::Time end_time,
                                 std::vector<SignalDatabase::Sample> samples);

  // List of custom inputs to process into input tensors.
  base::flat_map<FeatureIndex, Data> uma_features_;

  // Main signal database for user actions and histograms.
  // This dangling raw_ptr occurred in:
  // browser_tests: SegmentationPlatformTest.RunDefaultModel (flaky)
  // https://ci.chromium.org/ui/p/chromium/builders/try/win-rel/175245/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Abrowser_tests%2FSegmentationPlatformTest.RunDefaultModel+VHash%3Abdbee181b3e0309b
  // This also occurs while checking for dangling ptrs at exit.
  const raw_ptr<SignalDatabase, DanglingUntriaged> signal_database_;

  // The FeatureAggregator aggregates all the data based on metadata and input.
  const raw_ptr<FeatureAggregator, FlakyDanglingUntriaged> feature_aggregator_;

  // Data needed for the processing of uma features.
  const base::Time prediction_time_;
  const base::Time observation_time_;
  const base::TimeDelta bucket_duration_;
  const proto::SegmentId segment_id_;
  const bool is_output_;

  // Temporary storage of the processing state object.
  // TODO(haileywang): Remove dependency to the state object once error check is
  // no longer part of the state.
  std::unique_ptr<FeatureProcessorState> feature_processor_state_;

  // Callback for sending the resulting indexed tensors to the feature list
  // processor.
  QueryProcessorCallback callback_;

  // List of resulting input tensors.
  IndexedTensors result_;

  base::WeakPtrFactory<UmaFeatureProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_UMA_FEATURE_PROCESSOR_H_

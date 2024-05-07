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
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/execution/processing/feature_aggregator.h"
#include "components/segmentation_platform/internal/execution/processing/query_processor.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform::processing {
class FeatureProcessorState;
struct Data;

// A query processor that takes a list of UMAFeatures, fetches UMA histogram and
// user action samples from database and computes a result tensor.
class UmaFeatureProcessor : public QueryProcessor {
 public:
  UmaFeatureProcessor(base::flat_map<FeatureIndex, Data>&& uma_features,
                      StorageService* storage_service,
                      const std::string& profile_id,
                      FeatureAggregator* feature_aggregator,
                      const base::Time prediction_time,
                      const base::Time observation_time,
                      const base::TimeDelta bucket_duration,
                      const proto::SegmentId segment_id,
                      bool is_output);

  ~UmaFeatureProcessor() override;

  // QueryProcessor implementation.
  void Process(FeatureProcessorState& feature_processor_state,
               QueryProcessorCallback callback) override;

 private:
  void ProcessNextFeature();
  void OnGetSamplesForUmaFeature(FeatureIndex index,
                                 const proto::UMAFeature& feature,
                                 const base::Time end_time,
                                 std::vector<SignalDatabase::DbEntry> samples);

  void ProcessUsingSqlDatabase(FeatureProcessorState& feature_processor_state);
  void OnSqlQueriesRun(bool success, processing::IndexedTensors tensor);

  // Function for processing the next UMAFeature type of input for ML model.
  void ProcessOnGotAllSamples(
      FeatureProcessorState& feature_processor_state,
      const std::vector<SignalDatabase::DbEntry>& samples);

  void GetStartAndEndTime(size_t bucket_count,
                          base::Time& start_time,
                          base::Time& end_time) const;

  // Helper function for parsing a single uma feature.
  void ProcessSingleUmaFeature(
      const std::vector<SignalDatabase::DbEntry>& samples,
      FeatureIndex index,
      const proto::UMAFeature& feature);

  SignalDatabase* GetSignalDatabase();

  UkmDatabase* GetUkmDatabase();

  // List of custom inputs to process into input tensors.
  base::flat_map<FeatureIndex, Data> uma_features_;

  // Storage service to get user actions and histograms.
  base::WeakPtr<StorageService> weak_storage_service_;

  // The profile ID of current profile, required to query the `ukm_database_`.
  const std::string profile_id_;

  // The FeatureAggregator aggregates all the data based on metadata and input.
  const raw_ptr<FeatureAggregator, FlakyDanglingUntriaged> feature_aggregator_;

  // Data needed for the processing of uma features.
  const base::Time prediction_time_;
  const base::Time observation_time_;
  const base::TimeDelta bucket_duration_;
  const proto::SegmentId segment_id_;
  const bool is_output_;
  const bool is_batch_processing_enabled_;
  const bool use_sql_database_;

  // Callback for sending the resulting indexed tensors to the feature list
  // processor.
  QueryProcessorCallback callback_;

  // List of resulting input tensors.
  IndexedTensors result_;

  base::WeakPtrFactory<UmaFeatureProcessor> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_PROCESSING_UMA_FEATURE_PROCESSOR_H_

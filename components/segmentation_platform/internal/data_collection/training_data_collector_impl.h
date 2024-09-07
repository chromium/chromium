// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"

namespace segmentation_platform {
using proto::ModelSource;
using proto::SegmentId;

class SegmentationResultPrefs;

// Implementation of TrainingDataCollector.
class TrainingDataCollectorImpl : public TrainingDataCollector,
                                  public HistogramSignalHandler::Observer,
                                  public UserActionSignalHandler::Observer {
 public:
  TrainingDataCollectorImpl(const PlatformOptions& platform_options,
                            processing::FeatureListQueryProcessor* processor,
                            HistogramSignalHandler* histogram_signal_handler,
                            UserActionSignalHandler* user_action_signal_handler,
                            StorageService* storage_service,
                            PrefService* profile_prefs,
                            base::Clock* clock,
                            CachedResultProvider* cached_result_provider);
  ~TrainingDataCollectorImpl() override;

  // TrainingDataCollector implementation.
  void OnModelMetadataUpdated() override;
  void OnServiceInitialized() override;
  void ReportCollectedContinuousTrainingData() override;
  TrainingRequestId OnDecisionTime(
      proto::SegmentId id,
      scoped_refptr<InputContext> input_context,
      DecisionType type,
      std::optional<ModelProvider::Request> inputs,
      bool decision_result_update_trigger = false) override;
  void CollectTrainingData(SegmentId segment_id,
                           TrainingRequestId request_id,
                           ukm::SourceId ukm_source_id,
                           const TrainingLabels& param,
                           SuccessCallback callback) override;

  // HistogramSignalHandler::Observer implementation.
  void OnHistogramSignalUpdated(const std::string& histogram_name,
                                base::HistogramBase::Sample sample) override;

  // UserActionSignalHandler::Observer implementation.
  void OnUserAction(const std::string& user_action,
                    base::TimeTicks action_time) override;

  void SetSamplingRateForTesting(uint64_t sampling_rate);

 private:
  struct TrainingTimings;

  // Called when a relevant uma histogram is recorded, when a time delay
  // trigger is hit or data collection is manually triggered, retrieve input
  // training data from storage, collect output training data and upload all
  // training data.
  void OnObservationTrigger(
      const std::optional<ImmediateCollectionParam>& param,
      TrainingRequestId request_id,
      const proto::SegmentInfo& segment_info,
      SuccessCallback callback);

  void OnGetSegmentsInfoList(
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_list);

  void ReportForSegmentsInfoList(
      const std::optional<ImmediateCollectionParam>& param,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments);

  void OnUmaUpdatedReportForSegmentInfo(
      const std::optional<ImmediateCollectionParam>& param,
      const proto::SegmentInfo* segment);

  void OnGetSegmentInfoAtDecisionTime(
      proto::SegmentId segment_id,
      TrainingRequestId request_id,
      DecisionType type,
      scoped_refptr<InputContext> input_context,
      const proto::SegmentInfo& segment_info,
      std::optional<ModelProvider::Request> inputs);

  void OnGetTrainingTensorsAtDecisionTime(
      TrainingRequestId request_id,
      const TrainingTimings& training_request,
      const proto::SegmentInfo& segment_info,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

  void OnGetStoredTrainingData(
      const std::optional<ImmediateCollectionParam>& param,
      const proto::SegmentInfo& segment_info,
      SuccessCallback callback,
      std::optional<proto::TrainingData> input);

  void OnGetOutputsOnObservationTrigger(
      const std::optional<ImmediateCollectionParam>& param,
      const proto::SegmentInfo& segment_info,
      const ModelProvider::Request& cached_input_tensors,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

  void OnGetTrainingTensors(
      const std::optional<ImmediateCollectionParam>& param,
      const proto::SegmentInfo& segment_info,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

  void PostObservationTask(TrainingRequestId request_id,
                           const proto::SegmentInfo& segment_info,
                           const base::TimeDelta& delay,
                           stats::TrainingDataCollectionEvent event);

  // Returns whether training data can be reported through UKM. If
  // |include_output| is false, only input data will be checked to see if they
  // meet the collection requirement.
  bool CanReportTrainingData(const proto::SegmentInfo& segment_info,
                             bool include_output);

  TrainingTimings ComputeDecisionTiming(const proto::SegmentInfo& info) const;
  base::Time ComputeObservationTiming(const proto::SegmentInfo& info,
                                      base::Time prediction_time) const;

  // Returns whether to store the training data to disk.
  bool FillTrainingData(TrainingRequestId request_id,
                        const TrainingTimings& training_request,
                        const ModelProvider::Request& input_tensors,
                        const proto::SegmentInfo& segment_info,
                        proto::TrainingData& training_data);

  const PlatformOptions platform_options_;
  const raw_ptr<SegmentInfoDatabase, DanglingUntriaged> segment_info_database_;
  const raw_ptr<processing::FeatureListQueryProcessor>
      feature_list_query_processor_;
  const raw_ptr<HistogramSignalHandler> histogram_signal_handler_;
  const raw_ptr<UserActionSignalHandler> user_action_signal_handler_;
  const raw_ptr<SignalStorageConfig, DanglingUntriaged> signal_storage_config_;
  const raw_ptr<const ConfigHolder, DanglingUntriaged> config_holder_;
  const raw_ptr<base::Clock> clock_;

  // Helper class to read/write results to the prefs.
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;

  const raw_ptr<CachedResultProvider, DanglingUntriaged>
      cached_result_provider_;

  // Cache class to temporarily store training data in the observation period.
  std::unique_ptr<TrainingDataCache> training_cache_;

  // Hash of histograms and their corresponding accepted enum ids for trigger
  // based training data collection.
  base::flat_map<uint64_t,
                 base::flat_set<std::pair<std::pair<SegmentId, ModelSource>,
                                          std::vector<int>>>>
      immediate_trigger_histograms_;

  // Hash of user actions for trigger based training data collection.
  base::flat_map<uint64_t, base::flat_set<std::pair<SegmentId, ModelSource>>>
      immediate_trigger_user_actions_;

  // A list of segment IDs that needs to report metrics continuously.
  base::flat_set<SegmentId> continuous_collection_segments_;

  // List of all segments that need to upload training data.
  // TODO(ssid): Clean up the list of segment IDs in this class to be a single
  // list.
  base::flat_map<SegmentId, ModelSource> all_segments_for_training_;

  uint64_t time_trigger_sampling_rate_{0};

  base::WeakPtrFactory<TrainingDataCollectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_

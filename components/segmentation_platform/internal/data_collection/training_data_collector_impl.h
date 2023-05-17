// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_

#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/config_holder.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
using proto::SegmentId;

class SegmentationResultPrefs;

// Implementation of TrainingDataCollector.
class TrainingDataCollectorImpl : public TrainingDataCollector,
                                  public HistogramSignalHandler::Observer,
                                  public UserActionSignalHandler::Observer {
 public:
  TrainingDataCollectorImpl(processing::FeatureListQueryProcessor* processor,
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
  TrainingRequestId OnDecisionTime(proto::SegmentId id,
                                   scoped_refptr<InputContext> input_context,
                                   DecisionType type) override;
  void CollectTrainingData(SegmentId segment_id,
                           TrainingRequestId request_id,
                           const TrainingLabels& param,
                           SuccessCallback callback) override;

  // HistogramSignalHandler::Observer implementation.
  void OnHistogramSignalUpdated(const std::string& histogram_name,
                                base::HistogramBase::Sample sample) override;

  // UserActionSignalHandler::Observer implementation.
  void OnUserAction(const std::string& user_action,
                    base::TimeTicks action_time) override;

 private:
  struct TrainingTimings;

  // Called when a relevant uma histogram is recorded, when a time delay
  // trigger is hit or data collection is manually triggered, retrieve input
  // training data from storage, collect output training data and upload all
  // training data.
  void OnObservationTrigger(
      const absl::optional<ImmediateCollectionParam>& param,
      TrainingRequestId request_id,
      const proto::SegmentInfo& segment_info,
      SuccessCallback callback);

  void OnGetSegmentsInfoList(DefaultModelManager::SegmentInfoList segment_list);

  void ReportForSegmentsInfoList(
      const absl::optional<ImmediateCollectionParam>& param,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments);

  void OnUmaUpdatedReportForSegmentInfo(
      const absl::optional<ImmediateCollectionParam>& param,
      absl::optional<proto::SegmentInfo> segment);

  void OnGetSegmentInfoAtDecisionTime(
      proto::SegmentId segment_id,
      TrainingRequestId request_id,
      DecisionType type,
      scoped_refptr<InputContext> input_context,
      DefaultModelManager::SegmentInfoList segment_list);

  void OnGetTrainingTensorsAtDecisionTime(
      TrainingRequestId request_id,
      const TrainingTimings& training_request,
      const proto::SegmentInfo& segment_info,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

  void OnGetStoredTrainingData(
      const absl::optional<ImmediateCollectionParam>& param,
      const proto::SegmentInfo& segment_info,
      SuccessCallback callback,
      absl::optional<proto::TrainingData> input);

  void OnGetOutputsOnObservationTrigger(
      const absl::optional<ImmediateCollectionParam>& param,
      const proto::SegmentInfo& segment_info,
      const ModelProvider::Request& cached_input_tensors,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

  void OnGetTrainingTensors(
      const absl::optional<ImmediateCollectionParam>& param,
      const proto::SegmentInfo& segment_info,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

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

  const raw_ptr<SegmentInfoDatabase> segment_info_database_;
  const raw_ptr<processing::FeatureListQueryProcessor>
      feature_list_query_processor_;
  const raw_ptr<HistogramSignalHandler> histogram_signal_handler_;
  const raw_ptr<UserActionSignalHandler> user_action_signal_handler_;
  const raw_ptr<SignalStorageConfig> signal_storage_config_;
  const raw_ptr<const ConfigHolder> config_holder_;
  const raw_ptr<base::Clock> clock_;

  // Helper class to read/write results to the prefs.
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;

  const raw_ptr<CachedResultProvider> cached_result_provider_;

  // Cache class to temporarily store training data in the observation period.
  std::unique_ptr<TrainingDataCache> training_cache_;

  // Class to get segment info from default models.
  const raw_ptr<DefaultModelManager> default_model_manager_;

  // Hash of histograms for immediate training data collection. When any
  // histogram hash contained in the map is recorded, a UKM message is reported
  // right away.
  base::flat_map<uint64_t, base::flat_set<proto::SegmentId>>
      immediate_collection_histograms_;

  // Hash of histograms for trigger based training data collection.
  base::flat_map<uint64_t, base::flat_set<proto::SegmentId>>
      immediate_trigger_histograms_;

  // Hash of user actions for trigger based training data collection.
  base::flat_map<uint64_t, base::flat_set<proto::SegmentId>>
      immediate_trigger_user_actions_;

  // A list of segment IDs that needs to report metrics continuously.
  base::flat_set<SegmentId> continuous_collection_segments_;

  // List of all segments that need to upload training data.
  // TODO(ssid): Clean up the list of segment IDs in this class to be a single
  // list.
  base::flat_set<SegmentId> all_segments_for_training_;

  base::WeakPtrFactory<TrainingDataCollectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_

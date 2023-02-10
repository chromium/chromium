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
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
using proto::SegmentId;

struct Config;
class SegmentationResultPrefs;

// Implementation of TrainingDataCollector.
class TrainingDataCollectorImpl : public TrainingDataCollector,
                                  public HistogramSignalHandler::Observer {
 public:
  TrainingDataCollectorImpl(processing::FeatureListQueryProcessor* processor,
                            HistogramSignalHandler* histogram_signal_handler,
                            StorageService* storage_service,
                            std::vector<std::unique_ptr<Config>>* configs,
                            PrefService* profile_prefs,
                            base::Clock* clock);
  ~TrainingDataCollectorImpl() override;

  // TrainingDataCollector implementation.
  void OnModelMetadataUpdated() override;
  void OnServiceInitialized() override;
  void ReportCollectedContinuousTrainingData() override;
  void OnDecisionTime(proto::SegmentId id,
                      scoped_refptr<InputContext> input_context,
                      DecisionType type) override;

  void OnObservationTrigger(const absl::optional<ImmediaCollectionParam>& param,
                            TrainingDataCache::RequestId request_id,
                            const proto::SegmentInfo& segment_info) override;

  // HistogramSignalHandler::Observer implementation.
  void OnHistogramSignalUpdated(const std::string& histogram_name,
                                base::HistogramBase::Sample sample) override;

 private:
  struct TrainingTimings;

  void OnGetSegmentsInfoList(DefaultModelManager::SegmentInfoList segment_list);

  void ReportForSegmentsInfoList(
      const absl::optional<ImmediaCollectionParam>& param,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments);

  void OnHistogramUpdatedReportForSegmentInfo(
      const absl::optional<ImmediaCollectionParam>& param,
      absl::optional<proto::SegmentInfo> segment);

  void OnGetSegmentInfoAtDecisionTime(
      proto::SegmentId segment_id,
      TrainingDataCache::RequestId request_id,
      DecisionType type,
      scoped_refptr<InputContext> input_context,
      DefaultModelManager::SegmentInfoList segment_list);

  void OnGetTrainingTensorsAtDecisionTime(
      TrainingDataCache::RequestId request_id,
      const TrainingTimings& training_request,
      const proto::SegmentInfo& segment_info,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

  void OnGetOutputsOnObservationTrigger(
      const absl::optional<ImmediaCollectionParam>& param,
      TrainingDataCache::RequestId request_id,
      const proto::SegmentInfo& segment_info,
      const ModelProvider::Request& cached_input_tensors,
      bool has_error,
      const ModelProvider::Request& input_tensors,
      const ModelProvider::Response& output_tensors);

  void OnGetTrainingTensors(const absl::optional<ImmediaCollectionParam>& param,
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

  const raw_ptr<SegmentInfoDatabase> segment_info_database_;
  const raw_ptr<processing::FeatureListQueryProcessor>
      feature_list_query_processor_;
  const raw_ptr<HistogramSignalHandler> histogram_signal_handler_;
  const raw_ptr<SignalStorageConfig> signal_storage_config_;
  const raw_ptr<std::vector<std::unique_ptr<Config>>> configs_;
  const raw_ptr<base::Clock> clock_;

  // Helper class to read/write results to the prefs.
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;

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

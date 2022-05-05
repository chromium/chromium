// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {
struct Config;
class SegmentationResultPrefs;

// Implementation of TrainingDataCollector.
class TrainingDataCollectorImpl : public TrainingDataCollector,
                                  public HistogramSignalHandler::Observer {
 public:
  TrainingDataCollectorImpl(SegmentInfoDatabase* segment_info_database,
                            processing::FeatureListQueryProcessor* processor,
                            HistogramSignalHandler* histogram_signal_handler,
                            SignalStorageConfig* signal_storage_config,
                            std::vector<std::unique_ptr<Config>>* configs,
                            PrefService* profile_prefs,
                            base::Clock* clock);
  ~TrainingDataCollectorImpl() override;

  // TrainingDataCollector implementation.
  void OnModelMetadataUpdated() override;
  void OnServiceInitialized() override;
  void ReportCollectedContinuousTrainingData() override;

  // HistogramSignalHandler::Observer implementation.
  void OnHistogramSignalUpdated(const std::string& histogram_name,
                                base::HistogramBase::Sample sample) override;

 private:
  // Parameters used for reporting immediate output collections.
  struct ImmediaCollectionParam {
    uint64_t output_metric_hash;  // Hash of the output metric name.
    int output_index;             // Index of the output metric in metadata.
    float output_value;           // Value of the output.
  };

  void OnGetSegmentsInfoList(
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments);

  void ReportForSegmentsInfoList(
      const absl::optional<ImmediaCollectionParam>& param,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments);

  void OnGetTrainingTensors(const absl::optional<ImmediaCollectionParam>& param,
                            const proto::SegmentInfo& segment_info,
                            bool success,
                            const std::vector<float>& input_tensors,
                            const std::vector<float>& output_tensors);

  // Returns whether training data can be reported through UKM. If
  // |include_output| is false, only input data will be checked to see if they
  // meet the collection requirement.
  bool CanReportTrainingData(const proto::SegmentInfo& segment_info,
                             bool include_output);

  raw_ptr<SegmentInfoDatabase> segment_info_database_;
  raw_ptr<processing::FeatureListQueryProcessor> feature_list_query_processor_;
  raw_ptr<HistogramSignalHandler> histogram_signal_handler_;
  raw_ptr<SignalStorageConfig> signal_storage_config_;
  raw_ptr<std::vector<std::unique_ptr<Config>>> configs_;
  raw_ptr<base::Clock> clock_;

  // Helper class to read/write results to the prefs.
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;

  // Hash of histograms for immediate training data collection. When any
  // histogram hash contained in the map is recorded, a UKM message is reported
  // right away.
  base::flat_map<uint64_t,
                 base::flat_set<optimization_guide::proto::OptimizationTarget>>
      immediate_collection_histograms_;

  // A list of segment IDs that needs to report metrics continuously.
  std::set<OptimizationTarget> continuous_collection_segments_;

  base::WeakPtrFactory<TrainingDataCollectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_

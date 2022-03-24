// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"

namespace segmentation_platform {

// Implementation of TrainingDataCollector.
class TrainingDataCollectorImpl : public TrainingDataCollector,
                                  public HistogramSignalHandler::Observer {
 public:
  TrainingDataCollectorImpl(SegmentInfoDatabase* segment_info_database,
                            FeatureListQueryProcessor* processor,
                            HistogramSignalHandler* histogram_signal_handler,
                            SignalStorageConfig* signal_storage_config,
                            base::Clock* clock);
  ~TrainingDataCollectorImpl() override;

  // TrainingDataCollector implementation.
  void OnModelMetadataUpdated() override;
  void OnServiceInitialized() override;

  // HistogramSignalHandler::Observer implementation.
  void OnHistogramSignalUpdated(const std::string& histogram_name,
                                base::HistogramBase::Sample sample) override;

 private:
  void OnGetSegmentsInfoList(
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments);

  void ReportForSegmentsInfoList(
      uint64_t output_metric_hash,
      base::HistogramBase::Sample output_metric_sample,
      std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segments);

  void OnGetInputTensor(float output_value,
                        int output_index,
                        OptimizationTarget segment_id,
                        int64_t model_version,
                        bool success,
                        const std::vector<float>& inputs);

  bool CanReportImmediateTrainingData(const proto::SegmentInfo& segment_info);

  raw_ptr<SegmentInfoDatabase> segment_info_database_;
  raw_ptr<FeatureListQueryProcessor> feature_list_query_processor_;
  raw_ptr<HistogramSignalHandler> histogram_signal_handler_;
  raw_ptr<SignalStorageConfig> signal_storage_config_;
  raw_ptr<base::Clock> clock_;

  // Hash of histograms for immediate training data collection. When any
  // histogram hash contained in the map is recorded, a UKM message is reported
  // right away.
  base::flat_map<uint64_t,
                 base::flat_set<optimization_guide::proto::OptimizationTarget>>
      immediate_collection_histograms_;

  base::WeakPtrFactory<TrainingDataCollectorImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_IMPL_H_

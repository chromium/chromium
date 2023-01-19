// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_

#include <memory>

#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/execution/default_model_manager.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {
using DecisionType = proto::TrainingOutputs::TriggerConfig::DecisionType;

namespace processing {
class FeatureListQueryProcessor;
}

struct Config;
class HistogramSignalHandler;

// Collect training data and report as Ukm message. Live on main thread.
// TODO(ssid): Make a new class that owns the training data collector and
// model execution collector.
class TrainingDataCollector {
 public:
  static std::unique_ptr<TrainingDataCollector> Create(
      processing::FeatureListQueryProcessor* processor,
      HistogramSignalHandler* histogram_signal_handler,
      StorageService* storage_service,
      std::vector<std::unique_ptr<Config>>* configs,
      PrefService* profile_prefs,
      base::Clock* clock);

  // Parameters used for reporting immediate output collections.
  struct ImmediaCollectionParam {
    // Hash of the output metric name given by
    // |base::HashMetricName(histogram_name)| function.
    uint64_t output_metric_hash;
    // Value of the output metric.
    // TODO(haileywang): Make this a vector and append all the values to the
    // output.
    float output_value;
  };

  // Called when model metadata is updated. May result in training data
  // collection behavior change.
  virtual void OnModelMetadataUpdated() = 0;

  // Called after segmentation platform is initialized. May report training data
  // to Ukm for |UMAOutput| in |SegmentationModelMetadata|.
  virtual void OnServiceInitialized() = 0;

  // Called by the DataCollectionScheduler to upload all the training data
  // collected. This will only upload tensors that require continuous
  // collection.
  virtual void ReportCollectedContinuousTrainingData() = 0;

  // Called to collect and store training input data. The data will only be
  // uploaded once |OnObservationTrigger| is triggered.
  virtual void OnDecisionTime(proto::SegmentId id,
                              scoped_refptr<InputContext> input_context,
                              DecisionType type) = 0;

  // Called when a relevant uma histogram is recorded or when a time delay
  // trigger is hit, retrieve input training data from storage, collect output
  // training data and upload all training data.
  virtual void OnObservationTrigger(
      const absl::optional<ImmediaCollectionParam>& param,
      TrainingDataCache::RequestId request_id,
      const proto::SegmentInfo& segment_info) = 0;

  virtual ~TrainingDataCollector();

  // Disallow copy/assign.
  TrainingDataCollector(const TrainingDataCollector&) = delete;
  TrainingDataCollector& operator=(const TrainingDataCollector&) = delete;

 protected:
  TrainingDataCollector();
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_

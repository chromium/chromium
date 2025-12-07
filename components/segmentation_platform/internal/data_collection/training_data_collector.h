// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_

#include <memory>

#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"
#include "components/segmentation_platform/internal/signals/user_action_signal_handler.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {
using DecisionType = proto::TrainingOutputs::TriggerConfig::DecisionType;

namespace processing {
class FeatureListQueryProcessor;
}

class HistogramSignalHandler;

// Collect training data and report as Ukm message. Live on main thread.
// TODO(ssid): Make a new class that owns the training data collector and
// model execution collector.
class TrainingDataCollector {
 public:
  using SuccessCallback = SegmentationPlatformService::SuccessCallback;
  // Name and sample of the uma output metric to be collected as training data.
  static std::unique_ptr<TrainingDataCollector> Create(
      const PlatformOptions& platform_options,
      processing::FeatureListQueryProcessor* processor,
      HistogramSignalHandler* histogram_signal_handler,
      UserActionSignalHandler* user_action_signal_handler,
      StorageService* storage_service,
      PrefService* profile_prefs,
      base::Clock* clock,
      CachedResultProvider* cached_result_provider);

  // Parameters used for reporting immediate output collections.
  struct ImmediateCollectionParam {
    // Optional name used for debugging.
    std::string output_metric_name;
    // Hash of the output metric name given by
    // |base::HashMetricName(name)| function.
    uint64_t output_metric_hash;
    // Value of the output metric.
    // TODO(haileywang): Make this a vector and append all the values to the
    // output.
    float output_value;

    // Optional source ID to record UKM with.
    ukm::SourceId ukm_source_id;
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
  // uploaded once |OnObservationTrigger| is triggered. |TrainingRequestId| can
  // be used to trigger observation for a specific set of training data. If
  // `decision_result_update_trigger` is true, then collect data only when
  // exact_prediction_time is set for the config.
  virtual TrainingRequestId OnDecisionTime(
      proto::SegmentId id,
      scoped_refptr<InputContext> input_context,
      DecisionType type,
      std::optional<ModelProvider::Request> inputs,
      bool decision_result_update_trigger = false) = 0;

  // Called by Segmentation Platform when manually triggering data collection on
  // the client.
  virtual void CollectTrainingData(SegmentId segment_id,
                                   TrainingRequestId request_id,
                                   ukm::SourceId ukm_source_id,
                                   const TrainingLabels& param,
                                   SuccessCallback callback) = 0;

  virtual ~TrainingDataCollector();

  // Disallow copy/assign.
  TrainingDataCollector(const TrainingDataCollector&) = delete;
  TrainingDataCollector& operator=(const TrainingDataCollector&) = delete;

 protected:
  TrainingDataCollector();
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_

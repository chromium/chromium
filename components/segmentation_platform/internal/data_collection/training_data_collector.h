// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_

#include <memory>

#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {

namespace processing {
class FeatureListQueryProcessor;
}

struct Config;
class HistogramSignalHandler;
class SegmentInfoDatabase;
class SignalStorageConfig;

// Collect training data and report as Ukm message. Live on main thread.
// TODO(ssid): Make a new class that owns the training data collector and
// model execution collector.
class TrainingDataCollector {
 public:
  static std::unique_ptr<TrainingDataCollector> Create(
      SegmentInfoDatabase* segment_info_database,
      processing::FeatureListQueryProcessor* processor,
      HistogramSignalHandler* histogram_signal_handler,
      SignalStorageConfig* signal_storage_config,
      std::vector<std::unique_ptr<Config>>* configs,
      PrefService* profile_prefs,
      base::Clock* clock);

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

  virtual ~TrainingDataCollector();

  // Disallow copy/assign.
  TrainingDataCollector(const TrainingDataCollector&) = delete;
  TrainingDataCollector& operator=(const TrainingDataCollector&) = delete;

 protected:
  TrainingDataCollector();
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_

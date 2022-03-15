// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_

#include <memory>

#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {

class FeatureListQueryProcessor;
class HistogramSignalHandler;
class SegmentInfoDatabase;
class SignalStorageConfig;

// Collect training data and report as Ukm message. Live on main thread.
// TODO(xingliu): Make a new class that owns the training data collector and
// model execution collector.
class TrainingDataCollector : public HistogramSignalHandler::Observer {
 public:
  static std::unique_ptr<TrainingDataCollector> Create(
      SegmentInfoDatabase* segment_info_database,
      FeatureListQueryProcessor* processor,
      HistogramSignalHandler* histogram_signal_handler,
      SignalStorageConfig* signal_storage_config,
      base::Clock* clock);

  // Called when model metadata is updated. May result in training data
  // collection behavior change.
  virtual void OnModelMetadataUpdated() = 0;

  // Called after segmentation platform is initialized. May report training data
  // to Ukm for |UMAOutput| in |SegmentationModelMetadata|.
  virtual void OnServiceInitialized() = 0;

  ~TrainingDataCollector() override = default;

  // Disallow copy/assign.
  TrainingDataCollector(const TrainingDataCollector&) = delete;
  TrainingDataCollector& operator=(const TrainingDataCollector&) = delete;

 protected:
  TrainingDataCollector() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_

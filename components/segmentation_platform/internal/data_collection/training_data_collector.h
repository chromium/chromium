// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_

#include "base/memory/raw_ptr.h"

namespace segmentation_platform {

class FeatureListQueryProcessor;

// Collect training data and report as Ukm message. Live on main thread.
// TODO(xingliu): Make a new class that owns the training data collector and
// model execution collector.
class TrainingDataCollector {
 public:
  explicit TrainingDataCollector(FeatureListQueryProcessor* processor);
  ~TrainingDataCollector();

  // Disallow copy/assign.
  TrainingDataCollector(const TrainingDataCollector&) = delete;
  TrainingDataCollector& operator=(const TrainingDataCollector&) = delete;

  // Called when model metadata is updated. May result in training data
  // collection behavior change.
  void OnModelMetadataUpdated();

  // Called after segmentation platform is initialized. May report training data
  // to Ukm that has a non-zero |duration| field in |UMAOutput|.
  void OnServiceInitialized();

 private:
  raw_ptr<FeatureListQueryProcessor> feature_list_query_processor_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_

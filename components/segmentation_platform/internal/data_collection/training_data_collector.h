// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_COLLECTOR_H_

namespace segmentation_platform {

// Collect training data and report as Ukm message. Live on main thread.
class TrainingDataCollector {
 public:
  TrainingDataCollector();
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
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_DATABASE_MAINTENANCE_IMPL_H_

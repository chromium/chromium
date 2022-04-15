// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_DATA_COLLECTION_SCHEDULER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_DATA_COLLECTION_SCHEDULER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {
class TrainingDataCollector;

// Class for determining when to upload structured metrics for continuously
// observed features.
class DataCollectionScheduler {
 public:
  DataCollectionScheduler(TrainingDataCollector* training_data_collector,
                          PrefService* prefs,
                          base::Clock* clock);
  ~DataCollectionScheduler();

  // Disallow copy/assign.
  DataCollectionScheduler(const DataCollectionScheduler&) = delete;
  DataCollectionScheduler& operator=(const DataCollectionScheduler&) = delete;

  // Report training data through UKM messages if applicable.
  void ReportTrainingDataIfApplicable();

 private:
  raw_ptr<TrainingDataCollector> training_data_collector_;
  PrefService* prefs_;
  base::Clock* clock_;

  base::WeakPtrFactory<DataCollectionScheduler> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_DATA_COLLECTION_SCHEDULER_H_

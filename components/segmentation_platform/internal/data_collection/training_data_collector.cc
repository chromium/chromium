// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include <utility>

#include "base/feature_list.h"
#include "components/segmentation_platform/internal/data_collection/dummy_training_data_collector.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"
#include "components/segmentation_platform/public/features.h"

namespace segmentation_platform {

// static
std::unique_ptr<TrainingDataCollector> TrainingDataCollector::Create(
    processing::FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    StorageService* storage_service,
    std::vector<std::unique_ptr<Config>>* configs,
    PrefService* profile_prefs,
    base::Clock* clock) {
  if (base::FeatureList::IsEnabled(
          features::kSegmentationStructuredMetricsFeature)) {
    return std::make_unique<TrainingDataCollectorImpl>(
        processor, histogram_signal_handler, storage_service, configs,
        profile_prefs, clock);
  }

  return std::make_unique<DummyTrainingDataCollector>();
}

TrainingDataCollector::TrainingDataCollector() = default;

TrainingDataCollector::~TrainingDataCollector() = default;

}  // namespace segmentation_platform

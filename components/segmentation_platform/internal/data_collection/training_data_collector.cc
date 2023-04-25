// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include <utility>

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"

namespace segmentation_platform {

// static
std::unique_ptr<TrainingDataCollector> TrainingDataCollector::Create(
    processing::FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    UserActionSignalHandler* user_action_signal_handler,
    StorageService* storage_service,
    std::vector<std::unique_ptr<Config>>* configs,
    PrefService* profile_prefs,
    base::Clock* clock) {
  return std::make_unique<TrainingDataCollectorImpl>(
      processor, histogram_signal_handler, user_action_signal_handler,
      storage_service, configs, profile_prefs, clock);
}

TrainingDataCollector::TrainingDataCollector() = default;

TrainingDataCollector::~TrainingDataCollector() = default;

}  // namespace segmentation_platform

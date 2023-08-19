// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include <utility>

#include "components/segmentation_platform/internal/data_collection/training_data_collector_impl.h"
#include "components/segmentation_platform/internal/database/cached_result_provider.h"

namespace segmentation_platform {

// static
std::unique_ptr<TrainingDataCollector> TrainingDataCollector::Create(
    const PlatformOptions& platform_options,
    processing::FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    UserActionSignalHandler* user_action_signal_handler,
    StorageService* storage_service,
    PrefService* profile_prefs,
    base::Clock* clock,
    CachedResultProvider* cached_result_provider) {
  return std::make_unique<TrainingDataCollectorImpl>(
      platform_options, processor, histogram_signal_handler,
      user_action_signal_handler, storage_service, profile_prefs, clock,
      cached_result_provider);
}

TrainingDataCollector::TrainingDataCollector() = default;

TrainingDataCollector::~TrainingDataCollector() = default;

}  // namespace segmentation_platform

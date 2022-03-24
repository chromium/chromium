// Copyright 2022 The Chromium Authors. All rights reserved.
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
    SegmentInfoDatabase* segment_info_database,
    FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler,
    SignalStorageConfig* signal_storage_config,
    base::Clock* clock) {
  if (base::FeatureList::IsEnabled(
          features::kSegmentationStructuredMetricsFeature)) {
    return std::make_unique<TrainingDataCollectorImpl>(
        segment_info_database, processor, histogram_signal_handler,
        signal_storage_config, clock);
  }

  return std::make_unique<DummyTrainingDataCollector>();
}

TrainingDataCollector::TrainingDataCollector() = default;

TrainingDataCollector::~TrainingDataCollector() = default;

}  // namespace segmentation_platform

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include "base/notreached.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"

namespace segmentation_platform {

TrainingDataCollector::TrainingDataCollector(
    FeatureListQueryProcessor* processor)
    : feature_list_query_processor_(processor) {}

TrainingDataCollector::~TrainingDataCollector() = default;

void TrainingDataCollector::OnModelMetadataUpdated() {
  NOTIMPLEMENTED();
}

void TrainingDataCollector::OnServiceInitialized() {
  NOTIMPLEMENTED();
}

}  // namespace segmentation_platform

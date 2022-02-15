// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include "base/notreached.h"

namespace segmentation_platform {

TrainingDataCollector::TrainingDataCollector() = default;

TrainingDataCollector::~TrainingDataCollector() = default;

void TrainingDataCollector::OnModelMetadataUpdated() {
  NOTIMPLEMENTED();
}

void TrainingDataCollector::OnServiceInitialized() {
  NOTIMPLEMENTED();
}

}  // namespace segmentation_platform

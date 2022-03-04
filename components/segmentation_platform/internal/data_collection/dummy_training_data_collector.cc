// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/dummy_training_data_collector.h"

namespace segmentation_platform {

DummyTrainingDataCollector::DummyTrainingDataCollector() = default;

DummyTrainingDataCollector::~DummyTrainingDataCollector() = default;

void DummyTrainingDataCollector::OnModelMetadataUpdated() {}

void DummyTrainingDataCollector::OnServiceInitialized() {}

void DummyTrainingDataCollector::OnHistogramSignalUpdated(
    const std::string& histogram_name,
    base::HistogramBase::Sample sample) {}

}  // namespace segmentation_platform

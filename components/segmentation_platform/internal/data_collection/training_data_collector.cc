// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"

#include "base/notreached.h"
#include "components/segmentation_platform/internal/execution/feature_list_query_processor.h"

namespace segmentation_platform {

TrainingDataCollector::TrainingDataCollector(
    FeatureListQueryProcessor* processor,
    HistogramSignalHandler* histogram_signal_handler)
    : feature_list_query_processor_(processor),
      histogram_signal_handler_(histogram_signal_handler) {
  DCHECK(histogram_signal_handler_);
  histogram_signal_handler_->AddObserver(this);
}

TrainingDataCollector::~TrainingDataCollector() {
  DCHECK(histogram_signal_handler_);
  histogram_signal_handler_->RemoveObserver(this);
}

void TrainingDataCollector::OnModelMetadataUpdated() {
  NOTIMPLEMENTED();
}

void TrainingDataCollector::OnServiceInitialized() {
  NOTIMPLEMENTED();
}

void TrainingDataCollector::OnHistogramSignalUpdated(
    const std::string& histogram_name,
    base::HistogramBase::Sample) {
  // TODO(xingliu): Check whether the histogram needs to trigger a data
  // collection, and report to UKM.
  NOTIMPLEMENTED();
}

}  // namespace segmentation_platform

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/segmentation_platform_service.h"

#include <ostream>

namespace segmentation_platform {

TrainingLabels::TrainingLabels() = default;
TrainingLabels::~TrainingLabels() = default;

std::ostream& operator<<(std::ostream& out, const TrainingLabels& labels) {
  if (!labels.output_metric.has_value()) {
    out << "{}";
    return out;
  }
  std::pair<std::string, base::HistogramBase::Sample> metric =
      labels.output_metric.value();
  out << "{" << metric.first << ": " << metric.second << "}";
  return out;
}

ServiceProxy* SegmentationPlatformService::GetServiceProxy() {
  return nullptr;
}

}  // namespace segmentation_platform

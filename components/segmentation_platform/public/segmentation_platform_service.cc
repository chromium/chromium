// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace segmentation_platform {

TrainingLabels::TrainingLabels() = default;
TrainingLabels::~TrainingLabels() = default;

TrainingLabels::TrainingLabels(const TrainingLabels& other) = default;

ServiceProxy* SegmentationPlatformService::GetServiceProxy() {
  return nullptr;
}

DatabaseClient* SegmentationPlatformService::GetDatabaseClient() {
  return nullptr;
}

}  // namespace segmentation_platform

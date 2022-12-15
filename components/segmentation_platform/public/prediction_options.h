// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_PREDICTION_OPTIONS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_PREDICTION_OPTIONS_H_

namespace segmentation_platform {

// Options that can be specified when invoking SegmentationPlatformService APIs.
struct PredictionOptions {
  PredictionOptions() = default;
  ~PredictionOptions() = default;

  PredictionOptions(const PredictionOptions&) = default;
  PredictionOptions& operator=(const PredictionOptions&) = default;

  // Set to true if on demand execution is to be done.
  bool on_demand_execution = false;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_PREDICTION_OPTIONS_H_

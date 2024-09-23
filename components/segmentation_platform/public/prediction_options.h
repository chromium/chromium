// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_PREDICTION_OPTIONS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_PREDICTION_OPTIONS_H_

namespace segmentation_platform {

// Options that can be specified when invoking SegmentationPlatformService APIs.
struct PredictionOptions {
  PredictionOptions() = default;
  explicit PredictionOptions(bool on_demand_execution,
                             bool can_update_cache_for_future_requests = false,
                             bool fallback_allowed = false);
  ~PredictionOptions() = default;

  PredictionOptions(const PredictionOptions&) = default;
  PredictionOptions& operator=(const PredictionOptions&) = default;

  // Cannot pass input_context. Set this option to get cached results.
  // If `can_fallback_to_execution` is set to true, then if cached results are
  // not available, the service will try executing the model.
  // `can_update_cache_for_future_requests` is default set to true which
  // means cached results will be updated, if model is executed successfully.
  static PredictionOptions ForCached(bool can_fallback_to_execution = false);

  // Set this option to get results by executing model ondemand.
  // If `can_fallback_to_execution` is set to true, then if there are no results
  // available from executing model ondemand, the service will try returning
  // cached results.
  static PredictionOptions ForOnDemand(bool can_fallback_to_cache = false);

  // Set to true if the execution results can be reused for future API calls to
  // fetch cached results.
  bool can_update_cache_for_future_requests{false};

  // Set to true if on demand execution is to be done.
  bool on_demand_execution{false};

  // Set to true if executing the fallback case if allowed in case
  // Example : If `can_fallback_to_cache = true` for ondemand option, returning
  // result using fallback is allowed. If `can_fallback_to_execution = true` for
  // cached option, returning result using fallback is allowed.
  bool fallback_allowed{false};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_PREDICTION_OPTIONS_H_

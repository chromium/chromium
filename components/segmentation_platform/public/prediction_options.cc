// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/prediction_options.h"

#include "base/values.h"

namespace segmentation_platform {

PredictionOptions::PredictionOptions(bool on_demand_execution,
                                     bool can_update_cache_for_future_requests,
                                     bool fallback_allowed)
    : can_update_cache_for_future_requests(
          can_update_cache_for_future_requests),
      on_demand_execution(on_demand_execution),
      fallback_allowed(fallback_allowed) {}

// static
PredictionOptions PredictionOptions::ForCached(bool can_fallback_to_execution) {
  return PredictionOptions(/*on_demand_execution=*/false,
                           /*can_update_cache_for_future_requests=*/true,
                           can_fallback_to_execution);
}

// static
PredictionOptions PredictionOptions::ForOnDemand(bool can_fallback_to_cache) {
  return PredictionOptions(/*on_demand_execution=*/true,
                           /*can_update_cache_for_future_requests=*/false,
                           can_fallback_to_cache);
}

}  // namespace segmentation_platform

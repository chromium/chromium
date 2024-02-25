// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_PENDING_SWAP_PARAMS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_PENDING_SWAP_PARAMS_H_

#include <optional>

#include "components/viz/service/viz_service_export.h"

namespace viz {

struct VIZ_SERVICE_EXPORT PendingSwapParams {
  explicit PendingSwapParams(int max_pending_swaps);

  PendingSwapParams(const PendingSwapParams&);
  PendingSwapParams& operator=(const PendingSwapParams&);
  PendingSwapParams(PendingSwapParams&&);
  PendingSwapParams& operator=(PendingSwapParams&&);

  int max_pending_swaps;
  // If set, should be the max number of pending frames when running at 90hz
  // Otherwise, fallback to `max_pending_swaps`.
  std::optional<int> max_pending_swaps_90hz;
  // If set, should be the max number of pending frames when running at or
  // above 120hz. Otherwise, fallback to `max_pending_swaps`.
  std::optional<int> max_pending_swaps_120hz;

  int GetMax();
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_PENDING_SWAP_PARAMS_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/pending_swap_params.h"

#include <algorithm>

namespace viz {

PendingSwapParams::PendingSwapParams(int max_pending_swaps)
    : max_pending_swaps(max_pending_swaps) {}

PendingSwapParams::PendingSwapParams(const PendingSwapParams&) = default;
PendingSwapParams& PendingSwapParams::operator=(const PendingSwapParams&) =
    default;

PendingSwapParams::PendingSwapParams(PendingSwapParams&&) = default;
PendingSwapParams& PendingSwapParams::operator=(PendingSwapParams&&) = default;

int PendingSwapParams::GetMax() {
  int ret = max_pending_swaps;
  if (max_pending_swaps_90hz)
    ret = std::max(ret, max_pending_swaps_90hz.value());
  if (max_pending_swaps_120hz)
    ret = std::max(ret, max_pending_swaps_120hz.value());
  return ret;
}

}  // namespace viz

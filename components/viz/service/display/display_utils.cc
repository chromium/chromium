// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_utils.h"

#include <vector>

namespace viz {

bool IsScroll(const std::vector<ui::LatencyInfo>& latency_infos) {
  for (const ui::LatencyInfo& latency_info : latency_infos) {
    base::TimeTicks scroll_ts;
    latency_info.FindLatency(
        ui::LatencyComponentType::
            INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
        &scroll_ts);
    if (!scroll_ts.is_null()) {
      return true;
    }

    latency_info.FindLatency(
        ui::LatencyComponentType::
            INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
        &scroll_ts);
    if (!scroll_ts.is_null()) {
      return true;
    }
  }
  return false;
}

}  // namespace viz

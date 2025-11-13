// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_INPUT_VIZ_TOUCH_STATE_H_
#define COMPONENTS_VIZ_COMMON_INPUT_VIZ_TOUCH_STATE_H_

#include <atomic>
#include <cstdint>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// This struct is allocated in a shared memory region created by the
// Viz process and shared read-only with the browser process.
struct VIZ_COMMON_EXPORT VizTouchState {
  // Initialize to false by default. This constructor will be called via
  // placement new.
  std::atomic<bool> is_sequence_active{false};
  // Stores the down_time_ms of the last touch sequence that Viz transferred
  // back to the Browser. This is used to prevent the Browser from immediately
  // re-transferring the same sequence back to Viz. It is reset to 0 when Viz
  // starts processing a new sequence.
  std::atomic<int64_t> last_transferred_back_down_time_ms{0};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_INPUT_VIZ_TOUCH_STATE_H_

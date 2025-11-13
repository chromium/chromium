// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_INPUT_VIZ_TOUCH_STATE_H_
#define COMPONENTS_VIZ_COMMON_INPUT_VIZ_TOUCH_STATE_H_

#include <atomic>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// This struct is allocated in a shared memory region created by the
// Viz process and shared read-only with the browser process.
struct VIZ_COMMON_EXPORT VizTouchState {
  // Initialize to false by default. This constructor will be called via
  // placement new.
  std::atomic<bool> is_sequence_active{false};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_INPUT_VIZ_TOUCH_STATE_H_

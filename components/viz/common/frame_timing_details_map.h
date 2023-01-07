// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_MAP_H_
#define COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_MAP_H_

#include "base/containers/flat_map.h"
#include "components/viz/common/frame_timing_details.h"

namespace viz {

// The uint32_t is the frame_token that the FrameTimingDetails is associated
// with.
using FrameTimingDetailsMap = base::flat_map<uint32_t, FrameTimingDetails>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_TIMING_DETAILS_MAP_H_

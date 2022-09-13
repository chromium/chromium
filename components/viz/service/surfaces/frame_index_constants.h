// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_FRAME_INDEX_CONSTANTS_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_FRAME_INDEX_CONSTANTS_H_

#include <cstdint>

namespace viz {

// Frame index values for the first frame received by from a viz client and an
// invalid value that will never be assigned to a frame.
inline constexpr uint64_t kFrameIndexStart = 2;
inline constexpr uint64_t kInvalidFrameIndex = 0;

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_FRAME_INDEX_CONSTANTS_H_

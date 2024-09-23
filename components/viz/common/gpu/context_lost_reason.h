// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_CONTEXT_LOST_REASON_H_
#define COMPONENTS_VIZ_COMMON_GPU_CONTEXT_LOST_REASON_H_

#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/constants.h"

namespace viz {

enum ContextLostReason {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  CONTEXT_INIT_FAILED = 0,
  CONTEXT_LOST_GPU_CHANNEL_ERROR = 1,
  CONTEXT_PARSE_ERROR_INVALID_SIZE = 2,
  CONTEXT_PARSE_ERROR_OUT_OF_BOUNDS = 3,
  CONTEXT_PARSE_ERROR_UNKNOWN_COMMAND = 4,
  CONTEXT_PARSE_ERROR_INVALID_ARGS = 5,
  CONTEXT_PARSE_ERROR_GENERIC_ERROR = 6,
  CONTEXT_LOST_GUILTY = 7,
  CONTEXT_LOST_INNOCENT = 8,
  CONTEXT_LOST_UNKNOWN = 9,
  CONTEXT_LOST_OUT_OF_MEMORY = 10,
  CONTEXT_LOST_MAKECURRENT_FAILED = 11,
  CONTEXT_LOST_INVALID_GPU_MESSAGE = 12,
  // SkiaRenderer marked context as lost because of failed Reshape call
  CONTEXT_LOST_RESHAPE_FAILED = 13,
  CONTEXT_LOST_SET_DRAW_RECTANGLE_FAILED [[deprecated]] = 14,
  CONTEXT_LOST_DIRECT_COMPOSITION_OVERLAY_FAILED = 15,
  CONTEXT_LOST_SWAP_FAILED = 16,
  CONTEXT_LOST_BEGIN_PAINT_FAILED = 17,
  CONTEXT_LOST_ALLOCATE_FRAME_BUFFERS_FAILED = 18,
  // Update kMaxValue here and <enum name="ContextLostReason"> in
  // tools/metrics/histograms/enums.xml when adding new values.
  kMaxValue = CONTEXT_LOST_ALLOCATE_FRAME_BUFFERS_FAILED
};

VIZ_COMMON_EXPORT ContextLostReason
GetContextLostReason(gpu::error::Error error,
                     gpu::error::ContextLostReason reason);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_CONTEXT_LOST_REASON_H_

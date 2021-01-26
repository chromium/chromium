// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_transition_directive.h"

#include "base/time/time.h"

namespace viz {

constexpr base::TimeDelta CompositorFrameTransitionDirective::kMaxDuration;

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective() =
    default;

CompositorFrameTransitionDirective::CompositorFrameTransitionDirective(
    uint32_t sequence_id,
    Type type,
    Effect effect,
    base::TimeDelta duration)
    : sequence_id_(sequence_id),
      type_(type),
      effect_(effect),
      duration_(duration) {
  DCHECK_LE(duration_, kMaxDuration);
}

}  // namespace viz

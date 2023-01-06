// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_

#include <stdint.h>

#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {

// A AggregatedHitTestRegion element with child_count of kEndOfList indicates
// the last element and end of the list.
constexpr int32_t kEndOfList = -1;

// An array of AggregatedHitTestRegion elements is used to define the
// aggregated hit-test data for the Display.
//
// It is designed to be in shared memory so that the viz service can
// write the hit_test data, and the viz host can read without
// process hops.
struct AggregatedHitTestRegion {
  AggregatedHitTestRegion() = default;

  AggregatedHitTestRegion(
      const FrameSinkId& frame_sink_id,
      uint32_t flags,
      const gfx::Rect& rect,
      const gfx::Transform& transform,
      int32_t child_count,
      uint32_t async_hit_test_reasons = AsyncHitTestReasons::kNotAsyncHitTest)
      : frame_sink_id(frame_sink_id),
        flags(flags),
        async_hit_test_reasons(async_hit_test_reasons),
        rect(rect),
        child_count(child_count),
        // NOLINTNEXTLINE(build/include_what_you_use). See crbug.com/1301129.
        transform(transform) {
    DCHECK_EQ(!!(flags & HitTestRegionFlags::kHitTestAsk),
              !!async_hit_test_reasons);
  }

  // The FrameSinkId corresponding to this region.  Events that match
  // are routed to this surface.
  FrameSinkId frame_sink_id;

  // HitTestRegionFlags to indicate the type of region.
  uint32_t flags = 0;

  // AsyncHitTestReasons to indicate why we are doing slow path hit testing.
  uint32_t async_hit_test_reasons = AsyncHitTestReasons::kNotAsyncHitTest;

  // The rectangle that defines the region in parent region's coordinate space.
  gfx::Rect rect;

  // The number of children including their children below this entry.
  // If this element is not matched then child_count elements can be skipped
  // to move to the next entry.
  int32_t child_count = 0;

  gfx::Transform transform;

  bool operator==(const AggregatedHitTestRegion& rhs) const {
    return (frame_sink_id == rhs.frame_sink_id && flags == rhs.flags &&
            async_hit_test_reasons == rhs.async_hit_test_reasons &&
            rect == rhs.rect && child_count == rhs.child_count &&
            transform == rhs.transform);
  }

  bool operator!=(const AggregatedHitTestRegion& other) const {
    return !(*this == other);
  }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_

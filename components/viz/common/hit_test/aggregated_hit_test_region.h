// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_

#include <stdint.h>

#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace viz {

namespace mojom {
class AggregatedHitTestRegionDataView;
}

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
        transform_(transform) {
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

  // gfx::Transform is backed by SkMatrix44. SkMatrix44 has a mutable attribute
  // which can be changed even during a const function call (e.g.
  // SkMatrix44::getType()). This means that when HitTestQuery reads the
  // transform in the read-only shared memory segment created (and populated) by
  // HitTestAggregator, if it attempts to perform any operation on the
  // transform (e.g. use Transform::IsIdentity()), skia will attempt to write to
  // the read-only shared memory segment, causing exception in HitTestQuery.
  // For this reason, it is necessary for the HitTestQuery to make a copy of the
  // transform before using it. To enforce this, the |transform_| attribute is
  // made private here, and exposed through an accessor which always makes a
  // copy.
  gfx::Transform transform() const { return transform_; }
  void set_transform(const gfx::Transform& transform) {
    transform_ = transform;
  }

  bool operator==(const AggregatedHitTestRegion& rhs) const {
    return (frame_sink_id == rhs.frame_sink_id && flags == rhs.flags &&
            async_hit_test_reasons == rhs.async_hit_test_reasons &&
            rect == rhs.rect && child_count == rhs.child_count &&
            transform_ == rhs.transform());
  }

  bool operator!=(const AggregatedHitTestRegion& other) const {
    return !(*this == other);
  }

 private:
  friend struct mojo::StructTraits<mojom::AggregatedHitTestRegionDataView,
                                   AggregatedHitTestRegion>;

  // The transform applied to the rect in parent region's coordinate space.
  gfx::Transform transform_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_AGGREGATED_HIT_TEST_REGION_H_

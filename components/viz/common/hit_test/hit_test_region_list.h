// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_LIST_H_
#define COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_LIST_H_

#include <vector>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {

// New flags must be added to GetFlagNames in hit_test_query.cc in order to be
// displayed in hit-test debug logging.
enum HitTestRegionFlags : uint32_t {
  // Region maps to this surface (me).
  kHitTestMine = 0x01,
  // Region ignored for hit testing (e.g. pointer-events:none).
  kHitTestIgnore = 0x02,
  // Region maps to child surface (OOPIF).
  kHitTestChildSurface = 0x04,
  // Irregular boundary - send HitTestRequest to resolve.
  kHitTestAsk = 0x08,

  // TODO(varkha): Add other kHitTest* flags as necessary for other event
  // sources such as mouse-wheel, stylus or perhaps even mouse-move.

  // Hit-testing for mouse events.
  kHitTestMouse = 0x10,
  // Hit-testing for touch events.
  kHitTestTouch = 0x20,

  // Client hasn't submitted its own hit-test data yet.
  kHitTestNotActive = 0x40,
};

// In viz hit testing surface layer, hit test regions are marked as kHitTestAsk
// for various reasons. This is a class to track the reasons of why a
// |HitTestRegion| cannot do synchronous targeting.
enum AsyncHitTestReasons : uint32_t {
  // The |HitTestRegion| does not have |kHitTestAsk| flag.
  kNotAsyncHitTest = 0,
  // The |HitTestRegion| is overlapped with its parent frame's elements.
  kOverlappedRegion = 1 << 0,
  // The |HitTestRegion| is clipped by irregular shape.
  kIrregularClip = 1 << 1,
  // The |HitTestRegion|'s surface has not been activated yet.
  kRegionNotActive = 1 << 2,
  // Synchronous event targeting aborts at the present of perspective transform.
  kPerspectiveTransform = 1 << 3,
  // The |HitTestRegion| is marked as |kHitTestAsk| because it comes from draw
  // quad. This is a reason specifically for slow path |hit-test| with draw quad
  // variant.
  kUseDrawQuadData = 1 << 4,

  // The maximum number of flags in this enum excluding itself.
  kAsyncHitTestReasonCount = 5,
};

struct HitTestRegion {
  // HitTestRegionFlags to indicate the type of HitTestRegion.
  uint32_t flags = 0;

  // AsyncHitTestReasons to indicate the reason of having |kHitTestAsk| flag.
  uint32_t async_hit_test_reasons = AsyncHitTestReasons::kNotAsyncHitTest;

  // FrameSinkId of this region.
  FrameSinkId frame_sink_id;

  // The rect of the region in the coordinate space of the embedder.
  gfx::Rect rect;

  // The transform of the region.  The transform applied to the rect
  // defines the space occupied by this region in the coordinate space of
  // the embedder.
  gfx::Transform transform;

  static bool IsEqual(const HitTestRegion&, const HitTestRegion&);
};

struct VIZ_COMMON_EXPORT HitTestRegionList {
  HitTestRegionList();
  ~HitTestRegionList();

  HitTestRegionList(const HitTestRegionList&);
  HitTestRegionList& operator=(const HitTestRegionList&);

  HitTestRegionList(HitTestRegionList&&);
  HitTestRegionList& operator=(HitTestRegionList&&);

  // HitTestRegionFlags indicate how to handle events that match no sub-regions.
  // kHitTestMine routes un-matched events to this surface (opaque).
  // kHitTestIgnore keeps previous match in the parent (transparent).
  uint32_t flags = 0;

  uint32_t async_hit_test_reasons = AsyncHitTestReasons::kNotAsyncHitTest;

  // The bounds of the surface.
  gfx::Rect bounds;

  // The transform applied to all regions in this surface.
  gfx::Transform transform;

  // The list of sub-regions in front to back order.
  std::vector<HitTestRegion> regions;

  static bool IsEqual(const HitTestRegionList&, const HitTestRegionList&);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_HIT_TEST_HIT_TEST_REGION_LIST_H_

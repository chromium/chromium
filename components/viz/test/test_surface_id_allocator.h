// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_SURFACE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_TEST_TEST_SURFACE_ID_ALLOCATOR_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace viz {

// A SurfaceId allocator for ease of allocating and incrementing SurfaceIds in
// tests. Avoids boilerplate associated with ParentLocalSurfaceIdAllocator. The
// LocalSurfaceId is initially valid and can be changed via Increment().
// Will implicitly convert to a SurfaceId so it can be used interchangeably with
// one.
class TestSurfaceIdAllocator {
 public:
  explicit TestSurfaceIdAllocator(const FrameSinkId& frame_sink_id);

  const FrameSinkId& frame_sink_id() const {
    return surface_id_.frame_sink_id();
  }
  const LocalSurfaceId& local_surface_id() const {
    return surface_id_.local_surface_id();
  }
  const SurfaceId& Get() const { return surface_id_; }
  operator SurfaceId() const { return surface_id_; }

  // Increments the child sequence number.
  void Increment();

 private:
  SurfaceId surface_id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_SURFACE_ID_ALLOCATOR_H_

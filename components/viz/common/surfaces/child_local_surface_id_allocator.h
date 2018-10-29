// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_CHILD_LOCAL_SURFACE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_CHILD_LOCAL_SURFACE_ID_ALLOCATOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// This is a helper class for generating local surface IDs for a single
// FrameSink. This is not threadsafe, to use from multiple threads wrap this
// class in a mutex.
// The parent embeds a child's surface. The child allocates a surface when it
// changes its contents or surface parameters, for example.
// This is that child allocator.
class VIZ_COMMON_EXPORT ChildLocalSurfaceIdAllocator {
 public:
  ChildLocalSurfaceIdAllocator();
  ChildLocalSurfaceIdAllocator(ChildLocalSurfaceIdAllocator&& other) = default;
  ChildLocalSurfaceIdAllocator& operator=(
      ChildLocalSurfaceIdAllocator&& other) = default;
  ~ChildLocalSurfaceIdAllocator() = default;

  // When a parent-allocated LocalSurfaceId arrives in the child, the child
  // needs to update its understanding of the last generated message so the
  // messages can continue to monotonically increase. Returns whether the
  // current LocalSurfaceId has been updated.
  bool UpdateFromParent(
      const LocalSurfaceId& parent_allocated_local_surface_id,
      base::TimeTicks parent_local_surface_id_allocation_time);

  const LocalSurfaceId& GenerateId();

  const LocalSurfaceId& GetCurrentLocalSurfaceId() const {
    return current_local_surface_id_;
  }

  base::TimeTicks allocation_time() const { return allocation_time_; }

 private:
  LocalSurfaceId current_local_surface_id_;

  // The time at which |current_local_surface_id_| was allocated.
  base::TimeTicks allocation_time_;

  DISALLOW_COPY_AND_ASSIGN(ChildLocalSurfaceIdAllocator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_CHILD_LOCAL_SURFACE_ID_ALLOCATOR_H_

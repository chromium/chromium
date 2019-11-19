// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_PARENT_LOCAL_SURFACE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_PARENT_LOCAL_SURFACE_ID_ALLOCATOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/viz_common_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace viz {

// This is a helper class for generating local surface IDs for a single
// FrameSink. This is not threadsafe, to use from multiple threads wrap this
// class in a mutex.
// The parent embeds a child's surface. The parent allocates a surface for the
// child when the parent needs to change surface parameters, for example.
class VIZ_COMMON_EXPORT ParentLocalSurfaceIdAllocator {
 public:
  explicit ParentLocalSurfaceIdAllocator(const base::TickClock* tick_clock);

  ParentLocalSurfaceIdAllocator();

  ~ParentLocalSurfaceIdAllocator() = default;

  // When a child-allocated LocalSurfaceId arrives in the parent, the parent
  // needs to update its understanding of the last generated message so the
  // messages can continue to monotonically increase. Returns whether the
  // current LocalSurfaceId has been updated.
  bool UpdateFromChild(
      const LocalSurfaceIdAllocation& child_local_surface_id_allocation);

  // Marks the last known LocalSurfaceId as invalid until the next call to
  // GenerateId. This is used to defer commits until some LocalSurfaceId is
  // provided from an external source.
  void Invalidate();

  void GenerateId();

  const LocalSurfaceIdAllocation& GetCurrentLocalSurfaceIdAllocation() const;

  bool HasValidLocalSurfaceIdAllocation() const;

  static const LocalSurfaceIdAllocation& InvalidLocalSurfaceIdAllocation();

  const base::UnguessableToken& GetEmbedToken() const;

  bool is_allocation_suppressed() const { return is_allocation_suppressed_; }

 private:
  LocalSurfaceIdAllocation current_local_surface_id_allocation_;

  // When true, the last known LocalSurfaceId is an invalid LocalSurfaceId.
  // TODO(fsamuel): Once the parent sequence number is only monotonically
  // increasing for a given embed_token then we should just reset
  // |current_local_surface_id_| to an invalid state.
  bool is_invalid_ = false;
  bool is_allocation_suppressed_ = false;
  const base::TickClock* tick_clock_;

  friend class ScopedSurfaceIdAllocator;

  DISALLOW_COPY_AND_ASSIGN(ParentLocalSurfaceIdAllocator);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_PARENT_LOCAL_SURFACE_ID_ALLOCATOR_H_

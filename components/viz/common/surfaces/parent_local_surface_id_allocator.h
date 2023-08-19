// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_PARENT_LOCAL_SURFACE_ID_ALLOCATOR_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_PARENT_LOCAL_SURFACE_ID_ALLOCATOR_H_

#include <stdint.h>

#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// This is a helper class for generating local surface IDs for a single
// FrameSink. This is not threadsafe, to use from multiple threads wrap this
// class in a mutex.
// The parent embeds a child's surface. The parent allocates a surface for the
// child when the parent needs to change surface parameters, for example.
class VIZ_COMMON_EXPORT ParentLocalSurfaceIdAllocator {
 public:
  ParentLocalSurfaceIdAllocator();

  ParentLocalSurfaceIdAllocator(const ParentLocalSurfaceIdAllocator&) = delete;
  ParentLocalSurfaceIdAllocator& operator=(
      const ParentLocalSurfaceIdAllocator&) = delete;

  ~ParentLocalSurfaceIdAllocator() = default;

  // When a child-allocated LocalSurfaceId arrives in the parent, the parent
  // needs to update its understanding of the last generated message so the
  // messages can continue to monotonically increase. Returns whether the
  // current LocalSurfaceId has been updated.
  bool UpdateFromChild(const LocalSurfaceId& child_local_surface_id);

  // Marks the last known LocalSurfaceId as invalid until the next call to
  // GenerateId. This is used to defer commits until some LocalSurfaceId is
  // provided from an external source. If `also_invalidate_allocation_group` is
  // set to true, the allocation group is also invalidated.
  void Invalidate(bool also_invalidate_allocation_group = false);

  void GenerateId();

  const LocalSurfaceId& GetCurrentLocalSurfaceId() const;

  bool HasValidLocalSurfaceId() const;

  static const LocalSurfaceId& InvalidLocalSurfaceId();

  const base::UnguessableToken& GetEmbedToken() const;

  bool is_allocation_suppressed() const { return is_allocation_suppressed_; }

 private:
  LocalSurfaceId current_local_surface_id_;

  // When true, the last known LocalSurfaceId is an invalid LocalSurfaceId.
  // TODO(fsamuel): Once the parent sequence number is only monotonically
  // increasing for a given embed_token then we should just reset
  // |current_local_surface_id_| to an invalid state.
  bool is_invalid_ = false;
  bool is_allocation_suppressed_ = false;

  friend class ScopedSurfaceIdAllocator;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_PARENT_LOCAL_SURFACE_ID_ALLOCATOR_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_SURFACE_ID_ALLOCATOR_SET_H_
#define COMPONENTS_VIZ_TEST_SURFACE_ID_ALLOCATOR_SET_H_

#include <map>

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

namespace viz {

// This class holds the ParentLocalSurfaceIdAllocators for different
// FrameSinkIds in tests.
class SurfaceIdAllocatorSet {
 public:
  SurfaceIdAllocatorSet();
  ~SurfaceIdAllocatorSet();

  // Returns the ParentLocalSurfaceIdAllocator of |frame_sink_id|.
  ParentLocalSurfaceIdAllocator* GetAllocator(const FrameSinkId& frame_sink_id);

  // Creates a SurfaceId with the given sequence numbers and the embed token of
  // |frame_sink_id|'s allocator.
  SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id,
                          uint32_t parent_sequence_number,
                          uint32_t child_sequence_number = 1);

 private:
  std::map<FrameSinkId, ParentLocalSurfaceIdAllocator> allocators_;
};

}  // namespace viz

#endif  //  COMPONENTS_VIZ_TEST_SURFACE_ID_ALLOCATOR_SET_H_

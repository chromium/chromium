// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"

#include <stdint.h>

#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"

namespace viz {

ChildLocalSurfaceIdAllocator::ChildLocalSurfaceIdAllocator()
    : current_local_surface_id_(kInvalidParentSequenceNumber,
                                kInitialChildSequenceNumber,
                                base::UnguessableToken()) {}

bool ChildLocalSurfaceIdAllocator::UpdateFromParent(
    const LocalSurfaceId& parent_allocated_local_surface_id,
    base::TimeTicks parent_local_surface_id_allocation_time) {
  if ((parent_allocated_local_surface_id.parent_sequence_number() >
       current_local_surface_id_.parent_sequence_number()) ||
      parent_allocated_local_surface_id.embed_token() !=
          current_local_surface_id_.embed_token()) {
    current_local_surface_id_.parent_sequence_number_ =
        parent_allocated_local_surface_id.parent_sequence_number_;
    current_local_surface_id_.embed_token_ =
        parent_allocated_local_surface_id.embed_token_;
    allocation_time_ = parent_local_surface_id_allocation_time;
    return true;
  }
  return false;
}

const LocalSurfaceId& ChildLocalSurfaceIdAllocator::GenerateId() {
  // UpdateFromParent must be called before we can generate a valid ID.
  DCHECK_NE(current_local_surface_id_.parent_sequence_number(),
            kInvalidParentSequenceNumber);

  ++current_local_surface_id_.child_sequence_number_;
  allocation_time_ = base::TimeTicks::Now();

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Embed.Flow",
      TRACE_ID_GLOBAL(current_local_surface_id_.embed_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ChildLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
      current_local_surface_id_.ToString());
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_GLOBAL(current_local_surface_id_.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ChildLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
      current_local_surface_id_.ToString());

  return current_local_surface_id_;
}

}  // namespace viz

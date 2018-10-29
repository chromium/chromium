// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"

namespace viz {

constexpr LocalSurfaceId g_invalid_local_surface_id;

ParentLocalSurfaceIdAllocator::ParentLocalSurfaceIdAllocator()
    : current_local_surface_id_(kInvalidParentSequenceNumber,
                                kInitialChildSequenceNumber,
                                base::UnguessableToken::Create()) {
  GenerateId();
}

bool ParentLocalSurfaceIdAllocator::UpdateFromChild(
    const LocalSurfaceId& child_allocated_local_surface_id,
    base::TimeTicks child_local_surface_id_allocation_time) {
  if (child_allocated_local_surface_id.child_sequence_number() >
      current_local_surface_id_.child_sequence_number()) {
    current_local_surface_id_.child_sequence_number_ =
        child_allocated_local_surface_id.child_sequence_number_;
    allocation_time_ = child_local_surface_id_allocation_time;
    is_invalid_ = false;
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Embed.Flow",
        TRACE_ID_GLOBAL(current_local_surface_id_.embed_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "UpdateFromChild", "local_surface_id",
        current_local_surface_id_.ToString());
    return true;
  }
  return false;
}

void ParentLocalSurfaceIdAllocator::Reset(
    const LocalSurfaceId& local_surface_id) {
  current_local_surface_id_ = local_surface_id;
}

void ParentLocalSurfaceIdAllocator::Invalidate() {
  is_invalid_ = true;
}

const LocalSurfaceId& ParentLocalSurfaceIdAllocator::GenerateId() {
  if (!is_allocation_suppressed_) {
    ++current_local_surface_id_.parent_sequence_number_;
    allocation_time_ = base::TimeTicks::Now();
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Embed.Flow",
        TRACE_ID_GLOBAL(current_local_surface_id_.embed_trace_id()),
        TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "ParentLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
        current_local_surface_id_.ToString());
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Submission.Flow",
        TRACE_ID_GLOBAL(current_local_surface_id_.submission_trace_id()),
        TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "ParentLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
        current_local_surface_id_.ToString());
  }
  is_invalid_ = false;


  return current_local_surface_id_;
}

const LocalSurfaceId& ParentLocalSurfaceIdAllocator::GetCurrentLocalSurfaceId()
    const {
  if (is_invalid_)
    return g_invalid_local_surface_id;
  return current_local_surface_id_;
}

// static
const LocalSurfaceId& ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId() {
  return g_invalid_local_surface_id;
}

}  // namespace viz

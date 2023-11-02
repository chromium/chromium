// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"

#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"

namespace viz {

ChildLocalSurfaceIdAllocator::ChildLocalSurfaceIdAllocator()
    : current_local_surface_id_(kInvalidParentSequenceNumber,
                                kInitialChildSequenceNumber,
                                base::UnguessableToken()) {}

bool ChildLocalSurfaceIdAllocator::UpdateFromParent(
    const LocalSurfaceId& parent_local_surface_id) {
  const LocalSurfaceId& current_local_surface_id = current_local_surface_id_;
  const LocalSurfaceId& parent_allocated_local_surface_id =
      parent_local_surface_id;

  // If the parent has not incremented its parent sequence number or updated its
  // embed token then there is nothing to do here. This allocator already has
  // the latest LocalSurfaceId.
  if (current_local_surface_id.parent_sequence_number() >=
          parent_allocated_local_surface_id.parent_sequence_number() &&
      current_local_surface_id.embed_token() ==
          parent_allocated_local_surface_id.embed_token()) {
    return false;
  }

  if (current_local_surface_id.child_sequence_number() >
      parent_allocated_local_surface_id.child_sequence_number()) {
    // If the current LocalSurfaceId has a newer child sequence number
    // than the one provided by the parent, then the merged LocalSurfaceId
    // is actually a new LocalSurfaceId and so we report its allocation time
    // as now.
    if (current_local_surface_id != parent_allocated_local_surface_id) {
      TRACE_EVENT_WITH_FLOW2(
          TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
          "ChildLocalSurfaceIdAllocator::UpdateFromParent New Id Allocation",
          TRACE_ID_LOCAL(
              parent_allocated_local_surface_id.submission_trace_id()),
          TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "current",
          current_local_surface_id_.ToString(), "parent",
          parent_local_surface_id.ToString());
    }
  } else if (current_local_surface_id != parent_allocated_local_surface_id) {
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "ChildLocalSurfaceIdAllocator::UpdateFromParent Synchronization",
        TRACE_ID_LOCAL(parent_allocated_local_surface_id.submission_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "current",
        current_local_surface_id_.ToString(), "parent",
        parent_local_surface_id.ToString());
  }

  // If embed token has changed, accept all fields from the parent
  // including child sequence number.
  if (current_local_surface_id.embed_token() !=
      parent_allocated_local_surface_id.embed_token()) {
    current_local_surface_id_.child_sequence_number_ =
        parent_allocated_local_surface_id.child_sequence_number_;
  }

  current_local_surface_id_.parent_sequence_number_ =
      parent_allocated_local_surface_id.parent_sequence_number_;
  current_local_surface_id_.embed_token_ =
      parent_allocated_local_surface_id.embed_token_;

  return true;
}

void ChildLocalSurfaceIdAllocator::GenerateId() {
  // UpdateFromParent must be called before we can generate a valid ID.
  DCHECK_NE(current_local_surface_id_.parent_sequence_number(),
            kInvalidParentSequenceNumber);

  ++current_local_surface_id_.child_sequence_number_;

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
}

}  // namespace viz

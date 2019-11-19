// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"

namespace viz {

constexpr LocalSurfaceIdAllocation g_invalid_local_surface_id_allocation;

ParentLocalSurfaceIdAllocator::ParentLocalSurfaceIdAllocator(
    const base::TickClock* tick_clock)
    : current_local_surface_id_allocation_(
          LocalSurfaceId(kInvalidParentSequenceNumber,
                         kInitialChildSequenceNumber,
                         base::UnguessableToken::Create()),
          base::TimeTicks()),
      tick_clock_(tick_clock) {}

ParentLocalSurfaceIdAllocator::ParentLocalSurfaceIdAllocator()
    : ParentLocalSurfaceIdAllocator(base::DefaultTickClock::GetInstance()) {}

bool ParentLocalSurfaceIdAllocator::UpdateFromChild(
    const LocalSurfaceIdAllocation& child_local_surface_id_allocation) {
  const LocalSurfaceId& current_local_surface_id =
      current_local_surface_id_allocation_.local_surface_id_;
  const LocalSurfaceId& child_allocated_local_surface_id =
      child_local_surface_id_allocation.local_surface_id();

  // If the child has not incremented its child sequence number then there is
  // nothing to do here. This allocator already has the latest LocalSurfaceId.
  if (current_local_surface_id.child_sequence_number() >=
      child_allocated_local_surface_id.child_sequence_number()) {
    return false;
  }

  is_invalid_ = false;
  if (current_local_surface_id.parent_sequence_number() >
      child_allocated_local_surface_id.parent_sequence_number()) {
    // If the current LocalSurfaceId has a newer parent sequence number
    // than the one provided by the child, then the merged LocalSurfaceId
    // is actually a new LocalSurfaceId and so we report its allocation time
    // as now.
    TRACE_EVENT2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "ParentLocalSurfaceIdAllocator::UpdateFromChild New Allocation",
        "current", current_local_surface_id_allocation_.ToString(), "child",
        child_local_surface_id_allocation.ToString());
    current_local_surface_id_allocation_.allocation_time_ =
        tick_clock_->NowTicks();
  } else {
    TRACE_EVENT2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "ParentLocalSurfaceIdAllocator::UpdateFromChild Synchronization",
        "current", current_local_surface_id_allocation_.ToString(), "child",
        child_local_surface_id_allocation.ToString());
    current_local_surface_id_allocation_.allocation_time_ =
        child_local_surface_id_allocation.allocation_time();
  }

  current_local_surface_id_allocation_.local_surface_id_
      .child_sequence_number_ =
      child_allocated_local_surface_id.child_sequence_number_;

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Embed.Flow",
      TRACE_ID_GLOBAL(current_local_surface_id_allocation_.local_surface_id_
                          .embed_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "UpdateFromChild", "local_surface_id",
      current_local_surface_id_allocation_.local_surface_id_.ToString());

  return true;
}

void ParentLocalSurfaceIdAllocator::Invalidate() {
  is_invalid_ = true;
}

void ParentLocalSurfaceIdAllocator::GenerateId() {
  if (is_allocation_suppressed_)
    return;
  is_invalid_ = false;

  ++current_local_surface_id_allocation_.local_surface_id_
        .parent_sequence_number_;
  current_local_surface_id_allocation_.allocation_time_ =
      tick_clock_->NowTicks();

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Embed.Flow",
      TRACE_ID_GLOBAL(current_local_surface_id_allocation_.local_surface_id_
                          .embed_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ParentLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
      current_local_surface_id_allocation_.local_surface_id_.ToString());

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_GLOBAL(current_local_surface_id_allocation_.local_surface_id_
                          .submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ParentLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
      current_local_surface_id_allocation_.local_surface_id_.ToString());
}

const LocalSurfaceIdAllocation&
ParentLocalSurfaceIdAllocator::GetCurrentLocalSurfaceIdAllocation() const {
  if (is_invalid_)
    return g_invalid_local_surface_id_allocation;
  return current_local_surface_id_allocation_;
}

bool ParentLocalSurfaceIdAllocator::HasValidLocalSurfaceIdAllocation() const {
  return !is_invalid_ && current_local_surface_id_allocation_.IsValid();
}

const base::UnguessableToken& ParentLocalSurfaceIdAllocator::GetEmbedToken()
    const {
  return current_local_surface_id_allocation_.local_surface_id_.embed_token();
}

// static
const LocalSurfaceIdAllocation&
ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceIdAllocation() {
  return g_invalid_local_surface_id_allocation;
}

}  // namespace viz

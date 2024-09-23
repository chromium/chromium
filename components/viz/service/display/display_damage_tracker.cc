// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_damage_tracker.h"

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"

namespace viz {
namespace {

// Kill switch for optimization to skip updating pending surfaces on begin
// frames from other displays.
BASE_FEATURE(kSkipBeginFramesFromOtherDisplays,
             "SkipBeginFramesFromOtherDisplays",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool ShouldAccumulateInteraction(
    SurfaceObserver::HandleInteraction handle_interaction) {
  switch (handle_interaction) {
    case SurfaceObserver::HandleInteraction::kYes:
      return true;
    case SurfaceObserver::HandleInteraction::kNo:
      return false;
    case SurfaceObserver::HandleInteraction::kNoChange:
      return false;
  }
}

}  // namespace

DisplayDamageTracker::DisplayDamageTracker(SurfaceManager* surface_manager,
                                           SurfaceAggregator* aggregator)
    : surface_manager_(surface_manager), aggregator_(aggregator) {
  DCHECK(surface_manager_);
  DCHECK(aggregator_);
  surface_manager_->AddObserver(this);
}

DisplayDamageTracker::~DisplayDamageTracker() {
  surface_manager_->RemoveObserver(this);
}

void DisplayDamageTracker::SetDisplayBeginFrameSourceId(
    uint64_t begin_frame_source_id) {
  if (base::FeatureList::IsEnabled(kSkipBeginFramesFromOtherDisplays)) {
    begin_frame_source_id_ = begin_frame_source_id;
  }
}

void DisplayDamageTracker::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void DisplayDamageTracker::SetRootFrameMissing(bool missing) {
  TRACE_EVENT1("viz", "DisplayDamageTracker::SetRootFrameMissing", "missing",
               missing);
  if (root_frame_missing_ == missing)
    return;

  root_frame_missing_ = missing;
  NotifyRootFrameMissing(missing);
}

void DisplayDamageTracker::SetNewRootSurface(const SurfaceId& root_surface_id) {
  TRACE_EVENT0("viz", "DisplayDamageTracker::SetNewRootSurface");
  root_surface_id_ = root_surface_id;
  UpdateRootFrameMissing();
  SetRootSurfaceDamaged();
}

void DisplayDamageTracker::SetRootSurfaceDamaged() {
  BeginFrameAck ack;
  ack.has_damage = true;
  // Since we're damaging to redraw the last activated frame, there shouldn't be
  // any change in interaction state.
  ProcessSurfaceDamage(root_surface_id_, ack, true,
                       HandleInteraction::kNoChange);
}

bool DisplayDamageTracker::IsRootSurfaceValid() const {
  return root_surface_id_.is_valid();
}

void DisplayDamageTracker::DisplayResized() {
  expecting_root_surface_damage_because_of_resize_ = true;
  // Technically we don't have any damage yet, but we need to draw after resize,
  // so we report display damaged here.
  NotifyDisplayDamaged(root_surface_id_);
}

void DisplayDamageTracker::ProcessSurfaceDamage(
    const SurfaceId& surface_id,
    const BeginFrameAck& ack,
    bool display_damaged,
    HandleInteraction handle_interaction) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"),
               "DisplayDamageTracker::SurfaceDamaged", "surface_id",
               surface_id.ToString());

  has_surface_damage_due_to_interaction_ |=
      ShouldAccumulateInteraction(handle_interaction);

  if (surface_id == root_surface_id_)
    expecting_root_surface_damage_because_of_resize_ = false;

  // Update surface state.
  bool valid_ack = ack.frame_id.IsSequenceValid();
  if (valid_ack) {
    auto it = surface_states_.find(surface_id);
    // Ignore stray acknowledgments for prior BeginFrames, to ensure we don't
    // override a newer sequence number in the surface state. We may receive
    // such stray acks e.g. when a CompositorFrame activates in a later
    // BeginFrame than it was created.
    if (it != surface_states_.end() &&
        !it->second.last_ack.frame_id.IsNextInSequenceTo(ack.frame_id)) {
      it->second.last_ack = ack;
    } else {
      valid_ack = false;
    }
  }

  if (display_damaged) {
    NotifyDisplayDamaged(surface_id);
  } else if (valid_ack) {
    NotifyPendingSurfacesChanged();
  }
}

bool DisplayDamageTracker::SurfaceHasUnackedFrame(
    const SurfaceId& surface_id) const {
  Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
  if (!surface)
    return false;

  return surface->HasUnackedActiveFrame();
}

bool DisplayDamageTracker::HasPendingSurfaces(
    const BeginFrameArgs& begin_frame_args) {
  for (auto& entry : surface_states_) {
    const SurfaceId& surface_id = entry.first;
    const SurfaceBeginFrameState& state = entry.second;

    // Surface is ready if it hasn't received the current BeginFrame or receives
    // BeginFrames from a different source and thus likely belongs to a
    // different surface hierarchy.
    if (!state.last_args.IsValid() ||
        state.last_args.frame_id != begin_frame_args.frame_id) {
      continue;
    }

    // Surface is ready if it has acknowledged the current BeginFrame.
    if (state.last_ack.frame_id == begin_frame_args.frame_id) {
      continue;
    }

    // Surface is ready if there is an unacked active CompositorFrame, because
    // its producer is CompositorFrameAck throttled.
    if (SurfaceHasUnackedFrame(surface_id))
      continue;

    TRACE_EVENT_INSTANT2("viz", "DisplayDamageTracker::HasPendingSurfaces",
                         TRACE_EVENT_SCOPE_THREAD, "has_pending_surfaces", true,
                         "pending_surface_id", surface_id.ToString());

    return true;
  }

  TRACE_EVENT_INSTANT1("viz", "DisplayDamageTracker::HasPendingSurfaces",
                       TRACE_EVENT_SCOPE_THREAD, "has_pending_surfaces", false);

  return false;
}

bool DisplayDamageTracker::HasDamageDueToInteraction() {
  return has_surface_damage_due_to_interaction_;
}

void DisplayDamageTracker::DidFinishFrame() {
  // We need to unset this bit otherwise we will continue to draw immediately
  // even when we have no new damage from an active scroller.
  has_surface_damage_due_to_interaction_ = false;
}

void DisplayDamageTracker::OnSurfaceMarkedForDestruction(
    const SurfaceId& surface_id) {
  auto it = surface_states_.find(surface_id);
  if (it == surface_states_.end())
    return;
  surface_states_.erase(it);

  NotifyPendingSurfacesChanged();
}

bool DisplayDamageTracker::CheckForDisplayDamage(const SurfaceId& surface_id) {
  return aggregator_->CheckForDisplayDamage(surface_id);
}

bool DisplayDamageTracker::OnSurfaceDamaged(
    const SurfaceId& surface_id,
    const BeginFrameAck& ack,
    HandleInteraction handle_interaction) {
  bool display_damaged = false;
  if (ack.has_damage) {
    // Display is damaged if we purged some resources or if this surface
    // contributes to this display.
    display_damaged = aggregator_->ForceReleaseResourcesIfNeeded(surface_id) ||
                      CheckForDisplayDamage(surface_id);

    if (surface_id == root_surface_id_)
      display_damaged = true;
    if (display_damaged)
      surfaces_to_ack_on_next_draw_.push_back(surface_id);
  }

  if (surface_id == root_surface_id_)
    UpdateRootFrameMissing();

  ProcessSurfaceDamage(surface_id, ack, display_damaged, handle_interaction);

  return display_damaged;
}

bool DisplayDamageTracker::CheckBeginFrameSourceId(uint64_t source_id) {
  return !begin_frame_source_id_ || source_id == *begin_frame_source_id_ ||
         source_id == BeginFrameArgs::kManualSourceId;
}

void DisplayDamageTracker::OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                                   const BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"),
               "DisplayDamageTracker::SurfaceDamageExpected", "surface_id",
               surface_id.ToString());

  // Insert a new state for the surface if we don't know of it yet. We don't
  // use OnSurfaceCreated() for this, because it may not be called if a
  // CompositorFrameSinkSupport starts submitting frames to a different
  // Display, but continues using the same Surface, or if a Surface does not
  // activate its first CompositorFrame immediately.
  surface_states_[surface_id].last_args = args;

  // HasPendingSurfaces() won't consider any surfaces that received a begin
  // frame from a different displays begin frame source but will still iterate
  // through all of the entries in `surface_states_`. That iteration is
  // expensive so avoid doing it when source_id doesn't match.
  if (!CheckBeginFrameSourceId(args.frame_id.source_id)) {
    return;
  }

  NotifyPendingSurfacesChanged();
}

void DisplayDamageTracker::UpdateRootFrameMissing() {
  Surface* surface = surface_manager_->GetSurfaceForId(root_surface_id_);
  SetRootFrameMissing(!surface || !surface->HasActiveFrame());
}

void DisplayDamageTracker::RunDrawCallbacks() {
  for (const auto& surface_id : surfaces_to_ack_on_next_draw_) {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
    if (surface)
      surface->SendAckToClient();
  }
  surfaces_to_ack_on_next_draw_.clear();
  // |surfaces_to_ack_on_next_draw_| does not cover surfaces that are being
  // embedded for the first time, so also go through SurfaceAggregator's list.
  for (const auto& surface_id : aggregator_->previous_contained_surfaces()) {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
    if (surface) {
      surface->SendAckToClient();
    }
  }
}

void DisplayDamageTracker::NotifyDisplayDamaged(SurfaceId surface_id) {
  if (delegate_) {
    delegate_->OnDisplayDamaged(surface_id);
  }
}

void DisplayDamageTracker::NotifyRootFrameMissing(bool missing) {
  if (delegate_) {
    delegate_->OnRootFrameMissing(missing);
  }
}

void DisplayDamageTracker::NotifyPendingSurfacesChanged() {
  if (delegate_) {
    delegate_->OnPendingSurfacesChanged();
  }
}

}  // namespace viz

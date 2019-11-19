// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_scheduler.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/display/output_surface.h"

namespace viz {

DisplayScheduler::DisplayScheduler(BeginFrameSource* begin_frame_source,
                                   base::SingleThreadTaskRunner* task_runner,
                                   int max_pending_swaps,
                                   bool wait_for_all_surfaces_before_draw)
    : client_(nullptr),
      begin_frame_source_(begin_frame_source),
      task_runner_(task_runner),
      inside_surface_damaged_(false),
      visible_(false),
      output_surface_lost_(false),
      root_frame_missing_(true),
      inside_begin_frame_deadline_interval_(false),
      needs_draw_(false),
      expecting_root_surface_damage_because_of_resize_(false),
      has_pending_surfaces_(false),
      next_swap_id_(1),
      pending_swaps_(0),
      max_pending_swaps_(max_pending_swaps),
      wait_for_all_surfaces_before_draw_(wait_for_all_surfaces_before_draw),
      observing_begin_frame_source_(false) {
  begin_frame_deadline_closure_ = base::BindRepeating(
      &DisplayScheduler::OnBeginFrameDeadline, weak_ptr_factory_.GetWeakPtr());

  // The DisplayScheduler handles animate_only BeginFrames as if they were
  // normal BeginFrames: Clients won't commit a CompositorFrame but will still
  // acknowledge when they have completed the BeginFrame via BeginFrameAcks and
  // the DisplayScheduler will still indicate when all clients have finished via
  // DisplayObserver::OnDisplayDidFinishFrame.
  wants_animate_only_begin_frames_ = true;
}

DisplayScheduler::~DisplayScheduler() {
  // It is possible for DisplayScheduler to be destroyed while there's an
  // in-flight swap. So always mark the gpu as not busy during destruction.
  begin_frame_source_->SetIsGpuBusy(false);
  StopObservingBeginFrames();
}

void DisplayScheduler::SetClient(DisplaySchedulerClient* client) {
  client_ = client;
}

void DisplayScheduler::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  // If going invisible, we'll stop observing begin frames once we try
  // to draw and fail.
  MaybeStartObservingBeginFrames();
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::SetRootFrameMissing(bool missing) {
  TRACE_EVENT1("viz", "DisplayScheduler::SetRootFrameMissing", "missing",
               missing);
  if (root_frame_missing_ == missing)
    return;

  root_frame_missing_ = missing;
  MaybeStartObservingBeginFrames();
  ScheduleBeginFrameDeadline();
}

// This is used to force an immediate swap before a resize.
void DisplayScheduler::ForceImmediateSwapIfPossible() {
  TRACE_EVENT0("viz", "DisplayScheduler::ForceImmediateSwapIfPossible");
  bool in_begin = inside_begin_frame_deadline_interval_;
  bool did_draw = AttemptDrawAndSwap();
  if (in_begin)
    DidFinishFrame(did_draw);
}

void DisplayScheduler::DisplayResized() {
  expecting_root_surface_damage_because_of_resize_ = true;
  needs_draw_ = true;
  ScheduleBeginFrameDeadline();
}

// Notification that there was a resize or the root surface changed and
// that we should just draw immediately.
void DisplayScheduler::SetNewRootSurface(const SurfaceId& root_surface_id) {
  TRACE_EVENT0("viz", "DisplayScheduler::SetNewRootSurface");
  root_surface_id_ = root_surface_id;
  BeginFrameAck ack;
  ack.has_damage = true;
  ProcessSurfaceDamage(root_surface_id, ack, true);
}

// Indicates that there was damage to one of the surfaces.
// Has some logic to wait for multiple active surfaces before
// triggering the deadline.
void DisplayScheduler::ProcessSurfaceDamage(const SurfaceId& surface_id,
                                            const BeginFrameAck& ack,
                                            bool display_damaged) {
  TRACE_EVENT1("viz", "DisplayScheduler::SurfaceDamaged", "surface_id",
               surface_id.ToString());

  // We may cause a new BeginFrame to be run inside this method, but to help
  // avoid being reentrant to the caller of SurfaceDamaged, track when this is
  // happening with |inside_surface_damaged_|.
  base::AutoReset<bool> auto_reset(&inside_surface_damaged_, true);

  if (display_damaged) {
    needs_draw_ = true;

    if (surface_id == root_surface_id_)
      expecting_root_surface_damage_because_of_resize_ = false;

    MaybeStartObservingBeginFrames();
  }

  // Update surface state.
  bool valid_ack = ack.sequence_number != BeginFrameArgs::kInvalidFrameNumber;
  if (valid_ack) {
    auto it = surface_states_.find(surface_id);
    // Ignore stray acknowledgments for prior BeginFrames, to ensure we don't
    // override a newer sequence number in the surface state. We may receive
    // such stray acks e.g. when a CompositorFrame activates in a later
    // BeginFrame than it was created.
    if (it != surface_states_.end() &&
        (it->second.last_ack.source_id != ack.source_id ||
         it->second.last_ack.sequence_number < ack.sequence_number)) {
      it->second.last_ack = ack;
    } else {
      valid_ack = false;
    }
  }

  bool pending_surfaces_changed = false;
  if (display_damaged || valid_ack)
    pending_surfaces_changed = UpdateHasPendingSurfaces();

  if (display_damaged || pending_surfaces_changed)
    ScheduleBeginFrameDeadline();
}

bool DisplayScheduler::UpdateHasPendingSurfaces() {
  // If we're not currently inside a deadline interval, we will call
  // UpdateHasPendingSurfaces() again during OnBeginFrameImpl().
  if (!inside_begin_frame_deadline_interval_ || !client_)
    return false;

  bool old_value = has_pending_surfaces_;

  for (const std::pair<SurfaceId, SurfaceBeginFrameState>& entry :
       surface_states_) {
    const SurfaceId& surface_id = entry.first;
    const SurfaceBeginFrameState& state = entry.second;

    // Surface is ready if it hasn't received the current BeginFrame or receives
    // BeginFrames from a different source and thus likely belongs to a
    // different surface hierarchy.
    uint64_t source_id = current_begin_frame_args_.source_id;
    uint64_t sequence_number = current_begin_frame_args_.sequence_number;
    if (!state.last_args.IsValid() || state.last_args.source_id != source_id ||
        state.last_args.sequence_number != sequence_number) {
      continue;
    }

    // Surface is ready if it has acknowledged the current BeginFrame.
    if (state.last_ack.source_id == source_id &&
        state.last_ack.sequence_number == sequence_number) {
      continue;
    }

    // Surface is ready if there is an unacked active CompositorFrame, because
    // its producer is CompositorFrameAck throttled.
    if (client_->SurfaceHasUnackedFrame(entry.first))
      continue;

    has_pending_surfaces_ = true;
    TRACE_EVENT_INSTANT2("viz", "DisplayScheduler::UpdateHasPendingSurfaces",
                         TRACE_EVENT_SCOPE_THREAD, "has_pending_surfaces",
                         has_pending_surfaces_, "pending_surface_id",
                         surface_id.ToString());
    return has_pending_surfaces_ != old_value;
  }
  has_pending_surfaces_ = false;
  TRACE_EVENT_INSTANT1("viz", "DisplayScheduler::UpdateHasPendingSurfaces",
                       TRACE_EVENT_SCOPE_THREAD, "has_pending_surfaces",
                       has_pending_surfaces_);
  return has_pending_surfaces_ != old_value;
}

void DisplayScheduler::OutputSurfaceLost() {
  TRACE_EVENT0("viz", "DisplayScheduler::OutputSurfaceLost");
  output_surface_lost_ = true;
  ScheduleBeginFrameDeadline();
}

bool DisplayScheduler::DrawAndSwap() {
  TRACE_EVENT0("viz", "DisplayScheduler::DrawAndSwap");
  DCHECK_LT(pending_swaps_, max_pending_swaps_);
  DCHECK(!output_surface_lost_);

  bool success = client_ && client_->DrawAndSwap();
  if (!success)
    return false;

  needs_draw_ = false;
  return true;
}

bool DisplayScheduler::OnBeginFrameDerivedImpl(const BeginFrameArgs& args) {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT2("viz", "DisplayScheduler::BeginFrame", "args", args.AsValue(),
               "now", now);

  if (inside_surface_damaged_) {
    // Repost this so that we don't run a missed BeginFrame on the same
    // callstack. Otherwise we end up running unexpected scheduler actions
    // immediately while inside some other action (such as submitting a
    // CompositorFrame for a SurfaceFactory).
    DCHECK_EQ(args.type, BeginFrameArgs::MISSED);
    DCHECK(missed_begin_frame_task_.IsCancelled());
    missed_begin_frame_task_.Reset(base::BindOnce(
        base::IgnoreResult(&DisplayScheduler::OnBeginFrameDerivedImpl),
        // The CancelableCallback will not run after it is destroyed, which
        // happens when |this| is destroyed.
        base::Unretained(this), args));
    task_runner_->PostTask(FROM_HERE, missed_begin_frame_task_.callback());
    return true;
  }

  // Save the |BeginFrameArgs| as the callback (missed_begin_frame_task_) can be
  // destroyed if we StopObservingBeginFrames(), and it would take the |args|
  // with it. Instead save the args and cancel the |missed_begin_frame_task_|.
  BeginFrameArgs save_args = args;
  // If we get another BeginFrame before a posted missed frame, just drop the
  // missed frame. Also if this was the missed frame, drop the Callback inside
  // it.
  missed_begin_frame_task_.Cancel();

  // If we get another BeginFrame before the previous deadline,
  // synchronously trigger the previous deadline before progressing.
  if (inside_begin_frame_deadline_interval_)
    OnBeginFrameDeadline();

  // Schedule the deadline.
  current_begin_frame_args_ = save_args;
  current_begin_frame_args_.deadline -=
      BeginFrameArgs::DefaultEstimatedDisplayDrawTime(save_args.interval);
  inside_begin_frame_deadline_interval_ = true;
  UpdateHasPendingSurfaces();
  ScheduleBeginFrameDeadline();

  return true;
}

void DisplayScheduler::SetNeedsOneBeginFrame() {
  // If we are not currently observing BeginFrames because needs_draw_ is false,
  // we will stop observing again after one BeginFrame in AttemptDrawAndSwap().
  StartObservingBeginFrames();
}

void DisplayScheduler::MaybeStartObservingBeginFrames() {
  if (ShouldDraw())
    StartObservingBeginFrames();
}

void DisplayScheduler::StartObservingBeginFrames() {
  if (!observing_begin_frame_source_) {
    begin_frame_source_->AddObserver(this);
    observing_begin_frame_source_ = true;
  }
}

void DisplayScheduler::StopObservingBeginFrames() {
  if (observing_begin_frame_source_) {
    begin_frame_source_->RemoveObserver(this);
    observing_begin_frame_source_ = false;

    // A missed BeginFrame may be queued, so drop that too if we're going to
    // stop listening.
    missed_begin_frame_task_.Cancel();
  }
}

bool DisplayScheduler::ShouldDraw() const {
  // Note: When any of these cases becomes true, MaybeStartObservingBeginFrames
  // must be called to ensure the draw will happen.
  return needs_draw_ && !output_surface_lost_ && visible_ &&
         !root_frame_missing_;
}

void DisplayScheduler::OnBeginFrameSourcePausedChanged(bool paused) {
  // BeginFrameSources used with DisplayScheduler do not make use of this
  // feature.
  if (paused)
    NOTIMPLEMENTED();
}

void DisplayScheduler::OnFirstSurfaceActivation(
    const SurfaceInfo& surface_info) {}

void DisplayScheduler::OnSurfaceActivated(
    const SurfaceId& surface_id,
    base::Optional<base::TimeDelta> duration) {}

void DisplayScheduler::OnSurfaceMarkedForDestruction(
    const SurfaceId& surface_id) {
  auto it = surface_states_.find(surface_id);
  if (it == surface_states_.end())
    return;
  surface_states_.erase(it);
  if (UpdateHasPendingSurfaces())
    ScheduleBeginFrameDeadline();
}

bool DisplayScheduler::OnSurfaceDamaged(const SurfaceId& surface_id,
                                        const BeginFrameAck& ack) {
  bool damaged = client_ && client_->SurfaceDamaged(surface_id, ack);
  ProcessSurfaceDamage(surface_id, ack, damaged);

  return damaged;
}

void DisplayScheduler::OnSurfaceDestroyed(const SurfaceId& surface_id) {
  if (client_)
    client_->SurfaceDestroyed(surface_id);
}

void DisplayScheduler::OnSurfaceDamageExpected(const SurfaceId& surface_id,
                                               const BeginFrameArgs& args) {
  TRACE_EVENT1("viz", "DisplayScheduler::SurfaceDamageExpected", "surface_id",
               surface_id.ToString());
  // Insert a new state for the surface if we don't know of it yet. We don't use
  // OnSurfaceCreated for this, because it may not be called if a
  // CompositorFrameSinkSupport starts submitting frames to a different Display,
  // but continues using the same Surface, or if a Surface does not activate its
  // first CompositorFrame immediately.
  surface_states_[surface_id].last_args = args;
  if (UpdateHasPendingSurfaces())
    ScheduleBeginFrameDeadline();
}

base::TimeTicks DisplayScheduler::DesiredBeginFrameDeadlineTime() const {
  switch (AdjustedBeginFrameDeadlineMode()) {
    case BeginFrameDeadlineMode::kImmediate:
      return base::TimeTicks();
    case BeginFrameDeadlineMode::kRegular:
      return current_begin_frame_args_.deadline;
    case BeginFrameDeadlineMode::kLate:
      return current_begin_frame_args_.frame_time +
             current_begin_frame_args_.interval;
    case BeginFrameDeadlineMode::kNone:
      return base::TimeTicks::Max();
    default:
      NOTREACHED();
      return base::TimeTicks();
  }
}

DisplayScheduler::BeginFrameDeadlineMode
DisplayScheduler::AdjustedBeginFrameDeadlineMode() const {
  BeginFrameDeadlineMode mode = DesiredBeginFrameDeadlineMode();

  // In blocking mode, late and regular deadline should not apply. Wait
  // indefinitely instead.
  if (wait_for_all_surfaces_before_draw_ &&
      (mode == BeginFrameDeadlineMode::kRegular ||
       mode == BeginFrameDeadlineMode::kLate)) {
    return BeginFrameDeadlineMode::kNone;
  }

  return mode;
}

DisplayScheduler::BeginFrameDeadlineMode
DisplayScheduler::DesiredBeginFrameDeadlineMode() const {
  if (output_surface_lost_) {
    TRACE_EVENT_INSTANT0("viz", "Lost output surface",
                         TRACE_EVENT_SCOPE_THREAD);
    return BeginFrameDeadlineMode::kImmediate;
  }

  if (pending_swaps_ >= max_pending_swaps_) {
    TRACE_EVENT_INSTANT0("viz", "Swap throttled", TRACE_EVENT_SCOPE_THREAD);
    return BeginFrameDeadlineMode::kLate;
  }

  if (root_frame_missing_) {
    TRACE_EVENT_INSTANT0("viz", "Root frame missing", TRACE_EVENT_SCOPE_THREAD);
    return BeginFrameDeadlineMode::kLate;
  }

  bool all_surfaces_ready = !has_pending_surfaces_ &&
                            root_surface_id_.is_valid() &&
                            !expecting_root_surface_damage_because_of_resize_;

  // When no draw is needed, only allow an early deadline in full-pipe mode.
  // This way, we can unblock the BeginFrame in full-pipe mode if no draw is
  // necessary, but accommodate damage as a result of missed BeginFrames from
  // clients otherwise.
  bool allow_early_deadline_without_draw = wait_for_all_surfaces_before_draw_;

  if (all_surfaces_ready &&
      (needs_draw_ || allow_early_deadline_without_draw)) {
    TRACE_EVENT_INSTANT0("viz", "All active surfaces ready",
                         TRACE_EVENT_SCOPE_THREAD);
    return BeginFrameDeadlineMode::kImmediate;
  }

  if (!needs_draw_) {
    TRACE_EVENT_INSTANT0("viz", "No damage yet", TRACE_EVENT_SCOPE_THREAD);
    return BeginFrameDeadlineMode::kLate;
  }

  // TODO(mithro): Be smarter about resize deadlines.
  if (expecting_root_surface_damage_because_of_resize_) {
    TRACE_EVENT_INSTANT0("viz", "Entire display damaged",
                         TRACE_EVENT_SCOPE_THREAD);
    return BeginFrameDeadlineMode::kLate;
  }

  TRACE_EVENT_INSTANT0("viz", "More damage expected soon",
                       TRACE_EVENT_SCOPE_THREAD);
  return BeginFrameDeadlineMode::kRegular;
}

void DisplayScheduler::ScheduleBeginFrameDeadline() {
  TRACE_EVENT0("viz", "DisplayScheduler::ScheduleBeginFrameDeadline");

  // We need to wait for the next BeginFrame before scheduling a deadline.
  if (!inside_begin_frame_deadline_interval_) {
    TRACE_EVENT_INSTANT0("viz", "Waiting for next BeginFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    DCHECK(begin_frame_deadline_task_.IsCancelled());
    return;
  }

  // Determine the deadline we want to use.
  base::TimeTicks desired_deadline = DesiredBeginFrameDeadlineTime();

  // Avoid re-scheduling the deadline if it's already correctly scheduled.
  if (!begin_frame_deadline_task_.IsCancelled() &&
      desired_deadline == begin_frame_deadline_task_time_) {
    TRACE_EVENT_INSTANT0("viz", "Using existing deadline",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // Schedule the deadline.
  begin_frame_deadline_task_time_ = desired_deadline;
  begin_frame_deadline_task_.Cancel();

  if (begin_frame_deadline_task_time_ == base::TimeTicks::Max()) {
    TRACE_EVENT_INSTANT0("viz", "Using infinite deadline",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  begin_frame_deadline_task_.Reset(begin_frame_deadline_closure_);
  base::TimeDelta delta =
      std::max(base::TimeDelta(), desired_deadline - base::TimeTicks::Now());
  task_runner_->PostDelayedTask(FROM_HERE,
                                begin_frame_deadline_task_.callback(), delta);
  TRACE_EVENT2("viz", "Using new deadline", "delta", delta.ToInternalValue(),
               "desired_deadline", desired_deadline);
}

bool DisplayScheduler::AttemptDrawAndSwap() {
  inside_begin_frame_deadline_interval_ = false;
  begin_frame_deadline_task_.Cancel();
  begin_frame_deadline_task_time_ = base::TimeTicks();

  if (ShouldDraw()) {
    if (pending_swaps_ < max_pending_swaps_)
      return DrawAndSwap();
  } else {
    // We are going idle, so reset expectations.
    // TODO(eseckler): Should we avoid going idle if
    // |expecting_root_surface_damage_because_of_resize_| is true?
    expecting_root_surface_damage_because_of_resize_ = false;

    StopObservingBeginFrames();
  }
  return false;
}

void DisplayScheduler::OnBeginFrameDeadline() {
  TRACE_EVENT0("viz", "DisplayScheduler::OnBeginFrameDeadline");
  DCHECK(inside_begin_frame_deadline_interval_);

  bool did_draw = AttemptDrawAndSwap();
  DidFinishFrame(did_draw);
}

void DisplayScheduler::DidFinishFrame(bool did_draw) {
  DCHECK(begin_frame_source_);
  begin_frame_source_->DidFinishFrame(this);
  BeginFrameAck ack(current_begin_frame_args_, did_draw);
  if (client_)
    client_->DidFinishFrame(ack);
}

void DisplayScheduler::DidSwapBuffers() {
  pending_swaps_++;
  if (pending_swaps_ == max_pending_swaps_)
    begin_frame_source_->SetIsGpuBusy(true);

  uint32_t swap_id = next_swap_id_++;
  TRACE_EVENT_ASYNC_BEGIN0("viz", "DisplayScheduler:pending_swaps", swap_id);
}

void DisplayScheduler::DidReceiveSwapBuffersAck() {
  uint32_t swap_id = next_swap_id_ - pending_swaps_;
  pending_swaps_--;

  // It is important to call this after updating |pending_swaps_| above to
  // ensure any callback from BeginFrameSource observes the correct swap
  // throttled state.
  begin_frame_source_->SetIsGpuBusy(false);
  TRACE_EVENT_ASYNC_END0("viz", "DisplayScheduler:pending_swaps", swap_id);
  ScheduleBeginFrameDeadline();
}

}  // namespace viz

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_scheduler.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/delay_policy.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/service/performance_hint/hint_session.h"

namespace viz {

namespace {

base::TimeDelta ComputeAdpfTarget(const BeginFrameArgs& args) {
  if (args.possible_deadlines) {
    const auto& deadline = args.possible_deadlines->GetPreferredDeadline();
    // Arbitrarily use 75% of the deadline for CPU work.
    return deadline.latch_delta * 3 / 4;
  }
  return base::Milliseconds(12);
}

bool DrawImmediatelyWhenInteractive() {
  return features::ShouldDrawImmediatelyWhenInteractive();
}

}  // namespace

class DisplayScheduler::BeginFrameObserver : public BeginFrameObserverBase {
 public:
  explicit BeginFrameObserver(DisplayScheduler* scheduler)
      : scheduler_(scheduler) {
    // The DisplayScheduler handles animate_only BeginFrames as if they were
    // normal BeginFrames: Clients won't commit a CompositorFrame but will still
    // acknowledge when they have completed the BeginFrame via BeginFrameAcks
    // and the DisplayScheduler will still indicate when all clients have
    // finished via DisplayObserver::OnDisplayDidFinishFrame.
    wants_animate_only_begin_frames_ = true;
  }
  // BeginFrameObserverBase implementation.
  void OnBeginFrameSourcePausedChanged(bool paused) override {
    // TODO(crbug.com/40663506): DisplayScheduler doesn't handle
    // BeginFrameSource pause but it can happen on WebXR.
    if (paused) {
      NOTIMPLEMENTED();
    }
  }

  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override {
    return scheduler_->OnBeginFrame(args);
  }

 private:
  const raw_ptr<DisplayScheduler> scheduler_;
};

DisplayScheduler::DisplayScheduler(BeginFrameSource* begin_frame_source,
                                   base::SingleThreadTaskRunner* task_runner,
                                   PendingSwapParams pending_swap_params,
                                   HintSessionFactory* hint_session_factory,
                                   bool wait_for_all_surfaces_before_draw)
    : begin_frame_observer_(std::make_unique<BeginFrameObserver>(this)),
      begin_frame_source_(begin_frame_source),
      task_runner_(task_runner),
      inside_surface_damaged_(false),
      visible_(false),
      output_surface_lost_(false),
      inside_begin_frame_deadline_interval_(false),
      needs_draw_(false),
      has_pending_surfaces_(false),
      next_swap_id_(1),
      pending_swaps_(0),
      pending_swap_params_(std::move(pending_swap_params)),
      wait_for_all_surfaces_before_draw_(wait_for_all_surfaces_before_draw),
      observing_begin_frame_source_(false),
      hint_session_factory_(hint_session_factory) {
  begin_frame_deadline_timer_.SetTaskRunner(task_runner);
  begin_frame_deadline_closure_ = base::BindRepeating(
      &DisplayScheduler::OnBeginFrameDeadline, weak_ptr_factory_.GetWeakPtr());
}

DisplayScheduler::~DisplayScheduler() {
  // It is possible for DisplayScheduler to be destroyed while there's an
  // in-flight swap. So always mark the gpu as not busy during destruction.
  begin_frame_source_->SetIsGpuBusy(false);
  StopObservingBeginFrames();
}

void DisplayScheduler::SetDamageTracker(DisplayDamageTracker* damage_tracker) {
  DisplaySchedulerBase::SetDamageTracker(damage_tracker);
  damage_tracker->SetDisplayBeginFrameSourceId(
      begin_frame_source_->source_id());
}

void DisplayScheduler::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }

  visible_ = visible;
  // If going invisible, we'll stop observing begin frames once we try
  // to draw and fail.
  MaybeStartObservingBeginFrames();
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::OnRootFrameMissing(bool missing) {
  MaybeStartObservingBeginFrames();
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::OnDisplayDamaged(SurfaceId surface_id) {
  // We may cause a new BeginFrame to be run inside this method, but to help
  // avoid being reentrant to the caller of SurfaceDamaged, track when this is
  // happening with |inside_surface_damaged_|.
  base::AutoReset<bool> auto_reset(&inside_surface_damaged_, true);

  needs_draw_ = true;
  MaybeStartObservingBeginFrames();
  UpdateHasPendingSurfaces();
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::OnPendingSurfacesChanged() {
  if (UpdateHasPendingSurfaces())
    ScheduleBeginFrameDeadline();
}

base::TimeDelta DisplayScheduler::GetDeadlineOffset(
    base::TimeDelta interval) const {
  return BeginFrameArgs::DefaultEstimatedDisplayDrawTime(interval);
}

// This is used to force an immediate swap before a resize.
void DisplayScheduler::ForceImmediateSwapIfPossible() {
  TRACE_EVENT0("viz", "DisplayScheduler::ForceImmediateSwapIfPossible");
  bool in_begin = inside_begin_frame_deadline_interval_;
  bool did_draw = AttemptDrawAndSwap();
  if (in_begin)
    DidFinishFrame(did_draw);
}

bool DisplayScheduler::UpdateHasPendingSurfaces() {
  // If we're not currently inside a deadline interval, we will call
  // UpdateHasPendingSurfaces() again during OnBeginFrameImpl().
  if (!inside_begin_frame_deadline_interval_ || !client_)
    return false;

  bool old_value = has_pending_surfaces_;
  has_pending_surfaces_ =
      damage_tracker_->HasPendingSurfaces(current_begin_frame_args_);
  return has_pending_surfaces_ != old_value;
}

void DisplayScheduler::OutputSurfaceLost() {
  TRACE_EVENT0("viz", "DisplayScheduler::OutputSurfaceLost");
  output_surface_lost_ = true;
  ScheduleBeginFrameDeadline();
}

void DisplayScheduler::MaybeCreateHintSession(
    base::flat_set<base::PlatformThreadId> thread_ids) {
  if (!hint_session_factory_)
    return;

  if ((!create_session_for_current_thread_ids_failed_ && !hint_session_) ||
      current_thread_ids_ != thread_ids) {
    hint_session_.reset();
    current_thread_ids_ = std::move(thread_ids);
    hint_session_ = hint_session_factory_->CreateSession(
        current_thread_ids_, ComputeAdpfTarget(current_begin_frame_args_));
    create_session_for_current_thread_ids_failed_ = !hint_session_;
  }
}

void DisplayScheduler::ReportFrameTime(
    base::TimeDelta frame_time,
    base::flat_set<base::PlatformThreadId> thread_ids,
    base::TimeTicks draw_start,
    HintSession::BoostType boost_type) {
  MaybeCreateHintSession(std::move(thread_ids));
  if (hint_session_) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Compositing.Display.AdpfHintUs",
                                            frame_time, base::Microseconds(1),
                                            base::Milliseconds(50), 50);
    hint_session_->ReportCpuCompletionTime(frame_time, draw_start, boost_type);
  }
}

bool DisplayScheduler::DrawAndSwap() {
  TRACE_EVENT0("viz", "DisplayScheduler::DrawAndSwap");
  DCHECK_LT(pending_swaps_,
            std::max(pending_swap_params_.max_pending_swaps,
                     pending_swap_params_.max_pending_swaps_120hz.value_or(0)));
  DCHECK(!output_surface_lost_);

  DrawAndSwapParams params{current_begin_frame_args_.frame_time,
                           current_frame_display_time(), MaxPendingSwaps()};
  if (current_begin_frame_args_.possible_deadlines) {
    params.choreographer_vsync_id =
        current_begin_frame_args_.possible_deadlines->GetPreferredDeadline()
            .vsync_id;
  }
  bool success = client_ && client_->DrawAndSwap(params);
  if (!success)
    return false;

  needs_draw_ = false;
  return true;
}

bool DisplayScheduler::OnBeginFrame(const BeginFrameArgs& args) {
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
    missed_begin_frame_task_.Reset(
        base::BindOnce(base::IgnoreResult(&DisplayScheduler::OnBeginFrame),
                       // The CancelableOnceCallback will not run after it is
                       // destroyed, which happens when |this| is destroyed.
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

  // Although this always runs "in sequence" with the VizCompositorThread, it
  // may run on another thread (see use of `RunOrPostTask` in
  // `ExternalBeginFrameSourceWin`). To keep that other thread responsive, run
  // the continuation on the VizCompositorThread if a potentially expensive call
  // to `OnBeginFrameDeadline()` is needed.
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  if (inside_begin_frame_deadline_interval_ &&
      !task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DisplayScheduler::OnBeginFrameContinuation,
                                  weak_ptr_factory_.GetWeakPtr(), save_args));
  } else {
    OnBeginFrameContinuation(save_args);
  }

  return true;
}

void DisplayScheduler::OnBeginFrameContinuation(const BeginFrameArgs& args) {
  // If we get another BeginFrame before the previous deadline,
  // synchronously trigger the previous deadline before progressing.
  if (inside_begin_frame_deadline_interval_) {
    OnBeginFrameDeadline();
  }

  // Schedule the deadline.
  current_begin_frame_args_ = args;
  if (hint_session_) {
    hint_session_->UpdateTargetDuration(ComputeAdpfTarget(args));
  }

  current_begin_frame_args_.deadline -=
      BeginFrameArgs::DefaultEstimatedDisplayDrawTime(args.interval);

  inside_begin_frame_deadline_interval_ = true;

  UpdateHasPendingSurfaces();
  ScheduleBeginFrameDeadline();
}

int DisplayScheduler::MaxPendingSwaps() const {
  // Interval for 90hz and 120hz with some delta for margin of error.
  constexpr base::TimeDelta k90HzInterval = base::Microseconds(11500);
  constexpr base::TimeDelta k120HzInterval = base::Microseconds(8500);
  int param_max_pending_swaps;
  if (current_begin_frame_args_.interval < k120HzInterval &&
      pending_swap_params_.max_pending_swaps_120hz) {
    param_max_pending_swaps =
        pending_swap_params_.max_pending_swaps_120hz.value();
  } else if (current_begin_frame_args_.interval < k90HzInterval &&
             pending_swap_params_.max_pending_swaps_90hz) {
    param_max_pending_swaps =
        pending_swap_params_.max_pending_swaps_90hz.value();
  } else {
    param_max_pending_swaps = pending_swap_params_.max_pending_swaps;
  }
  if (!current_begin_frame_args_.possible_deadlines) {
    return param_max_pending_swaps;
  }

  // Estimate the max pending swap based on the frame rate and presentation
  // time.
  const auto& deadline =
      current_begin_frame_args_.possible_deadlines->GetPreferredDeadline();
  int64_t total_time_nanos = deadline.present_delta.InNanoseconds();
  int64_t interval_nanos = current_begin_frame_args_.interval.InNanoseconds();
  // Assuming no frames are dropped, then:
  // * A new frame is started every `interval_nanos`.
  // * A buffer is returned after its present time is passed.
  // This gives us the formula that number of pending swaps needed (ie the
  // max) is the number of new frames that can be started before the buffer
  // for a frame is returned: total_time_nanos / interval_nanos.
  // However present time is generally not an exact multiple of interval, and
  // here the 0.8 constant is chosen to bias rounding up.
  int deadline_max_pending_swaps =
      (total_time_nanos + 0.8 * interval_nanos) / interval_nanos;
  return std::clamp(deadline_max_pending_swaps, 0, param_max_pending_swaps);
}

void DisplayScheduler::SetNeedsOneBeginFrame(bool needs_draw) {
  // If we are not currently observing BeginFrames because needs_draw_ is false,
  // we will stop observing again after one BeginFrame in AttemptDrawAndSwap().
  StartObservingBeginFrames();
  if (needs_draw)
    needs_draw_ = true;
}

void DisplayScheduler::MaybeStartObservingBeginFrames() {
  if (ShouldDraw())
    StartObservingBeginFrames();
}

void DisplayScheduler::StartObservingBeginFrames() {
  if (!observing_begin_frame_source_) {
    begin_frame_source_->AddObserver(begin_frame_observer_.get());
    observing_begin_frame_source_ = true;
  }
}

void DisplayScheduler::StopObservingBeginFrames() {
  if (observing_begin_frame_source_) {
    begin_frame_source_->RemoveObserver(begin_frame_observer_.get());
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
         !damage_tracker_->root_frame_missing();
}

// static
base::TimeTicks DisplayScheduler::DesiredBeginFrameDeadlineTime(
    BeginFrameDeadlineMode deadline_mode,
    BeginFrameArgs begin_frame_args) {
  switch (deadline_mode) {
    case BeginFrameDeadlineMode::kImmediate:
      return base::TimeTicks();
    case BeginFrameDeadlineMode::kRegular:
      return begin_frame_args.deadline;
    case BeginFrameDeadlineMode::kLate:
      return begin_frame_args.frame_time + begin_frame_args.interval;
    case BeginFrameDeadlineMode::kNone:
      return base::TimeTicks::Max();
    default:
      NOTREACHED_IN_MIGRATION();
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

  const int max_pending_swaps = MaxPendingSwaps();
  if (pending_swaps_ >= max_pending_swaps) {
    TRACE_EVENT_INSTANT2("viz", "Swap throttled", TRACE_EVENT_SCOPE_THREAD,
                         "pending_swaps", pending_swaps_, "max_pending_swaps",
                         max_pending_swaps);
    return BeginFrameDeadlineMode::kLate;
  }

  if (damage_tracker_->root_frame_missing()) {
    TRACE_EVENT_INSTANT0("viz", "Root frame missing", TRACE_EVENT_SCOPE_THREAD);
    return BeginFrameDeadlineMode::kLate;
  }

  // Only wait if we actually have pending surfaces and we're not forcing draw
  // due to an ongoing interaction.
  bool wait_for_pending_surfaces =
      has_pending_surfaces_ && !(DrawImmediatelyWhenInteractive() &&
                                 damage_tracker_->HasDamageDueToInteraction());

  bool all_surfaces_ready =
      !wait_for_pending_surfaces && damage_tracker_->IsRootSurfaceValid() &&
      !damage_tracker_->expecting_root_surface_damage_because_of_resize();

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
  if (damage_tracker_->expecting_root_surface_damage_because_of_resize()) {
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
    DCHECK(!begin_frame_deadline_timer_.IsRunning());
    return;
  }

  // Determine the deadline we want to use.
  BeginFrameDeadlineMode deadline_mode = AdjustedBeginFrameDeadlineMode();
  base::TimeTicks desired_deadline =
      DesiredBeginFrameDeadlineTime(deadline_mode, current_begin_frame_args_);

  // Avoid re-scheduling the deadline if it's already correctly scheduled.
  if (begin_frame_deadline_timer_.IsRunning() &&
      desired_deadline == begin_frame_deadline_task_time_) {
    TRACE_EVENT_INSTANT0("viz", "Using existing deadline",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // Schedule the deadline.
  begin_frame_deadline_task_time_ = desired_deadline;
  begin_frame_deadline_timer_.Stop();

  if (begin_frame_deadline_task_time_ == base::TimeTicks::Max()) {
    TRACE_EVENT_INSTANT0("viz", "Using infinite deadline",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  begin_frame_deadline_timer_.Start(FROM_HERE, desired_deadline,
                                    begin_frame_deadline_closure_,
                                    base::subtle::DelayPolicy::kPrecise);
  TRACE_EVENT2("viz", "Using new deadline", "deadline_mode", deadline_mode,
               "desired_deadline", desired_deadline);
}

bool DisplayScheduler::AttemptDrawAndSwap() {
  inside_begin_frame_deadline_interval_ = false;
  begin_frame_deadline_timer_.Stop();
  begin_frame_deadline_task_time_ = base::TimeTicks();

  if (ShouldDraw()) {
    if (pending_swaps_ < MaxPendingSwaps())
      return DrawAndSwap();
  } else {
    // We are going idle, so reset expectations.
    // TODO(eseckler): Should we avoid going idle if
    // |expecting_root_surface_damage_because_of_resize_| is true?
    damage_tracker_->reset_expecting_root_surface_damage_because_of_resize();

    StopObservingBeginFrames();
  }
  return false;
}

void DisplayScheduler::OnBeginFrameDeadline() {
  TRACE_EVENT0("viz,input.scrolling", "DisplayScheduler::OnBeginFrameDeadline");
  DCHECK(inside_begin_frame_deadline_interval_);

  bool did_draw = AttemptDrawAndSwap();
  DidFinishFrame(did_draw);
}

void DisplayScheduler::DidFinishFrame(bool did_draw) {
  DCHECK(begin_frame_source_);
  begin_frame_source_->DidFinishFrame(begin_frame_observer_.get());
  BeginFrameAck ack(current_begin_frame_args_, did_draw);
  if (client_)
    client_->DidFinishFrame(ack);
  damage_tracker_->DidFinishFrame();
}

void DisplayScheduler::DidSwapBuffers() {
  pending_swaps_++;
  if (pending_swaps_ >= MaxPendingSwaps())
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

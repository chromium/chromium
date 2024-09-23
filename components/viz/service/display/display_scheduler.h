// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_

#include <memory>
#include <optional>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_scheduler_base.h"
#include "components/viz/service/display/pending_swap_params.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class HintSession;
class HintSessionFactory;

class VIZ_SERVICE_EXPORT DisplayScheduler
    : public DisplaySchedulerBase,
      public DynamicBeginFrameDeadlineOffsetSource {
 public:
  // `max_pending_swaps_120hz`, if positive, is used as the number of pending
  // swaps while running at 120hz. Otherwise, this will fallback to
  // `max_pending_swaps`.
  DisplayScheduler(BeginFrameSource* begin_frame_source,
                   base::SingleThreadTaskRunner* task_runner,
                   PendingSwapParams pending_swap_params,
                   HintSessionFactory* hint_session_factory = nullptr,
                   bool wait_for_all_surfaces_before_draw = false);

  DisplayScheduler(const DisplayScheduler&) = delete;
  DisplayScheduler& operator=(const DisplayScheduler&) = delete;

  ~DisplayScheduler() override;

  // DisplaySchedulerBase implementation.
  void SetDamageTracker(DisplayDamageTracker* damage_tracker) override;
  void SetVisible(bool visible) override;
  void ForceImmediateSwapIfPossible() override;
  void SetNeedsOneBeginFrame(bool needs_draw) override;
  void DidSwapBuffers() override;
  void DidReceiveSwapBuffersAck() override;
  void OutputSurfaceLost() override;
  void ReportFrameTime(base::TimeDelta frame_time,
                       base::flat_set<base::PlatformThreadId> thread_ids,
                       base::TimeTicks draw_start,
                       HintSession::BoostType boost_type) override;

  // DisplayDamageTracker::Delegate implementation.
  void OnDisplayDamaged(SurfaceId surface_id) override;
  void OnRootFrameMissing(bool missing) override;
  void OnPendingSurfacesChanged() override;

  // DynamicBeginFrameDeadlineOffsetSource:
  base::TimeDelta GetDeadlineOffset(base::TimeDelta interval) const override;

 protected:
  class BeginFrameObserver;
  class BeginFrameRequestObserverImpl;

  bool OnBeginFrame(const BeginFrameArgs& args);
  void OnBeginFrameContinuation(const BeginFrameArgs& args);
  int MaxPendingSwaps() const;

  base::TimeTicks current_frame_display_time() const {
    return current_begin_frame_args_.frame_time +
           current_begin_frame_args_.interval;
  }

  // These values inidicate how a response to the BeginFrame should be
  // scheduled.
  enum class BeginFrameDeadlineMode {
    // Respond immediately. This means either all clients have responded with a
    // BeginFrameAck so there is nothing to wait for, or DrawAndSwap cannot
    // happen anymore (for example, OutputSurface is lost) and we might as well
    // respond right now.
    kImmediate,
    // Schedule a task at the the end of BeginFrame interval minus the estimated
    // time to run DrawAndSwap. This indicates that all requirements for calling
    // DrawAndSwap are met, but we just want to give clients as much time as
    // possible to send CompositorFrames.
    kRegular,
    // Schedule a response at the end of the BeginFrame interval. This usually
    // indicates that some requirements for calling DrawAndSwap are not
    // currently met (for example, the previous swap is not acked yet) and
    // we would like to wait as long as possible to see if DrawAndSwap becomes
    // possible.
    kLate,
    // A response to the BeginFrame cannot be scheduled right now. This means we
    // have an unlimited deadline and some clients haven't responded to the
    // BeginFrame yet so we need to wait longer.
    kNone
  };

  static base::TimeTicks DesiredBeginFrameDeadlineTime(
      BeginFrameDeadlineMode deadline_mode,
      BeginFrameArgs begin_frame_args);

  BeginFrameDeadlineMode AdjustedBeginFrameDeadlineMode() const;
  BeginFrameDeadlineMode DesiredBeginFrameDeadlineMode() const;
  virtual void ScheduleBeginFrameDeadline();
  bool AttemptDrawAndSwap();
  void OnBeginFrameDeadline();
  bool DrawAndSwap();
  void MaybeStartObservingBeginFrames();
  void StartObservingBeginFrames();
  void StopObservingBeginFrames();
  bool ShouldDraw() const;
  void DidFinishFrame(bool did_draw);
  // Updates |has_pending_surfaces_| and returns whether its value changed.
  bool UpdateHasPendingSurfaces();
  void MaybeCreateHintSession(
      base::flat_set<base::PlatformThreadId> thread_ids);

  std::unique_ptr<BeginFrameObserver> begin_frame_observer_;
  raw_ptr<BeginFrameSource> begin_frame_source_;
  raw_ptr<base::SingleThreadTaskRunner> task_runner_;

  BeginFrameArgs current_begin_frame_args_;
  base::RepeatingClosure begin_frame_deadline_closure_;
  base::DeadlineTimer begin_frame_deadline_timer_;
  base::TimeTicks begin_frame_deadline_task_time_;

  base::CancelableOnceClosure missed_begin_frame_task_;
  bool inside_surface_damaged_;

  bool visible_;
  bool output_surface_lost_;

  bool inside_begin_frame_deadline_interval_;
  bool needs_draw_;
  bool has_pending_surfaces_;

  int next_swap_id_;
  int pending_swaps_;
  const PendingSwapParams pending_swap_params_;
  bool wait_for_all_surfaces_before_draw_;

  bool observing_begin_frame_source_;

  const raw_ptr<HintSessionFactory> hint_session_factory_;
  base::flat_set<base::PlatformThreadId> current_thread_ids_;
  std::unique_ptr<HintSession> hint_session_;
  bool create_session_for_current_thread_ids_failed_ = false;

  base::WeakPtrFactory<DisplayScheduler> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_

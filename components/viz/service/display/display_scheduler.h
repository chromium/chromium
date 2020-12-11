// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_scheduler_base.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class BeginFrameSource;

class VIZ_SERVICE_EXPORT DisplayScheduler : public DisplaySchedulerBase {
 public:
  DisplayScheduler(BeginFrameSource* begin_frame_source,
                   base::SingleThreadTaskRunner* task_runner,
                   int max_pending_swaps,
                   bool wait_for_all_surfaces_before_draw = false);
  ~DisplayScheduler() override;

  // DisplaySchedulerBase implementation.
  void SetVisible(bool visible) override;
  void ForceImmediateSwapIfPossible() override;
  void SetNeedsOneBeginFrame(bool needs_draw) override;
  void DidSwapBuffers() override;
  void DidReceiveSwapBuffersAck() override;
  void OutputSurfaceLost() override;

  // DisplayDamageTrackerObserver implementation.
  void OnDisplayDamaged(SurfaceId surface_id) override;
  void OnRootFrameMissing(bool missing) override;
  void OnPendingSurfacesChanged() override;

 protected:
  class BeginFrameObserver;

  bool OnBeginFrame(const BeginFrameArgs& args);

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
  base::TimeTicks DesiredBeginFrameDeadlineTime() const;
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

  std::unique_ptr<BeginFrameObserver> begin_frame_observer_;
  BeginFrameSource* begin_frame_source_;
  base::SingleThreadTaskRunner* task_runner_;

  BeginFrameArgs current_begin_frame_args_;
  base::RepeatingClosure begin_frame_deadline_closure_;
  base::CancelableOnceClosure begin_frame_deadline_task_;
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
  int max_pending_swaps_;
  bool wait_for_all_surfaces_before_draw_;

  bool observing_begin_frame_source_;

  base::WeakPtrFactory<DisplayScheduler> weak_ptr_factory_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayScheduler);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_SCHEDULER_H_

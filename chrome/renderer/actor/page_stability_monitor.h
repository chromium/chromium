// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_
#define CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/delayed_task_handle.h"
#include "base/time/time.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/renderer/actor/journal.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

class Journal;
class NetworkAndMainThreadStabilityMonitor;
class PageStabilityMetrics;
class PaintStabilityMonitor;

// Helper class for monitoring page stability after tool usage. Its lifetime
// must not outlive the RenderFrame it is observing. This object is single-use,
// i.e. NotifyWhenStable can only be called once.
class PageStabilityMonitor : public content::RenderFrameObserver,
                             public mojom::PageStabilityMonitor {
 public:
  // Constructs the monitor and takes a baseline observation of the document in
  // the given RenderFrame. If `supports_paint_stability` is true, paint
  // stability will be included in page stability heuristics if the `frame`
  // supports it.
  PageStabilityMonitor(content::RenderFrame& frame,
                       bool supports_paint_stability,
                       TaskId task_id,
                       Journal& journal);

  ~PageStabilityMonitor() override;

  // RenderFrameObserver
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFailProvisionalLoad() override;
  void DidSetPageLifecycleState(
      blink::BFCacheStateChange bfcache_change) override;
  void OnDestruct() override;

  // mojom::PageStabilityMonitor:
  // Invokes the given callback when the page is deemed stable enough for an
  // observation to take place or when the document is no longer active.
  //
  // `observation_delay` is the amount of time to wait when observing tool
  // execution before starting to wait for page stability.
  void NotifyWhenStable(base::TimeDelta observation_delay,
                        NotifyWhenStableCallback callback) override;

  void Bind(mojo::PendingReceiver<mojom::PageStabilityMonitor> receiver);

 private:
  friend class PageStabilityMetrics;

  enum class State {
    kInitial,

    // If a tool specifies an execution delay, wait in this state before
    // starting monitoring.
    kMonitorStartDelay,

    // Before starting the monitor, if a navigation is in-progress, wait for it
    // to commit or fail.
    kWaitForNavigation,

    // Entry point into the state machine. Decides which state to start in.
    kStartMonitoring,

    //  The NetworkAndMainThreadStabilityMonitor or PaintStabilityMonitor has
    //  determined that the page stability has been reached. If
    //  `kGlicActorPageStabilityMinWait` is set, the callback passed to
    // NotifyWhenStable() may be delayed until the said amount of time is
    // reached.
    kMonitorCompleted,

    // Timeout state - this just logs and and moves to invoke callback state.
    kTimeout,

    // Delay the callback until the min wait time is reached.
    kDelayCallback,

    // Invoke the callback passed to NotifyWhenStable and cleanup.
    kInvokeCallback,

    // The render frame is about to be deleted (e.g. because of a navigation to
    // a new RenderFrame).
    kRenderFrameGoingAway,

    // The mojo pipeline gets disconnected. This just moves to kDone.
    kMojoDisconnected,

    kDone
  } state_ = State::kInitial;
  static std::string_view StateToString(State state);

  friend std::ostream& operator<<(std::ostream& o,
                                  const PageStabilityMonitor::State& state);

  // Synchronously moves to the given state.
  void MoveToState(State new_state);

  // Returns a closure that synchronously moves to the given state. This avoids
  // the extra scheduling hop of `PostMoveToStateClosure`, which is useful if
  // the closure is already being scheduled to run in a separate task.
  base::OnceClosure MoveToStateClosure(State new_state);

  // Helper that provides a closure that invokes MoveToState with the given
  // State on the default task queue for the sequence that created this object.
  base::OnceClosure PostMoveToStateClosure(
      State new_state,
      base::TimeDelta delay = base::TimeDelta());

  base::OnceCallback<base::DelayedTaskHandle()>
  PostCancelableMoveToStateClosure(State new_state,
                                   base::TimeDelta delay = base::TimeDelta());

  void DCheckStateTransition(State old_state, State new_state);

  void OnPaintStabilityReached();
  void OnNetworkAndMainThreadIdle();
  void OnRenderFrameGoingAway();
  void OnMojoDisconnected();
  void OnTimeout();

  void StopMonitoring();
  void Teardown();

  base::OnceClosure is_stable_callback_;

  std::unique_ptr<Journal::PendingAsyncEntry> journal_entry_;

  // Amount of time to delay before monitoring begins.
  base::TimeDelta monitoring_start_delay_;

  // The time at which monitoring begins.
  base::TimeTicks start_monitoring_time_;

  // A navigation may commit while waiting to start monitoring. Cancel the task
  // and don't move to `kStartMonitoring` when the delay expires in this case.
  base::DelayedTaskHandle start_monitoring_delayed_handle_;

  // Amount of time to delay before invoking the callback.
  base::TimeDelta callback_invoke_delay_;

  TaskId task_id_;

  base::raw_ref<Journal> journal_;

  std::unique_ptr<PageStabilityMetrics> metrics_;

  // This will be null if paint stability monitoring is disabled, or if we're
  // monitoring an unsupported interaction. This must be destroyed before
  // `journal_entry_` to avoid a dangling pointer.
  std::unique_ptr<PaintStabilityMonitor> paint_stability_monitor_;

  // This must be destroyed before `journal_` to avoid a dangling pointer.
  std::unique_ptr<NetworkAndMainThreadStabilityMonitor>
      network_and_main_thread_stability_monitor_;

  bool render_frame_did_go_away_ = false;

  bool monitoring_complete_ = false;

  mojo::Receiver<mojom::PageStabilityMonitor> receiver_{this};

  base::WeakPtrFactory<PageStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_

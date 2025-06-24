// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_
#define CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_

#include "base/cancelable_callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/renderer/actor/journal.h"
#include "content/public/renderer/render_frame_observer.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace actor {

class ToolBase;

// Helper class for monitoring page stability after tool usage. Its lifetime
// must not outlive the RenderFrame it is observing. This object is single-use,
// i.e. WaitForStable can only be called once.
class PageStabilityMonitor : public content::RenderFrameObserver {
 public:
  // Constructs the monitor and takes a baseline observation of the document in
  // the given RenderFrame.
  explicit PageStabilityMonitor(content::RenderFrame& frame);
  ~PageStabilityMonitor() override;

  // Invokes the given callback when the page is deemed stable enough for an
  // observation to take place or when the document is no longer active.
  void WaitForStable(const ToolBase& tool,
                     int32_t task_id,
                     Journal& journal,
                     base::OnceClosure callback);

  // RenderFrameObserver
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFailProvisionalLoad() override;
  void OnDestruct() override;

 private:
  enum class State {
    kInitial,

    // If a tool specifies an execution delay, wait in this state before
    // starting monitoring.
    kMonitorStartDelay,

    // Entry point into the state machine. Decides which state to start in.
    kStartMonitoring,

    // A navigation was started, wait for it to commit or cancel.
    kWaitForNavigation,

    // Wait until all network requests complete.
    kWaitForNetworkIdle,

    // Wait until the main thread is settled.
    kWaitForMainThreadIdle,

    // Wait until a new frame has been submitted to and presented by the display
    // compositor.
    kWaitForVisualStateRequest,

    // Timeout states - these just log and and move to invoke callback state.
    kTimeoutGlobal,
    kTimeoutMainThread,

    // Invoke the callback passed to WaitForStable and cleanup.
    kInvokeCallback,

    kDone
  } state_ = State::kInitial;
  friend std::ostream& operator<<(std::ostream& o,
                                  const PageStabilityMonitor::State& state);

  // Synchronously moves to the given state.
  void MoveToState(State new_state);

  // Helper that provides a closure that invokes MoveToState with the given
  // State on the default task queue for the sequence that created this object.
  base::OnceClosure PostMoveToStateClosure(
      State new_state,
      base::TimeDelta delay = base::TimeDelta());

  void SetTimeout(State timeout_type, base::TimeDelta delay);

  void DCheckStateTransition(State old_state, State new_state);

  // The number of active network requests at the time this object was
  // initialized. Used to compare to the number of requests after monitoring
  // begins to determine if new network requests were started in that interval.
  int starting_request_count_;

  // Track the callback given to the RequestNetworkIdle method so that it can be
  // canceled, the API supports only one request at a time.
  base::CancelableOnceClosure network_idle_callback_;

  base::OnceClosure is_stable_callback_;

  std::unique_ptr<Journal::PendingAsyncEntry> journal_entry_;

  // Amount of time to delay before monitoring begins.
  base::TimeDelta monitoring_start_delay_;

  base::WeakPtrFactory<PageStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_PAGE_STABILITY_MONITOR_H_

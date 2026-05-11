// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/delayed_task_handle.h"
#include "base/time/time.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/page_content_annotations/core/page_stability_state.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace page_content_annotations {

class NetworkAndMainThreadStabilityMonitor;
class PageStabilityMonitorDelegate;
class PaintStabilityMonitor;

// Helper class for monitoring page stability. Its lifetime is tied to the
// RenderFrame it observes. This object is single-use: NotifyWhenStable() can
// only be called once.
class PageStabilityMonitor : public content::RenderFrameObserver,
                             public mojom::PageStabilityMonitor {
 public:
  // Constructs the monitor and takes a baseline observation of the document in
  // the given RenderFrame. If `supports_paint_stability` is true, paint
  // stability will be included in page stability heuristics if the `frame`
  // supports it.
  PageStabilityMonitor(content::RenderFrame& frame,
                       bool supports_paint_stability,
                       std::unique_ptr<PageStabilityMonitorDelegate> delegate);

  ~PageStabilityMonitor() override;

  // RenderFrameObserver:
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFailProvisionalLoad() override;
  void DidSetPageLifecycleState(
      blink::BFCacheStateChange bfcache_change) override;
  void OnDestruct() override;

  // mojom::PageStabilityMonitor:
  // Invokes the given callback when the page is deemed stable enough for an
  // observation to take place or when the document is no longer active.
  //
  // `observation_delay` is the amount of time to wait before starting to wait
  // for page stability.
  void NotifyWhenStable(base::TimeDelta observation_delay,
                        NotifyWhenStableCallback callback) override;

  void Bind(mojo::PendingReceiver<mojom::PageStabilityMonitor> receiver);

 private:
  // Synchronously moves to the given state.
  void MoveToState(PageStabilityState new_state);

  // Returns a closure that synchronously moves to the given state. This avoids
  // the extra scheduling hop of `PostMoveToStateClosure`, which is useful if
  // the closure is already being scheduled to run in a separate task.
  base::OnceClosure MoveToStateClosure(PageStabilityState new_state);

  // Helper that provides a closure that invokes MoveToState with the given
  // State on the default task queue for the sequence that created this object.
  base::OnceClosure PostMoveToStateClosure(
      PageStabilityState new_state,
      base::TimeDelta delay = base::TimeDelta());

  base::OnceCallback<base::DelayedTaskHandle()>
  PostCancelableMoveToStateClosure(PageStabilityState new_state,
                                   base::TimeDelta delay = base::TimeDelta());

  void DCheckStateTransition(PageStabilityState old_state,
                             PageStabilityState new_state);

  void OnPaintStabilityReached();
  void OnNetworkAndMainThreadIdle();
  void OnRenderFrameGoingAway();
  void OnMojoDisconnected();
  void OnTimeout();

  void StopMonitoring();
  void Teardown();

  PageStabilityState state_ = PageStabilityState::kInitial;

  NotifyWhenStableCallback is_stable_callback_;

  // Amount of time to delay before monitoring begins.
  base::TimeDelta monitoring_start_delay_;

  // The time at which monitoring begins.
  base::TimeTicks start_monitoring_time_;

  // A navigation may commit while waiting to start monitoring. Cancel the task
  // and don't move to `kStartMonitoring` when the delay expires in this case.
  base::DelayedTaskHandle start_monitoring_delayed_handle_;

  // Amount of time to delay before invoking the callback.
  base::TimeDelta callback_invoke_delay_;

  std::unique_ptr<PageStabilityMonitorDelegate> delegate_;

  // This will be null if we're monitoring an unsupported interaction.
  std::unique_ptr<PaintStabilityMonitor> paint_stability_monitor_;

  std::unique_ptr<NetworkAndMainThreadStabilityMonitor>
      network_and_main_thread_stability_monitor_;

  bool render_frame_did_go_away_ = false;

  bool monitoring_complete_ = false;

  mojo::Receiver<mojom::PageStabilityMonitor> receiver_{this};

  base::WeakPtrFactory<PageStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_H_

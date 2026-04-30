// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_BROWSER_PAGE_SETTLED_MONITOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_BROWSER_PAGE_SETTLED_MONITOR_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace actor {
class TestObservationDelayController;
}  // namespace actor

namespace content {
class WebContents;
}  // namespace content

namespace page_content_annotations {
// Observes a WebContents and determines when the page has "settled".
// A page is considered settled when:
// 1. It has reached "stability" (network and main thread idle, plus optional
//    paint stability).
// 2. Loading has finished (if it was loading).
// 3. A visual state update has occurred (ensuring the renderer has had a
//    chance to produce a frame).
// 4. (Optional) An LCP (Largest Contentful Paint) has been detected or a
//    timeout has passed.
//
// The monitor provides hooks for a delegate to perform additional waits at each
// milestone.
class PageSettledMonitor : public content::WebContentsObserver {
 public:
  // Internal states of the monitor as it progresses through the settling
  // process.
  // LINT.IfChange(State)
  enum class State {
    kInitial,
    kWaitForPageStability,
    kPageStabilityMonitorDisconnected,
    kWaitForLoadCompletion,
    kWaitForVisualStateUpdate,
    kMaybeDelayForLcp,
    kDelayForLcp,
    kDidTimeout,
    kDone,
  };
  // LINT.ThenChange(//chrome/browser/actor/tools/observation_delay_controller.h:State)

  // Significant points in the process where a delegate can perform additional
  // work before the monitor proceeds.
  enum class Milestone {
    kPageStability,
    kLoadCompletion,
    kVisualStateUpdate,
    kLcpSettled,
  };

  // Specific occurrences within the monitor reported for logging or metrics.
  enum class Event {
    kPageStabilized,
    kMojoDisconnected,
    kLoadCompleted,
    kVisualStateUpdated,
    kVisualStateUpdateSkipped,
  };

  using ReadyCallback = base::OnceClosure;

  struct PageStabilityConfig {
    // Whether to include paint stability in page stability heuristics.
    bool supports_paint_stability = false;
    // The amount of time to wait when observing tool execution before starting
    // to wait for page stability.
    base::TimeDelta start_delay;
  };

  // Delegate interface for feature-specific hooks.
  class Delegate {
   public:
    explicit Delegate(std::optional<PageStabilityConfig> page_stability_config);
    virtual ~Delegate() = default;

    // Creates a renderer-side PageStabilityMonitor.
    virtual mojo::PendingRemote<mojom::PageStabilityMonitor>
    CreatePageStabilityMonitor(content::RenderFrameHost* target_frame) = 0;

    // Called when the monitor transitions to a new internal state.
    virtual void WillMoveToState(State state) {}

    // Called when a milestone is reached. The monitor will wait until
    // `resume_callback` is run before proceeding.
    virtual void OnMilestoneReached(Milestone milestone,
                                    base::OnceClosure resume_callback);

    // Called when significant internal events occur.
    virtual void OnEvent(Event event) {}

    // Whether to exclude ad subframes when checking if the page is loading.
    virtual bool ShouldExcludeAdSubframes() const;

    // Whether to skip visual state updates for tabs that are not visible.
    virtual bool ShouldSkipVisualStateUpdateForHiddenTabs() const;

    // The maximum time the monitor should wait for the page to settle.
    virtual base::TimeDelta GetCompletionTimeout() const;

    // The amount of time to wait for LCP if it hasn't occurred yet.
    virtual base::TimeDelta GetLcpDelay() const;

    // The initial delay before starting the renderer-side monitor.
    virtual base::TimeDelta GetStartDelay() const;

   protected:
    const std::optional<PageStabilityConfig> page_stability_config_;
  };

  static std::string_view StateToString(State state);

  PageSettledMonitor(content::RenderFrameHost* target_frame,
                     std::unique_ptr<Delegate> delegate);
  ~PageSettledMonitor() override;

  // Starts waiting for the page to settle.
  // `callback` is invoked when the page is settled or on timeout.
  void Wait(content::WebContents* web_contents, ReadyCallback callback);

 private:
  friend class actor::TestObservationDelayController;

  // content::WebContentsObserver:
  void DidStopLoading() override;

  void MoveToState(State new_state);
  void DCheckStateTransition(State old_state, State new_state);
  base::OnceClosure MoveToStateClosure(State new_state);
  base::OnceClosure PostMoveToStateClosure(
      State new_state,
      base::TimeDelta delay = base::TimeDelta());

  void OnPageStable();
  void OnMojoDisconnected();
  void OnVisualStateUpdated(bool);
  void OnLcpSettled();

  void NotifyMilestone(Milestone milestone);
  void Resume(Milestone milestone);

  std::unique_ptr<Delegate> delegate_;
  ReadyCallback ready_callback_;
  State state_ = State::kInitial;
  std::optional<Milestone> last_notified_milestone_;
  mojo::Remote<mojom::PageStabilityMonitor> page_stability_monitor_remote_;
  base::WeakPtrFactory<PageSettledMonitor> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_BROWSER_PAGE_SETTLED_MONITOR_H_

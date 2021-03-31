// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_POLICIES_TAB_LOADING_FRAME_NAVIGATION_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_POLICIES_TAB_LOADING_FRAME_NAVIGATION_POLICY_H_

#include "base/callback.h"
#include "base/containers/intrusive_heap.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

namespace performance_manager {
namespace policies {

// The policy half of the TabLoadingFrameNavigation system. The policy engine
// is responsible for deciding which WebContents will be throttled, and within
// a WebContents, which frames. Since the decision has to be made synchronously
// on the UI thread before the PM counterparts exist, this policy exposes
// helper functions for "pulling" those decisions from the policy engine, rather
// than having them "pushed" to the mechanism. Once throttling has started the
// subsequent policy decision of when to stop throttling is pushed to the
// mechanism as is more typical. By default the policy object will connect to
// the TabLoadingFrameNavigationScheduler as its policy mechanism, but this can
// be overridden for tests.
//
// At this moment the policy is the following:
//
// - Only pages using http or https are throttled.
// - Main frames are never throttled.
// - Child frames with the same eTLD+1 as the main frame are not throttled.
// - All other frames are throttled.
// - Throttling continues until the main frame hits LargestContentfulPaint.
//   Unfortunately, we can't know LCP in real time, so we use
//   3 x FirstContentfulPaint as an estimate, with an upper bound of
//   the UMA measures 95th %ile of LCP (both the multiple and the upper bound
//   are configurable via Finch).
//
// See design document:
//
// https://docs.google.com/document/d/141gYr7s2m0rPgzDxzAN0PDMtwT2diHEaa7-sZ0V_Q_M/edit#heading=h.7nki9mck5t64
class TabLoadingFrameNavigationPolicy
    : public FrameNode::ObserverDefaultImpl,
      public GraphOwned,
      public GraphRegisteredImpl<TabLoadingFrameNavigationPolicy>,
      public PageNode::ObserverDefaultImpl {
 public:
  class MechanismDelegate;
  using StopThrottlingCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  TabLoadingFrameNavigationPolicy();
  ~TabLoadingFrameNavigationPolicy() override;

  // Exposes policy decisions to the scheduler. This must be called on the UI
  // thread. If this returns true it is assumed that the |contents| is being
  // actively scheduled by the corresponding mechanism.
  static bool ShouldThrottleWebContents(content::WebContents* contents);

  // Exposes policy decisions to the scheduler. This must be called on the UI
  // thread. If this returns true it is assumed that the |handle| will have a
  // throttle applies by the corresponding mechanism.
  static bool ShouldThrottleNavigation(content::NavigationHandle* handle);

  // Exposed for testing. Only safe to call on the PM thread.
  size_t GetThrottledPageCountForTesting() const { return timeouts_.size(); }
  bool IsTimerRunningForTesting() const { return timeout_timer_.IsRunning(); }
  base::TimeTicks GetPageTimeoutForTesting(const PageNode* page_node) const;

  // Exposed for testing. Can be called on any sequence, as these are
  // initialized at construction and stays constant afterwards.
  base::TimeDelta GetMinTimeoutForTesting() const { return timeout_min_; }
  base::TimeDelta GetMaxTimeoutForTesting() const { return timeout_max_; }
  double GetFCPMultipleForTesting() const { return fcp_multiple_; }
  base::TimeDelta CalculateTimeoutFromFCPForTesting(base::TimeDelta fcp) const {
    return CalculateTimeoutFromFCP(fcp);
  }

  // Exposed for testing. Allows setting a MechanismDelegate. This should be
  // done immediately after construction and *before* passing to the PM graph.
  // Note that the provided mechanism will always be invoked on the UI thread.
  // Note also that it is expected to live until this policy object is taken
  // from the graph. In production the mechanism is backed by a static
  // singleton, but code using this seam must manually ensure lifetime
  // semantics are observed.
  void SetMechanismDelegateForTesting(MechanismDelegate* mechanism) {
    DCHECK(mechanism);
    mechanism_ = mechanism;
  }

  // Exposed for testing. Can only be called on PM sequence.
  void OnFirstContentfulPaintForTesting(
      const FrameNode* frame_node,
      base::TimeDelta time_since_navigation_start) {
    OnFirstContentfulPaint(frame_node, time_since_navigation_start);
  }

 private:
  base::TimeDelta CalculateTimeoutFromFCP(base::TimeDelta fcp) const;

  // PageNodeObserver:
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

  // FrameNodeObserver:
  void OnFirstContentfulPaint(
      const FrameNode* frame_node,
      base::TimeDelta time_since_navigation_start) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // Invoked by ShouldThrottleWebContents logic.
  static void SetPageNodeThrottled(base::WeakPtr<const PageNode> page_node,
                                   bool throttled,
                                   Graph* graph);
  void SetPageNodeThrottledImpl(const PageNode* page_node, bool throttled);

  // Creates a new page timeout. An entry for |page_node| must not already
  // exist.
  void CreatePageTimeout(const PageNode* page_node, base::TimeDelta timeout);

  // Updates the entry for |page_node| if it exists and is for a time later than
  // |timeout|. Otherwise, does nothing.
  void MaybeUpdatePageTimeout(const PageNode* page_node,
                              base::TimeDelta timeout);

  // Erases the entry for |page_node| if it exists.
  void MaybeErasePageTimeout(const PageNode* page_node);

  // Updates the |timeout_timer_| to reflect the current top of the |timeouts_|
  // min-heap, or stopping the timer if the heap is empty.
  void MaybeUpdateTimeoutTimer();

  // Called by the |timeout_timer_| when it fires. This will fire notifications
  // to the WebContents-affiliated scheduler objects on the UI thread, and then
  // reschedule a new timer.
  void OnTimeout();

  // Dispatches StopThrottling notifications for all pages that are currently
  // expired.
  void StopThrottlingExpiredPages();

  // A heap of all page nodes that are currently being throttled, sorted
  // by the time the throttling will timeout. This is intrusive because pages
  // can be removed before popping out of the heap naturally.
  struct Timeout {
    const PageNode* page_node;
    base::TimeTicks timeout;

    // Reverse the comparison operator so this is a min-heap.
    bool operator<(const Timeout& rhs) const { return timeout > rhs.timeout; }

    // Dummy implementation of the IntrusiveHeap API. We don't need external
    // handles into the heap, as we simply look up elements directly and update
    // them in place.
    void SetHeapHandle(base::HeapHandle) {}
    void ClearHeapHandle() {}
    base::HeapHandle GetHeapHandle() const {
      return base::HeapHandle::Invalid();
    }
  };
  base::IntrusiveHeap<Timeout> timeouts_;

  // The timeout after which throttling is stopped. Configured via Finch.
  // See features.cc.
  base::TimeDelta timeout_min_;
  base::TimeDelta timeout_max_;

  // The multiple applied to FCP to calculate the throttle timeout.
  // Configured via Finch. See features.cc.
  double fcp_multiple_;

  // A one shot timer that is used to timeout existing throttles. This will be
  // running whenever |timeouts_| is not empty.
  base::OneShotTimer timeout_timer_;

  // The time that has been set using |timeout_timer_|. When |timeouts_| is
  // empty this is set to "base::TimeTicks::Min()". This is used to know if the
  // current running timer is okay, or if a new timer is needed (helps to
  // minimize abandoned timer tasks). We would use
  // timeout_timer_.desired_run_time(), but this isn't precise.
  base::TimeTicks scheduled_timer_ = base::TimeTicks::Min();

  // The mechanism delegate that this object is using
  MechanismDelegate* mechanism_ = nullptr;
};

class TabLoadingFrameNavigationPolicy::MechanismDelegate {
 public:
  MechanismDelegate() = default;
  virtual ~MechanismDelegate() = default;

  // Notifies the mechanism when it is enabled/disabled. Mechanisms should start
  // in a disabled state, and only start throttling when explicitly enabled.
  // When they are subsequently disabled they should release any outstanding
  // throttles, and stop creating new ones.
  virtual void SetThrottlingEnabled(bool enabled) = 0;

  // Notifies a single |contents| that it should stop throttling the specified
  // |last_navigation_id|. The navigation ID is specified because of the race
  // between navigations on the UI thread and policy messages dispatched from
  // the PM sequence.
  virtual void StopThrottling(content::WebContents* contents,
                              int64_t last_navigation_id) = 0;
};

}  // namespace policies
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_TAB_LOADING_FRAME_NAVIGATION_POLICY_H_

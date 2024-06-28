// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator_data.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;

// The PageLoadTracker decorator is responsible for determining when a page is
// loading. A page starts loading when incoming data starts arriving for a
// top-level load to a different document. It stops loading when it reaches an
// "almost idle" state, based on CPU and network quiescence, or after an
// absolute timeout. This state is then updated on PageNodes in a graph.
class PageLoadTrackerDecorator : public FrameNode::ObserverDefaultImpl,
                                 public GraphOwnedDefaultImpl,
                                 public NodeDataDescriberDefaultImpl,
                                 public ProcessNode::ObserverDefaultImpl {
 public:
  using Data = PageLoadTrackerDecoratorData;

  PageLoadTrackerDecorator();

  PageLoadTrackerDecorator(const PageLoadTrackerDecorator&) = delete;
  PageLoadTrackerDecorator& operator=(const PageLoadTrackerDecorator&) = delete;

  ~PageLoadTrackerDecorator() override;

  // FrameNodeObserver implementation:
  void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // NodeDataDescriber implementation:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  // ProcessNodeObserver implementation:
  void OnMainThreadTaskLoadIsLow(const ProcessNode* process_node) override;

  // Invoked by PageLoadTrackerDecoratorHelper when corresponding
  // WebContentsObserver methods are invoked, and the WebContents is loading to
  // a different document.
  static void DidStartLoading(PageNodeImpl* page_node);
  static void PrimaryPageChanged(PageNodeImpl* page_node);
  static void DidStopLoading(PageNodeImpl* page_node);

 protected:
  friend class PageLoadTrackerDecoratorTest;

  // The amount of time after which a page transitions from
  // kWaitingForNavigation to kWaitingForNavigationTimedOut if the page change
  // hasn't been committed.
  static constexpr base::TimeDelta kWaitingForNavigationTimeout =
      base::Seconds(5);

  // The amount of time a page has to be idle post-loading in order for it to be
  // considered loaded and idle. This is used in UpdateLoadIdleState
  // transitions.
  static constexpr base::TimeDelta kLoadedAndIdlingTimeout = base::Seconds(1);

  // The maximum amount of time post-DidStopLoading a page can be waiting for
  // an idle state to occur before the page is simply considered loaded anyways.
  // Since PageAlmostIdle is intended as an "initial loading complete" signal,
  // it needs to eventually terminate. This is strictly greater than the
  // kLoadedAndIdlingTimeout.
  //
  // This is taken as the 95th percentile of tab loading times on desktop
  // (see SessionRestore.ForegroundTabFirstLoaded). This ensures that all tabs
  // eventually transition to loaded, even if they keep the main task queue
  // busy, or continue loading content.
  static constexpr base::TimeDelta kWaitingForIdleTimeout = base::Minutes(1);

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  // These are called when properties/events affecting the load-idle state are
  // observed. Frame and Process variants will eventually all redirect to the
  // appropriate Page variant, where the real work is done.
  void UpdateLoadIdleStateFrame(FrameNodeImpl* frame_node);
  void UpdateLoadIdleStateProcess(ProcessNodeImpl* process_node);
  static void UpdateLoadIdleStatePage(PageNodeImpl* page_node);

  // Schedules a call to UpdateLoadIdleStatePage() for |page_node| after
  // |delayed_run_time| - |now| has elapsed.
  static void ScheduleDelayedUpdateLoadIdleStatePage(
      PageNodeImpl* page_node,
      base::TimeTicks now,
      base::TimeTicks delayed_run_time);

  // Helper function for transitioning to the final state.
  static void TransitionToLoadedAndIdle(PageNodeImpl* page_node);

  static bool IsIdling(const PageNodeImpl* page_node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_H_

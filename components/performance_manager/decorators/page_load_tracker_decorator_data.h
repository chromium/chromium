// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_DATA_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/performance_manager/graph/node_inline_data.h"

namespace performance_manager {

class PageNodeImpl;

class PageLoadTrackerDecoratorData
    : public SparseNodeInlineData<PageLoadTrackerDecoratorData> {
 public:
  // The state transitions associated with a load. This is more granular than
  // the publicly exposed PageNode::LoadingState, to provide the required
  // details to implement state transitions.
  enum class LoadIdleState {
    // Loading started, but the page change hasn't been committed yet. Can
    // transition to kLoading, kWaitingForNavigationTimedOut or kLoadedAndIdle
    // from here.
    kWaitingForNavigation,
    // Loading started and a timeout has elapsed, but the page change hasn't
    // been committed yet. Can transition to kLoading or kLoadedAndIdle from
    // here.
    kWaitingForNavigationTimedOut,
    // Incoming data has started to arrive for a load. Almost idle signals are
    // ignored in this state. Can transition to kLoadedNotIdling and
    // kLoadedAndIdling from here.
    kLoading,
    // Loading has completed, but the page has not started idling. Can
    // transition to kLoadedAndIdling, kLoadedAndIdle or kWaitingForNavigation
    // from here (the latter occurs when a new load starts before the previous
    // ends).
    kLoadedNotIdling,
    // Loading has completed, and the page is idling. Can transition to
    // kLoadedNotIdling, kLoadedAndIdle or kWaitingForNavigation from here (the
    // latter occurs when a new load starts before the previous ends).
    kLoadedAndIdling,
    // Loading has completed and the page has been idling for sufficiently long
    // or encountered an error. This is the final state. Once this state has
    // been reached a signal will be emitted and no further state transitions
    // will be tracked. Committing a new non-same document navigation can start
    // the cycle over again.
    kLoadedAndIdle
  };

  // Sets the LoadIdleState for the page, and updates PageNode::IsLoading()
  // accordingly.
  void SetLoadIdleState(PageNodeImpl* page_node, LoadIdleState load_idle_state);

  // Returns the LoadIdleState for the page.
  LoadIdleState load_idle_state() const { return load_idle_state_; }

  void Describe(base::Value::Dict* dict);

  // Whether there is an ongoing different-document load, i.e. DidStartLoading()
  // was invoked but not DidStopLoading().
  bool is_loading_ = false;

  // Whether there is an ongoing different-document load for which data started
  // arriving, i.e. both DidStartLoading() and PrimaryPageChanged() were
  // invoked but not DidStopLoading().
  bool did_commit_ = false;

  // Marks the point in time when the state transitioned to
  // kWaitingForNavigation. This is used as the basis for the
  // kWaitingForNavigationTimeout.
  base::TimeTicks loading_started_;

  // Marks the point in time when the DidStopLoading signal was received,
  // transitioning to kLoadedAndNotIdling or kLoadedAndIdling. This is used as
  // the basis for the kWaitingForIdleTimeout.
  base::TimeTicks loading_stopped_;

  // Marks the point in time when the last transition to kLoadedAndIdling
  // occurred. This is used as the basis for the kLoadedAndIdlingTimeout.
  base::TimeTicks idling_started_;

  // A one-shot timer used to transition state after a timeout.
  base::OneShotTimer timer_;

 private:
  // Initially at kWaitingForNavigation when a load starts. Transitions
  // through the states via calls to UpdateLoadIdleState.
  LoadIdleState load_idle_state_ = LoadIdleState::kWaitingForNavigation;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_PAGE_LOAD_TRACKER_DECORATOR_DATA_H_

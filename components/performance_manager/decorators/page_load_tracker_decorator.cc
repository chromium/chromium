// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_load_tracker_decorator.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

namespace {

using LoadIdleState = PageLoadTrackerDecorator::Data::LoadIdleState;

const char kDescriberName[] = "PageLoadTrackerDecorator";

}  // namespace

// static
constexpr base::TimeDelta
    PageLoadTrackerDecorator::kWaitingForNavigationTimeout;
constexpr base::TimeDelta PageLoadTrackerDecorator::kLoadedAndIdlingTimeout;
constexpr base::TimeDelta PageLoadTrackerDecorator::kWaitingForIdleTimeout;

PageLoadTrackerDecorator::PageLoadTrackerDecorator() {
  // Ensure the timeouts make sense relative to each other.
  static_assert(kWaitingForIdleTimeout > kLoadedAndIdlingTimeout,
                "timeouts must be well ordered");
}

PageLoadTrackerDecorator::~PageLoadTrackerDecorator() = default;

void PageLoadTrackerDecorator::OnNetworkAlmostIdleChanged(
    const FrameNode* frame_node) {
  UpdateLoadIdleStateFrame(FrameNodeImpl::FromNode(frame_node));
}

void PageLoadTrackerDecorator::OnPassedToGraph(Graph* graph) {
  RegisterObservers(graph);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageLoadTrackerDecorator::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  UnregisterObservers(graph);
}

base::Value::Dict PageLoadTrackerDecorator::DescribePageNodeData(
    const PageNode* page_node) const {
  base::Value::Dict ret;

  PageNodeImpl* page_node_impl = PageNodeImpl::FromNode(page_node);
  if (!Data::Exists(page_node_impl)) {
    return ret;
  }

  Data& data = Data::Get(page_node_impl);
  data.Describe(&ret);

  return ret;
}

void PageLoadTrackerDecorator::OnMainThreadTaskLoadIsLow(
    const ProcessNode* process_node) {
  UpdateLoadIdleStateProcess(ProcessNodeImpl::FromNode(process_node));
}

// static
void PageLoadTrackerDecorator::DidStartLoading(PageNodeImpl* page_node) {
  if (!Data::Exists(page_node)) {
    Data::Create(page_node);
  }

  Data& data = Data::Get(page_node);

  // Typically, |data| is a newly created PageLoadTrackerDecoratorData. However,
  // if a load starts before the page reaches an idle state following the
  // previous load, |data| may indicate that the page is |kLoadedNotIdling| or
  // |kLoadedAndIdling|. In all cases, restart the state machine at
  // |kWaitingForNavigation| and clear the |loading_started_| timestamp.
  DCHECK_NE(data.load_idle_state(),
            LoadIdleState::kWaitingForNavigationTimedOut);
  DCHECK_NE(data.load_idle_state(), LoadIdleState::kLoading);
  DCHECK_NE(data.load_idle_state(), LoadIdleState::kLoadedAndIdle);
  DCHECK(!data.is_loading_);
  DCHECK(!data.did_commit_);
  data.is_loading_ = true;
  data.SetLoadIdleState(page_node, LoadIdleState::kWaitingForNavigation);
  data.loading_started_ = base::TimeTicks();
  UpdateLoadIdleStatePage(page_node);
}

// static
void PageLoadTrackerDecorator::PrimaryPageChanged(PageNodeImpl* page_node) {
  Data& data = Data::Get(page_node);

  DCHECK(data.is_loading_);
  DCHECK(!data.did_commit_);
  data.did_commit_ = true;

  UpdateLoadIdleStatePage(page_node);
}

// static
void PageLoadTrackerDecorator::DidStopLoading(PageNodeImpl* page_node) {
  Data& data = Data::Get(page_node);

  DCHECK(data.is_loading_);
  data.is_loading_ = false;
  data.did_commit_ = false;

  UpdateLoadIdleStatePage(page_node);
}

void PageLoadTrackerDecorator::RegisterObservers(Graph* graph) {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  // TODO(chrisha): Add graph introspection functions to Graph.
  DCHECK(graph->HasOnlySystemNode());
  graph->AddFrameNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void PageLoadTrackerDecorator::UnregisterObservers(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
}

void PageLoadTrackerDecorator::UpdateLoadIdleStateFrame(
    FrameNodeImpl* frame_node) {
  // Only main frames are relevant in the load idle state.
  if (!frame_node->IsMainFrame())
    return;

  // Update the load idle state of the page associated with this frame.
  auto* page_node = frame_node->page_node();
  if (!page_node)
    return;
  UpdateLoadIdleStatePage(page_node);
}

// static
void PageLoadTrackerDecorator::UpdateLoadIdleStatePage(
    PageNodeImpl* page_node) {
  // Once the cycle is complete state transitions are no longer tracked for this
  // page. When this occurs the backing data store is deleted.
  if (!Data::Exists(page_node)) {
    return;
  }

  Data& data = Data::Get(page_node);

  // Cancel any ongoing timers. A new timer will be set if necessary.
  data.timer_.Stop();
  const base::TimeTicks now = base::TimeTicks::Now();

  // If this is a new load, set the start time.
  if (data.loading_started_.is_null()) {
    DCHECK_EQ(data.load_idle_state(), LoadIdleState::kWaitingForNavigation);
    data.loading_started_ = now;
  }

  // Determine if the overall timeout has fired.
  if ((data.load_idle_state() == LoadIdleState::kLoadedNotIdling ||
       data.load_idle_state() == LoadIdleState::kLoadedAndIdling) &&
      (now - data.loading_stopped_) >= kWaitingForIdleTimeout) {
    TransitionToLoadedAndIdle(page_node);
    return;
  }

  // Otherwise do normal state transitions.
  switch (data.load_idle_state()) {
    case LoadIdleState::kWaitingForNavigation: {
      if (now - data.loading_started_ >= kWaitingForNavigationTimeout) {
        data.SetLoadIdleState(page_node,
                              LoadIdleState::kWaitingForNavigationTimedOut);
      }

      [[fallthrough]];
    }

    case LoadIdleState::kWaitingForNavigationTimedOut: {
      if (data.did_commit_) {
        data.SetLoadIdleState(page_node, LoadIdleState::kLoading);
        return;
      }

      if (!data.is_loading_) {
        // Transition to kLoadedAndIdle when load stops without committing a
        // page change.
        TransitionToLoadedAndIdle(page_node);
        return;
      }

      // Schedule a state update to transition to
      // |kWaitingForNavigationTimedOut| when the page has been waiting for the
      // page change commit for too long.
      if (data.load_idle_state() == LoadIdleState::kWaitingForNavigation) {
        ScheduleDelayedUpdateLoadIdleStatePage(
            page_node, now,
            data.loading_started_ + kWaitingForNavigationTimeout);
      }

      return;
    }

    case LoadIdleState::kLoading: {
      if (data.is_loading_) {
        // DidStopLoading() was not invoked yet.
        return;
      }
      // DidStartLoading() -> PrimaryPageChanged() -> DidStopLoading() were all
      // invoked. Wait for the page to become idle.
      data.SetLoadIdleState(page_node, LoadIdleState::kLoadedNotIdling);
      data.loading_stopped_ = now;
      // Let the kLoadedNotIdling state transition evaluate, allowing an
      // immediate transition to kLoadedAndIdling if the page is already idling.
      [[fallthrough]];
    }

    case LoadIdleState::kLoadedNotIdling: {
      if (!IsIdling(page_node)) {
        // Schedule a state update to transition to |kLoadedAndIdle| when the
        // page has been loaded but not idling for too long.
        ScheduleDelayedUpdateLoadIdleStatePage(
            page_node, now, data.loading_stopped_ + kWaitingForIdleTimeout);
        return;
      }

      data.SetLoadIdleState(page_node, LoadIdleState::kLoadedAndIdling);
      data.idling_started_ = now;
      [[fallthrough]];
    }

    case LoadIdleState::kLoadedAndIdling: {
      if (!IsIdling(page_node)) {
        // If the page is not still idling then transition back a state.
        data.SetLoadIdleState(page_node, LoadIdleState::kLoadedNotIdling);
        // Schedule a state update to transition to |kLoadedAndIdle| when the
        // page has been loaded but not idling for too long.
        ScheduleDelayedUpdateLoadIdleStatePage(
            page_node, now, data.loading_stopped_ + kWaitingForIdleTimeout);
        return;
      }

      if (now - data.idling_started_ >= kLoadedAndIdlingTimeout) {
        // Idling has been happening long enough so make the last state
        // transition.
        TransitionToLoadedAndIdle(page_node);
        return;
      }

      // Schedule a state update to transition to |kLoadedAndIdle| when the page
      // has been idling long enough to be considered "loaded and idle".
      ScheduleDelayedUpdateLoadIdleStatePage(
          page_node, now, data.idling_started_ + kLoadedAndIdlingTimeout);
      return;
    }

    // This should never occur.
    case LoadIdleState::kLoadedAndIdle:
      NOTREACHED_IN_MIGRATION();
  }

  // All paths of the switch statement return.
  NOTREACHED_IN_MIGRATION();
}

// static
void PageLoadTrackerDecorator::ScheduleDelayedUpdateLoadIdleStatePage(
    PageNodeImpl* page_node,
    base::TimeTicks now,
    base::TimeTicks delayed_run_time) {
  DCHECK_GE(delayed_run_time, now);

  // |timer| is owned by |page_node| indirectly through |data|. Because of that,
  // the |timer| will be canceled if |page_node| is deleted, making the use of
  // Unretained() safe.
  Data& data = Data::Get(page_node);
  data.timer_.Start(
      FROM_HERE, delayed_run_time - now,
      base::BindRepeating(&PageLoadTrackerDecorator::UpdateLoadIdleStatePage,
                          base::Unretained(page_node)));
}

void PageLoadTrackerDecorator::UpdateLoadIdleStateProcess(
    ProcessNodeImpl* process_node) {
  for (FrameNodeImpl* frame_node : process_node->frame_nodes()) {
    UpdateLoadIdleStateFrame(frame_node);
  }
}

// static
void PageLoadTrackerDecorator::TransitionToLoadedAndIdle(
    PageNodeImpl* page_node) {
  Data& data = Data::Get(page_node);
  data.SetLoadIdleState(page_node, LoadIdleState::kLoadedAndIdle);
  // Destroy the metadata as there are no more transitions possible. The
  // machinery will start up again if a navigation occurs.
  Data::Destroy(page_node);
}

// static
bool PageLoadTrackerDecorator::IsIdling(const PageNodeImpl* page_node) {
  // Get the frame node for the main frame associated with this page.
  const FrameNodeImpl* main_frame_node = page_node->main_frame_node();
  if (!main_frame_node)
    return false;

  // Get the process node associated with this main frame.
  const auto* process_node = main_frame_node->process_node();
  if (!process_node)
    return false;

  // Note that it's possible for one misbehaving frame hosted in the same
  // process as this page's main frame to keep the main thread task low high.
  // In this case the IsIdling signal will be delayed, despite the task load
  // associated with this page's main frame actually being low. In the case
  // of session restore this is mitigated by having a timeout while waiting for
  // this signal.
  return main_frame_node->GetNetworkAlmostIdle() &&
         process_node->GetMainThreadTaskLoadIsLow();
}

}  // namespace performance_manager

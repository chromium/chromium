// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_load_tracker_decorator.h"

#include <algorithm>

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

// Provides PageLoadTracker machinery access to some internals of a
// PageNodeImpl.
class PageLoadTrackerAccess {
 public:
  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return &page_node->GetPageLoadTrackerData(
        base::PassKey<PageLoadTrackerAccess>());
  }
};

namespace {

using LoadIdleState = PageLoadTrackerDecorator::Data::LoadIdleState;

class DataImpl : public PageLoadTrackerDecorator::Data,
                 public NodeAttachedDataImpl<DataImpl> {
 public:
  struct Traits : public NodeAttachedDataOwnedByNodeType<PageNodeImpl> {};

  explicit DataImpl(const PageNodeImpl* page_node) {}
  ~DataImpl() override = default;

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      PageNodeImpl* page_node) {
    return PageLoadTrackerAccess::GetUniquePtrStorage(page_node);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DataImpl);
};

// static
const char* ToString(LoadIdleState state) {
  switch (state) {
    case LoadIdleState::kLoadingNotStarted:
      return "kLoadingNotStarted";
    case LoadIdleState::kLoading:
      return "kLoading";
    case LoadIdleState::kLoadedNotIdling:
      return "kLoadedNotIdling";
    case LoadIdleState::kLoadedAndIdling:
      return "kLoadedAndIdling";
    case LoadIdleState::kLoadedAndIdle:
      return "kLoadedAndIdle";
  }
}

const char kDescriberName[] = "PageLoadTrackerDecorator";

}  // namespace

// static
constexpr base::TimeDelta PageLoadTrackerDecorator::kLoadedAndIdlingTimeout;
// static
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

base::Value PageLoadTrackerDecorator::DescribePageNodeData(
    const PageNode* page_node) const {
  auto* data = DataImpl::Get(PageNodeImpl::FromNode(page_node));
  if (data == nullptr)
    return base::Value();

  base::Value ret(base::Value::Type::DICTIONARY);
  ret.SetStringKey("load_idle_state", ToString(data->load_idle_state()));
  ret.SetBoolKey("loading_received_response", data->loading_received_response_);

  return ret;
}

void PageLoadTrackerDecorator::OnMainThreadTaskLoadIsLow(
    const ProcessNode* process_node) {
  UpdateLoadIdleStateProcess(ProcessNodeImpl::FromNode(process_node));
}

void PageLoadTrackerDecorator::DidReceiveResponse(PageNodeImpl* page_node) {
  auto* data = DataImpl::GetOrCreate(page_node);
  DCHECK(!data->loading_received_response_);
  data->loading_received_response_ = true;
  UpdateLoadIdleStatePage(page_node);
}

void PageLoadTrackerDecorator::DidStopLoading(PageNodeImpl* page_node) {
  auto* data = DataImpl::GetOrCreate(page_node);
  data->loading_received_response_ = false;
  UpdateLoadIdleStatePage(page_node);
}

void PageLoadTrackerDecorator::RegisterObservers(Graph* graph) {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  // TODO(chrisha): Add graph introspection functions to Graph.
  DCHECK(GraphImpl::FromGraph(graph)->nodes().empty());
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
  auto* data = DataImpl::Get(page_node);
  if (data == nullptr)
    return;

  // This is the terminal state, so should never occur.
  DCHECK_NE(LoadIdleState::kLoadedAndIdle, data->load_idle_state());

  // Cancel any ongoing timers. A new timer will be set if necessary.
  data->idling_timer_.Stop();
  const base::TimeTicks now = base::TimeTicks::Now();

  // Determine if the overall timeout has fired.
  if ((data->load_idle_state() == LoadIdleState::kLoadedNotIdling ||
       data->load_idle_state() == LoadIdleState::kLoadedAndIdling) &&
      (now - data->loading_stopped_) >= kWaitingForIdleTimeout) {
    TransitionToLoadedAndIdle(page_node);
    return;
  }

  // Otherwise do normal state transitions.
  switch (data->load_idle_state()) {
    case LoadIdleState::kLoadingNotStarted: {
      if (!data->loading_received_response_)
        return;
      data->SetLoadIdleState(page_node, LoadIdleState::kLoading);
      return;
    }

    case LoadIdleState::kLoading: {
      if (data->loading_received_response_)
        return;
      data->SetLoadIdleState(page_node, LoadIdleState::kLoadedNotIdling);
      data->loading_stopped_ = now;
      // Let the kLoadedNotIdling state transition evaluate, allowing an
      // effective transition directly from kLoading to kLoadedAndIdling.
      FALLTHROUGH;
    }

    case LoadIdleState::kLoadedNotIdling: {
      if (IsIdling(page_node)) {
        data->SetLoadIdleState(page_node, LoadIdleState::kLoadedAndIdling);
        data->idling_started_ = now;
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    case LoadIdleState::kLoadedAndIdling: {
      // If the page is not still idling then transition back a state.
      if (!IsIdling(page_node)) {
        data->SetLoadIdleState(page_node, LoadIdleState::kLoadedNotIdling);
      } else {
        // Idling has been happening long enough so make the last state
        // transition.
        if (now - data->idling_started_ >= kLoadedAndIdlingTimeout) {
          TransitionToLoadedAndIdle(page_node);
          return;
        }
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    // This should never occur.
    case LoadIdleState::kLoadedAndIdle:
      NOTREACHED();
  }

  // Getting here means a new timer needs to be set. Use the nearer of the two
  // applicable timeouts.
  base::TimeDelta timeout =
      (data->loading_stopped_ + kWaitingForIdleTimeout) - now;
  if (data->load_idle_state() == LoadIdleState::kLoadedAndIdling) {
    timeout = std::min(timeout,
                       (data->idling_started_ + kLoadedAndIdlingTimeout) - now);
  }

  // It's safe to use base::Unretained here because the graph owns the timer via
  // PageNodeImpl, and all nodes are destroyed *before* this observer during
  // tear down. By the time the observer is destroyed, the timer will have
  // already been destroyed and the associated posted task canceled.
  data->idling_timer_.Start(
      FROM_HERE, timeout,
      base::BindRepeating(&PageLoadTrackerDecorator::UpdateLoadIdleStatePage,
                          page_node));
}

void PageLoadTrackerDecorator::UpdateLoadIdleStateProcess(
    ProcessNodeImpl* process_node) {
  for (auto* frame_node : process_node->frame_nodes())
    UpdateLoadIdleStateFrame(frame_node);
}

// static
void PageLoadTrackerDecorator::TransitionToLoadedAndIdle(
    PageNodeImpl* page_node) {
  auto* data = DataImpl::Get(page_node);
  data->SetLoadIdleState(page_node, LoadIdleState::kLoadedAndIdle);

  // Destroy the metadata as there are no more transitions possible. The
  // machinery will start up again if a navigation occurs.
  DataImpl::Destroy(page_node);
}

// static
bool PageLoadTrackerDecorator::IsIdling(const PageNodeImpl* page_node) {
  // Get the frame node for the main frame associated with this page.
  const FrameNodeImpl* main_frame_node = page_node->GetMainFrameNodeImpl();
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
  return main_frame_node->network_almost_idle() &&
         process_node->main_thread_task_load_is_low();
}

// static
PageLoadTrackerDecorator::Data*
PageLoadTrackerDecorator::Data::GetOrCreateForTesting(PageNodeImpl* page_node) {
  return DataImpl::GetOrCreate(page_node);
}

// static
PageLoadTrackerDecorator::Data* PageLoadTrackerDecorator::Data::GetForTesting(
    PageNodeImpl* page_node) {
  return DataImpl::Get(page_node);
}

// static
bool PageLoadTrackerDecorator::Data::DestroyForTesting(
    PageNodeImpl* page_node) {
  return DataImpl::Destroy(page_node);
}

void PageLoadTrackerDecorator::Data::SetLoadIdleState(
    PageNodeImpl* page_node,
    LoadIdleState load_idle_state) {
  load_idle_state_ = load_idle_state;

  switch (load_idle_state_) {
    case LoadIdleState::kLoadingNotStarted:
    case LoadIdleState::kLoadedAndIdle:
      page_node->SetIsLoading(false);
      break;
    case LoadIdleState::kLoading:
    case LoadIdleState::kLoadedNotIdling:
    case LoadIdleState::kLoadedAndIdling:
      page_node->SetIsLoading(true);
      break;
  }
}

}  // namespace performance_manager

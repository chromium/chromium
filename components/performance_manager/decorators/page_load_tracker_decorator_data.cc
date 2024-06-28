// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_load_tracker_decorator_data.h"

#include "components/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

namespace {

const char* ToString(PageLoadTrackerDecoratorData::LoadIdleState state) {
  using LoadIdleState = PageLoadTrackerDecoratorData::LoadIdleState;
  switch (state) {
    case LoadIdleState::kWaitingForNavigation:
      return "kWaitingForNavigation";
    case LoadIdleState::kWaitingForNavigationTimedOut:
      return "kWaitingForNavigationTimedOut";
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

}  // namespace

void PageLoadTrackerDecoratorData::SetLoadIdleState(
    PageNodeImpl* page_node,
    LoadIdleState load_idle_state) {
  // Check that this is a valid state transition.
  switch (load_idle_state_) {
    case LoadIdleState::kWaitingForNavigation:
      DCHECK(load_idle_state == LoadIdleState::kWaitingForNavigation ||
             load_idle_state == LoadIdleState::kLoading ||
             load_idle_state == LoadIdleState::kWaitingForNavigationTimedOut ||
             load_idle_state == LoadIdleState::kLoadedAndIdle)
          << "Transition from " << ToString(load_idle_state_) << " to "
          << ToString(load_idle_state);
      break;
    case LoadIdleState::kWaitingForNavigationTimedOut:
      DCHECK(load_idle_state == LoadIdleState::kLoading ||
             load_idle_state == LoadIdleState::kLoadedAndIdle)
          << "Transition from " << ToString(load_idle_state_) << " to "
          << ToString(load_idle_state);
      break;
    case LoadIdleState::kLoading:
      DCHECK(load_idle_state == LoadIdleState::kLoadedNotIdling ||
             load_idle_state == LoadIdleState::kLoadedAndIdling)
          << "Transition from " << ToString(load_idle_state_) << " to "
          << ToString(load_idle_state);
      break;
    case LoadIdleState::kLoadedNotIdling:
      DCHECK(load_idle_state == LoadIdleState::kLoadedAndIdling ||
             load_idle_state == LoadIdleState::kLoadedAndIdle ||
             load_idle_state == LoadIdleState::kWaitingForNavigation)
          << "Transition from " << ToString(load_idle_state_) << " to "
          << ToString(load_idle_state);
      break;
    case LoadIdleState::kLoadedAndIdling:
      DCHECK(load_idle_state == LoadIdleState::kLoadedNotIdling ||
             load_idle_state == LoadIdleState::kLoadedAndIdle ||
             load_idle_state == LoadIdleState::kWaitingForNavigation)
          << "Transition from " << ToString(load_idle_state_) << " to "
          << ToString(load_idle_state);
      break;
    case LoadIdleState::kLoadedAndIdle:
      DCHECK(load_idle_state == LoadIdleState::kWaitingForNavigation)
          << "Transition from " << ToString(load_idle_state_) << " to "
          << ToString(load_idle_state);
      break;
  }

  // Apply the state transition.
  load_idle_state_ = load_idle_state;

  switch (load_idle_state_) {
    case LoadIdleState::kWaitingForNavigation:
    case LoadIdleState::kLoading: {
      page_node->SetLoadingState(PageNode::LoadingState::kLoading);
      break;
    }
    case LoadIdleState::kWaitingForNavigationTimedOut: {
      page_node->SetLoadingState(PageNode::LoadingState::kLoadingTimedOut);
      break;
    }
    case LoadIdleState::kLoadedNotIdling:
    case LoadIdleState::kLoadedAndIdling: {
      page_node->SetLoadingState(PageNode::LoadingState::kLoadedBusy);
      break;
    }
    case LoadIdleState::kLoadedAndIdle: {
      page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
      break;
    }
  }
}

void PageLoadTrackerDecoratorData::Describe(base::Value::Dict* dict) {
  CHECK(dict);
  dict->Set("load_idle_state", ToString(load_idle_state()));
  dict->Set("is_loading", is_loading_);
  dict->Set("did_commit", did_commit_);
}

}  // namespace performance_manager
